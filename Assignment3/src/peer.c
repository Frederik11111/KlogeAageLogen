#include <stdlib.h>          
#include <stdio.h>           
#include <stdint.h>          
#include <math.h>            
#include <netdb.h>           
#include <sys/socket.h>      
#include <arpa/inet.h>       
#include <string.h>          
#include <time.h>            

#ifdef __APPLE__
#include "./endian.h"        
#else
#include <endian.h>          
#endif

#include "./peer.h"          

// Define Maximum Payload size (Max Message 8196 - Header 80 = 8116)
#define MAX_PAYLOAD (MAX_MSG_LEN - REPLY_HEADER_LEN)

// Global variables to be used by both the server and client side of the peer.
NetworkAddress_t *my_address;        // This peer's own network address
NetworkAddress_t **network = NULL;   // Dynamic array of pointers to known peers
uint32_t peer_count = 0;             // Current number of known peers
NetworkAddress_t get_random_peer();  // Get a random peer from the network


// Function declarations
void *handle_connection(void *arg);                                                    // Handle incoming connections (per client socket)
void handle_registration(int connfd, RequestHeader_t *header);                         // Handle REGISTER command from a peer
void handle_inform(RequestHeader_t *header, int connfd);                               // Handle INFORM command 
void handle_retrieve(int connfd, RequestHeader_t *header);                             // Handle RETRIEVE command

int peer_exists(char *ip, uint32_t port);                                              // Check if a peer is already in the network
void compute_saved_signature(hashdata_t incoming, char *salt, hashdata_t out);         // Compute saved signature from hash + salt

void send_inform(NetworkAddress_t *target, NetworkAddress_t *new_peer);                // Send INFORM command to a peer about a new peer
void send_message(NetworkAddress_t peer_address, int command, char *req, int req_len); // Send a request to a peer

void update_network_from_payload(const uint8_t *payload, uint32_t len);                // Update local network state from a payload of peers
int receive_reply(int fd, ReplyHeader_t *header_out, uint8_t **body_out);              // Receive reply from a peer on a socket

// Lock protecting access to the global 'network' array and 'peer_count'
pthread_mutex_t network_mutex = PTHREAD_MUTEX_INITIALIZER;

// Client thread function (runs interactively and talks to other peers)
void* client_thread()
{
    // Registration: ask user which peer to initially connect to for REGISTER
    char peer_ip[IP_LEN];                               // Buffer for peer IP input
    fprintf(stdout, "Enter peer IP to connect to: ");   // Prompt for peer IP
    scanf("%15s", peer_ip);                             // Read peer IP input

    // Pad the IP buffer with '\0' to ensure it's fully null-terminated
    for (int i = strlen(peer_ip); i < IP_LEN; i++)
        peer_ip[i] = '\0';                              // Null-terminate the IP string fully

    char peer_port[PORT_STR_LEN];                       // Buffer for peer port input
    fprintf(stdout, "Enter peer port to connect to: "); // Prompt for peer port
    scanf("%7s", peer_port);                            // Read peer port input 

    // Pad the port buffer with '\0' to ensure it's fully null-terminated
    for (int i = strlen(peer_port); i < PORT_STR_LEN; i++) 
        peer_port[i] = '\0';                            // Null-terminate the port string

    NetworkAddress_t peer_address;                      // Target peer we will REGISTER with
    memcpy(peer_address.ip, peer_ip, IP_LEN);           // Set peer IP field
    peer_address.port = atoi(peer_port);                // Convert port string to integer

    // Send REGISTER command to another peer to join the network
    send_message(peer_address, COMMAND_REGISTER, NULL, 0);
    printf("Registration complete\n");

    // Command loop: allow user to retrieve files or quit
    while (1) {
        char buffer[10];                               // Input buffer for single-character command
        printf("\nOptions: (r)etrieve file, (q)uit: "); 
        scanf("%9s", buffer);                         // Read user command

        if (buffer[0] == 'q') break;                  // Quit client loop

        if (buffer[0] == 'r') {                       // User chose to retrieve a file
            char filename[PATH_LEN];                  // Buffer for requested filename
            printf("Enter filename to fetch: ");
            scanf("%127s", filename);                 // Read filename

            // Get random peer (other than ourselves) to request the file from
            NetworkAddress_t target = get_random_peer();
            
            if (target.port == 0) {                   // If port 0, no suitable peer was found
                printf("There are no other connected to the network\n");
                continue;
            }

            printf("Requesting %s from %s:%u...\n", filename, target.ip, target.port);
            
            // Send RETRIEVE request containing the filename as body
            send_message(target, COMMAND_RETREIVE, filename, strlen(filename));
        }
    }
    printf("Client thread done\n");
    return NULL;                                      // Exit client thread
}


// Server thread function
void* server_thread()
{
    char port_str[PORT_STR_LEN];                                // Buffer for port string             
    snprintf(port_str, PORT_STR_LEN, "%d", my_address->port);   // Convert our port to string       

    int listenfd = compsys_helper_open_listenfd(port_str);      // Open listening socket on given port
    if (listenfd < 0) {
        fprintf(stderr, ">> Error opening listening socket on port %s\n", port_str);
        pthread_exit(NULL);                                     // Exit server thread on failure
    }

    // Main accept loop: accept each incoming connection and handle it in a new thread
    while (1) {
        int connfd = accept(listenfd, NULL, NULL);              // Accept incoming connection 
        if (connfd < 0) continue;                               // Ignore failed accept

        pthread_t tid;                                                              // Thread ID for handling connection          
        pthread_create(&tid, NULL, handle_connection, (void *)(intptr_t)connfd);    // Create handler thread for this connection
        pthread_detach(tid);                                                        // Detach thread so resources are cleaned on exit     
    }

    return NULL;                                                // Unreachable, but keeps compiler happy
}

// Handle RETRIEVE request: Send file in blocks (Task 2.5)
void handle_retrieve(int connfd, RequestHeader_t *header) {
    char filename[PATH_LEN];
    uint32_t req_len = ntohl(header->length); 
    int read_len = (req_len < PATH_LEN - 1) ? req_len : PATH_LEN - 1;
    
    if (compsys_helper_readn(connfd, filename, read_len) != read_len) return;
    filename[read_len] = '\0';

    if (filename[0] == '/') memmove(filename, filename + 1, strlen(filename));

    printf("[SERVER] Sending file: %s\n", filename);

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        ReplyHeader_t reply = {0};
        reply.status = htonl(STATUS_BAD_REQUEST);
        reply.block_count = htonl(1);
        compsys_helper_writen(connfd, &reply, sizeof(reply));
        return;
    }

    fseek(fp, 0L, SEEK_END);
    uint32_t file_size = ftell(fp);
    rewind(fp);

    hashdata_t total_hash;
    uint8_t *all_data = malloc(file_size);
    if (fread(all_data, 1, file_size, fp) != file_size) {
        free(all_data); fclose(fp); return;
    }
    get_data_sha((char*)all_data, total_hash, file_size, SHA256_HASH_SIZE);
    free(all_data);
    rewind(fp);

    uint32_t total_blocks = (file_size + MAX_PAYLOAD - 1) / MAX_PAYLOAD;
    if (total_blocks == 0) total_blocks = 1;

    uint8_t *buffer = malloc(MAX_PAYLOAD);
    
    // Allocate a "packet buffer" big enough for Header + Max Payload
    // This allows us to send everything in one atomic write
    size_t max_packet_size = sizeof(ReplyHeader_t) + MAX_PAYLOAD;
    uint8_t *packet_buffer = malloc(max_packet_size);

    for (uint32_t i = 0; i < total_blocks; i++) {
        memset(buffer, 0, MAX_PAYLOAD);

        uint32_t bytes_to_read = MAX_PAYLOAD;
        if (i == total_blocks - 1) {
            uint32_t remaining = file_size - (i * MAX_PAYLOAD);
            if (remaining > 0) bytes_to_read = remaining;
        }

        size_t actual_read = fread(buffer, 1, bytes_to_read, fp);
        uint32_t payload_len = (uint32_t)actual_read;

        ReplyHeader_t reply = {0};
        reply.length = htonl(payload_len);
        reply.status = htonl(STATUS_OK);
        reply.this_block = htonl(i);
        reply.block_count = htonl(total_blocks);
        
        memcpy(reply.total_hash, total_hash, SHA256_HASH_SIZE);
        get_data_sha((char*)buffer, reply.block_hash, payload_len, SHA256_HASH_SIZE);

        
        // Copy Header to start of packet
        memcpy(packet_buffer, &reply, sizeof(ReplyHeader_t));
        
        // Copy Body to directly after Header
        memcpy(packet_buffer + sizeof(ReplyHeader_t), buffer, payload_len);
        
        // Calculate total size of this specific packet
        size_t packet_len = sizeof(ReplyHeader_t) + payload_len;

        // Send it all at once
        compsys_helper_writen(connfd, packet_buffer, packet_len);
        
    }

    free(packet_buffer);
    free(buffer);
    fclose(fp);
    printf("[SERVER] Completed transfer of %s (%u blocks)\n", filename, total_blocks);
}

// Pick a random peer from the network
NetworkAddress_t get_random_peer() {
    NetworkAddress_t target_peer;                           
    memset(&target_peer, 0, sizeof(NetworkAddress_t));        

    pthread_mutex_lock(&network_mutex);                       // Lock network while reading
    if (peer_count > 1) {
        // Pick index between 0 and peer_count-1, but avoid my own entry
        int idx = rand() % peer_count;
        while (network[idx]->port == my_address->port) {      // Ensure we do not pick ourselves
            idx = rand() % peer_count;
        }

        memcpy(&target_peer, network[idx], sizeof(NetworkAddress_t));
    }
    pthread_mutex_unlock(&network_mutex);                     // Unlock network


    return target_peer;                                       // Return chosen peer 
}

// Handle incoming connection: read request header and dispatch to correct handler
void* handle_connection(void *arg)
{
    int connfd = (intptr_t)arg;                       
    RequestHeader_t header;                      // Request header structure        
    printf("[DEBUG] Accepted new connection (fd: %d)\n", connfd); 

    // Read request header from connection
    if (compsys_helper_readn(connfd, &header, sizeof(header)) <= 0) {
        printf("[DEBUG] Failed to read header or connection closed\n"); 
        close(connfd);                           // If read fails, close connection
        return NULL;
    }

    uint32_t command = ntohl(header.command);    // Convert command field to host byte order      
    uint32_t length  = ntohl(header.length);     // Convert length field to host byte order 

    printf("[DEBUG] Header Read -> Cmd: %u, Len: %u\n", command, length);

    // Dispatch based on command type
    if (command == COMMAND_REGISTER)
        handle_registration(connfd, &header);    // Handle REGISTER command  

    else if (command == COMMAND_INFORM)
        handle_inform(&header, connfd);          // Handle INFORM command     

    else if (command == COMMAND_RETREIVE)        // Handle RETRIEVE command
        handle_retrieve(connfd, &header);
    else {
        printf("[DEBUG] Unknown command received: %u\n", command);
    }

    close(connfd);                               // Close this client connection when done
    return NULL;
}

// Check if a peer (ip, port) exists in the current network array
int peer_exists(char *ip, uint32_t port)
{
    int exists = 0;                               // Flag: 0 = not found, 1 = found

    pthread_mutex_lock(&network_mutex);          // Lock network while searching
    for (uint32_t i = 0; i < peer_count; i++) { 
        if (strcmp(network[i]->ip, ip) == 0 &&   // Compare IP strings
            network[i]->port == port) {          // Compare port
            exists = 1;
            break;
        }
    }
    pthread_mutex_unlock(&network_mutex);        // Unlock network

    return exists;                               // Return whether peer exists
}

// Compute saved signature
void compute_saved_signature(hashdata_t incoming, char *salt, hashdata_t out)
{
    uint8_t buf[SHA256_HASH_SIZE + SALT_LEN];       // Buffer for concatenated hash and salt
    memcpy(buf, incoming, SHA256_HASH_SIZE);        // Copy incoming hash to buffer
    memcpy(buf + SHA256_HASH_SIZE, salt, SALT_LEN); // Append salt after hash

    // Compute SHA256 over (incoming_hash || salt) into 'out'
    get_data_sha((char *)buf, out, SHA256_HASH_SIZE + SALT_LEN, SHA256_HASH_SIZE);
}


// Send INFORM command to a peer (target) about a new peer (new_peer)
void send_inform(NetworkAddress_t *target, NetworkAddress_t *new_peer)
{
    char port_str[PORT_STR_LEN];                                // Buffer for port string             
    snprintf(port_str, PORT_STR_LEN, "%u", target->port);       // Convert target port to string

    int connfd = compsys_helper_open_clientfd(target->ip, port_str); // Open connection to target peer
    if (connfd < 0) {
        fprintf(stderr, ">> Failed to connect for INFORM: %s:%u\n", target->ip, target->port);
        return;
    }

    RequestHeader_t header;                                     // Request header structure         
    memset(&header, 0, sizeof(header));                         // Initialize header to zero

    memcpy(header.ip, my_address->ip, IP_LEN);                  // Set sender IP (this peer)
    header.port = htonl(my_address->port);                      // Set sender port
    memcpy(header.signature, my_address->signature, SHA256_HASH_SIZE); // Set sender signature
    header.command = htonl(COMMAND_INFORM);                     // Set command to INFORM
    header.length = htonl(PEER_ADDR_LEN);                       // Payload will be one peer description

    uint8_t body[PEER_ADDR_LEN];                                // Body buffer for new peer information
    int offset = 0;                                             // Offset into body buffer

    memcpy(body + offset, new_peer->ip, IP_LEN);                // Copy new peer IP to body buffer
    offset += IP_LEN;

    uint32_t p = htonl(new_peer->port);                         // Convert new peer port to network byte order
    memcpy(body + offset, &p, 4);                               // Copy new peer port to body buffer
    offset += 4;

    memcpy(body + offset, new_peer->signature, SHA256_HASH_SIZE); // Copy new peer signature to body buffer
    offset += SHA256_HASH_SIZE;

    memcpy(body + offset, new_peer->salt, SALT_LEN);            // Copy new peer salt to body buffer

    compsys_helper_writen(connfd, &header, sizeof(header));     // Send request header
    compsys_helper_writen(connfd, body, PEER_ADDR_LEN);         // Send peer info payload

    close(connfd);                                              // Close connection to target
}


// Handle INFORM command: add a new peer to the network if not already known
void handle_inform(RequestHeader_t *header, int connfd)
{
    uint8_t buffer[PEER_ADDR_LEN];                              // Buffer for incoming peer information
    if (compsys_helper_readn(connfd, buffer, PEER_ADDR_LEN) != PEER_ADDR_LEN) {
        return;                                                 // If payload read fails, abort
    }   // Read peer information into buffer 

    printf("[SERVER] Received INFORM\n");
    
    size_t offset = 0;                                          // Offset for parsing buffer

    char ip[IP_LEN];                                            // Temporary storage for peer IP
    memcpy(ip, buffer + offset, IP_LEN);                        // Extract IP from payload
    ip[IP_LEN - 1] = '\0';                                      // Ensure IP string is null-terminated
    offset += IP_LEN;

    uint32_t port_net;                                          // Port in network byte order
    memcpy(&port_net, buffer + offset, 4);                      // Extract port from payload
    uint32_t port = ntohl(port_net);                            // Convert port to host order
    offset += 4;

    
    uint8_t signature[SHA256_HASH_SIZE];                        // Buffer for peer signature
    memcpy(signature, buffer + offset, SHA256_HASH_SIZE);       // Extract signature
    offset += SHA256_HASH_SIZE;

    uint8_t salt[SALT_LEN];                                     // Buffer for peer salt
    memcpy(salt, buffer + offset, SALT_LEN);                    // Extract salt
    offset += SALT_LEN;

    // If peer is already known, ignore this INFORM
    if (peer_exists(ip, port)) {
        printf("[SERVER] INFORM: peer already known\n");
        return;
    }

    // Allocate and populate new peer entry
    NetworkAddress_t *na = malloc(sizeof(NetworkAddress_t));    // Allocate memory for new peer
    memcpy(na->ip, ip, IP_LEN);                                 // Set peer IP
    na->port = port;                                            // Set peer port
    memcpy(na->signature, signature, SHA256_HASH_SIZE);         // Set peer signature
    memcpy(na->salt, salt, SALT_LEN);                           // Set peer salt

    // Add new peer to global network array
    pthread_mutex_lock(&network_mutex);
    network = realloc(network, sizeof(NetworkAddress_t*) * (peer_count + 1)); // Grow network array
    network[peer_count] = na;                                                 // Append new peer
    peer_count++;                                                             // Increment peer count
    pthread_mutex_unlock(&network_mutex);


    printf("[SERVER] INFORM added peer %s:%u\n", ip, port);
}


// Handle REGISTER command from a peer (possibly ourselves)
void handle_registration(int connfd, RequestHeader_t *header)
{
    printf("[SERVER] Received REGISTER from %s:%u\n", 
           header->ip, ntohl(header->port));             // Log registration request

    // Self-registration: if the registering peer is actually us, reply OK but don't add again
    if (strcmp(header->ip, my_address->ip) == 0 &&
        ntohl(header->port) == my_address->port)
    {
        printf("[SERVER] Ignoring self-registration.\n");
        
        ReplyHeader_t reply;                               // Reply header structure          
        reply.length = htonl(0);                          // Length 0 (no payload)   
        reply.status = htonl(STATUS_OK);                  // Status OK    
        reply.this_block = htonl(0);                      // Single block, index 0
        reply.block_count = htonl(1);                     // Block count 1
        memset(reply.block_hash, 0, SHA256_HASH_SIZE);    // block_hash not used
        memset(reply.total_hash, 0, SHA256_HASH_SIZE);    // total_hash not used

        compsys_helper_writen(connfd, &reply, sizeof(reply));   // Send reply
        return;
    }

    char *ip = header->ip;                          // Extract IP from header
    uint32_t port = ntohl(header->port);            // Extract port from header
 
    // If peer already exists, send a special status and return
    if (peer_exists(ip, port)) {
        ReplyHeader_t reply;
        reply.length = htonl(0);                    // No payload
        reply.status = htonl(STATUS_PEER_EXISTS);   // Indicate peer already exists
        reply.this_block = htonl(0);
        reply.block_count = htonl(1);
        memset(reply.block_hash, 0, SHA256_HASH_SIZE);
        memset(reply.total_hash, 0, SHA256_HASH_SIZE);

        compsys_helper_writen(connfd, &reply, REPLY_HEADER_LEN); // Send reply with header only
        return;
    }

    // Create new peer entry from header
    NetworkAddress_t *new_peer = malloc(sizeof(NetworkAddress_t));  // Allocate memory for new peer
    memcpy(new_peer->ip, ip, IP_LEN);                               // Copy peer IP         
    new_peer->ip[IP_LEN - 1] = '\0';                                // Ensure null-termination
    new_peer->port = port;                                          // Set peer port

    generate_random_salt(new_peer->salt);                           // Generate random salt for peer
    // Compute saved signature from incoming signature + salt
    compute_saved_signature(header->signature, new_peer->salt, new_peer->signature);    
    
    // Add new peer to global network list
    pthread_mutex_lock(&network_mutex);
    network = realloc(network, sizeof(NetworkAddress_t *) * (peer_count + 1));  // Grow network array
    network[peer_count] = new_peer;                                             // Store new peer
    peer_count++;                                                               // Increment peer count        

    printf("[SERVER] Added peer %s:%u. Total peers: %u\n",
           new_peer->ip, new_peer->port, peer_count);

    // Inform all existing peers (except the new one) about this new peer
    for (uint32_t i = 0; i < peer_count - 1; i++) {
        printf("[SERVER] Sending INFORM to %s:%u\n",
               network[i]->ip, network[i]->port);              
        send_inform(network[i], new_peer);                      
    }


    // Build payload with entire updated peer list for replying to the registering peer
    int payload_len = peer_count * PEER_ADDR_LEN;            // Total bytes for all peers
    uint8_t *payload = malloc(payload_len);                  // Allocate payload buffer  

    int offset = 0;
    for (uint32_t i = 0; i < peer_count; i++) {
        memcpy(payload + offset, network[i]->ip, IP_LEN);    // Copy peer IP to payload
        offset += IP_LEN;                                    // Update offset

        uint32_t p = htonl(network[i]->port);                // Convert peer port to network byte order
        memcpy(payload + offset, &p, 4);                     // Copy peer port to payload
        offset += 4;                                         // Update offset        

        memcpy(payload + offset, network[i]->signature, SHA256_HASH_SIZE); // Copy peer signature to payload
        offset += SHA256_HASH_SIZE;                          // Update offset           

        memcpy(payload + offset, network[i]->salt, SALT_LEN); // Copy peer salt to payload        
        offset += SALT_LEN;                                  // Update offset       
    }
    pthread_mutex_unlock(&network_mutex);


    ReplyHeader_t reply;                                     // Reply header structure
    reply.length = htonl(payload_len);                       // Length of payload
    reply.status = htonl(STATUS_OK);                         // Status OK
    reply.this_block = htonl(0);                             // Single block
    reply.block_count = htonl(1);                            // Block count = 1

    // Compute hash over payload and fill in header
    get_data_sha((char *)payload, reply.block_hash, payload_len, SHA256_HASH_SIZE); // block_hash = H
    memcpy(reply.total_hash, reply.block_hash, SHA256_HASH_SIZE);                   // total_hash = block_hash     

    // Send reply header + payload (full peer list)
    compsys_helper_writen(connfd, &reply, sizeof(reply));   // Send reply header        
    compsys_helper_writen(connfd, payload, payload_len);    // Send payload data

    free(payload);                                          // Free temporary payload buffer
}


// Get signature from password and salt: H(password || salt)
void get_signature(char *password, int password_len, char *salt, hashdata_t *hash)
{
    uint8_t buf[password_len + SALT_LEN];            // Buffer for concatenated password and salt

    memcpy(buf, password, password_len);             // Copy password into buffer      
    memcpy(buf + password_len, salt, SALT_LEN);      // Append salt after password  

    // Compute SHA256 hash over (password || salt) and store in *hash
    get_data_sha((char *)buf, *hash, password_len + SALT_LEN, SHA256_HASH_SIZE);
}


// Send a message to a peer: build header, send body if any, then wait for reply
void send_message(NetworkAddress_t peer_address, int command,
                  char *body, int body_len)
{
    char port_str[PORT_STR_LEN];                            // Buffer for port string
    snprintf(port_str, PORT_STR_LEN, "%u", peer_address.port); // Convert peer port to string   

    int clientfd = compsys_helper_open_clientfd(peer_address.ip, port_str); // Open connection to peer      
    if (clientfd < 0) {
        fprintf(stderr, "Failed to connect to %s:%u\n",
                peer_address.ip, peer_address.port);
        return;
    }

    RequestHeader_t header;                                     // Request header structure       
    memset(&header, 0, sizeof(header));                         // Initialize header to zero

    memcpy(header.ip, my_address->ip, IP_LEN);                  // Set sender IP to own IP     
    header.port = htonl(my_address->port);                      // Set sender port (network order)    
    memcpy(header.signature, my_address->signature, SHA256_HASH_SIZE);  // Set sender signature     

    header.command = htonl(command);                            // Set command field       
    header.length = htonl(body_len);                            // Set length of body   

    // Send header first
    compsys_helper_writen(clientfd, &header, REQUEST_HEADER_LEN);

    // Send body if present and length > 0
    if (body_len > 0 && body != NULL)
        compsys_helper_writen(clientfd, body, body_len);

    ReplyHeader_t reply;                        // Reply header structure from peer
    uint8_t *reply_body = NULL;                 // Pointer for reply body (if any)

    // Block waiting for reply header + body
    if (receive_reply(clientfd, &reply, &reply_body) == 0) {
        
        // Case 1: Register Response
        if (command == COMMAND_REGISTER && reply.status == STATUS_OK) {
            // Reply body contains updated network list
            update_network_from_payload(reply_body, reply.length);
        }
        // Case 2: Retrieve Response (Task 2.4/2.5 Multi-block)
     else if (command == COMMAND_RETREIVE && reply.status == STATUS_OK) {
        
        FILE *fp = fopen(body, "wb"); // Open file once
        if (!fp) {
            printf("Error opening file for writing.\n");
            free(reply_body);
            close(clientfd);
            return;
        }

        // We have the first block already
        // We need to loop until we have all blocks.
        
        uint32_t total_blocks = reply.block_count; // How many to expect
        uint32_t blocks_received = 0;

        while (blocks_received < total_blocks) {
            
            // Fix for out-of-order packets (Task 2.5)
            // Calculate the file offset: block_number * MAX_PAYLOAD
            long offset = (long)reply.this_block * MAX_PAYLOAD;
            
            // Seek to the correct position in the file
            fseek(fp, offset, SEEK_SET);

            printf("Received block %u/%u (%u bytes)\n", 
                   reply.this_block + 1, total_blocks, reply.length);

            // Write to file at the correct position
            fwrite(reply_body, 1, reply.length, fp);
            free(reply_body); // Free the body we just used
            reply_body = NULL; // Safety

            blocks_received++;

            // 3. If we still expect more blocks, read the next reply
            if (blocks_received < total_blocks) {
                if (receive_reply(clientfd, &reply, &reply_body) != 0) {
                    printf("Error: Connection closed before all blocks received.\n");
                    break;
                }
                // Loop continues with new reply/body
            }
        }
        
        fclose(fp);
        printf("File '%s' transfer complete.\n", body);
    }
        else {
            // For other commands or non-OK status, just log basic info
            printf("Got reply: status=%u length=%u\n", reply.status, reply.length);
        }
    }

    free(reply_body);                      // Free reply body buffer if allocated
    close(clientfd);                       // Close connection to peer
}


// Update network from payload containing one or more peer entries
void update_network_from_payload(const uint8_t *payload, uint32_t len)
{
    // Ensure payload length is a multiple of PEER_ADDR_LEN
    if (len % PEER_ADDR_LEN != 0) {
        fprintf(stderr, "Bad network payload length\n");
        return;
    }

    uint32_t count = len / PEER_ADDR_LEN;                  // Number of peers encoded in payload
    
    pthread_mutex_lock(&network_mutex);
    network = realloc(network, count * sizeof(NetworkAddress_t *)); // Resize network array to exactly 'count' peers
    peer_count = count;                                              // Update peer count    

    size_t offset = 0;                                              // Offset for parsing payload
    for (uint32_t i = 0; i < count; i++) {
        NetworkAddress_t *na = malloc(sizeof(NetworkAddress_t));    // Allocate memory for new peer

        memcpy(na->ip, payload + offset, IP_LEN);                   // Copy peer IP from payload    
        offset += IP_LEN;                                           

        uint32_t port_net = 0;                                      // Peer port in network byte order  
        memcpy(&port_net, payload + offset, 4);                     // Copy peer port from payload  
        na->port = ntohl(port_net);                                 // Convert peer port to host byte order    
        offset += 4;

        memcpy(na->signature, payload + offset, SHA256_HASH_SIZE);  // Copy peer signature from payload
        offset += SHA256_HASH_SIZE;

        memcpy(na->salt, payload + offset, SALT_LEN);               // Copy peer salt from payload
        offset += SALT_LEN;

        network[i] = na;                                           // Store peer in network array
    } 
    pthread_mutex_unlock(&network_mutex);


    // Print resulting network for debugging
    printf("Network now has %u peers:\n", peer_count);
    for (uint32_t i = 0; i < peer_count; i++)
        printf("  %s:%u\n", network[i]->ip, network[i]->port);
}


// Receive reply from a peer: read header, then body if any
int receive_reply(int fd, ReplyHeader_t *header_out, uint8_t **body_out)
{
    ReplyHeader_t header;                                    // Temporary header buffer

    // Read fixed-size reply header
    ssize_t n = compsys_helper_readn(fd, &header, REPLY_HEADER_LEN);    
    if (n != REPLY_HEADER_LEN)
        return -1;                                           // Error reading header

    // Convert all multi-byte fields to host byte order
    header.length = ntohl(header.length);                    // Convert length
    header.status = ntohl(header.status);                    // Convert status        
    header.this_block = ntohl(header.this_block);            // Convert this_block
    header.block_count = ntohl(header.block_count);          // Convert block_count   

    *header_out = header;                                    // Pass header back to caller

    uint8_t *body = NULL;                                    // Pointer for reply body
    if (header.length > 0) {                                 // If there is a body
        body = malloc(header.length);                        // Allocate buffer
        n = compsys_helper_readn(fd, body, header.length);   // Read reply body
        if (n != header.length) {                            // If body read fails
            free(body);                                      // Free allocated memory
            return -1;
        }
    }

    *body_out = body;                                        // Set body pointer output
    return 0;                                                // Success
}


int main(int argc, char **argv)
{
    // Expect exactly <program> <IP> <PORT>
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);   // Print usage on error
        exit(1);
    }

    my_address = malloc(sizeof(NetworkAddress_t));            // Allocate memory for own address
    memset(my_address->ip, 0, IP_LEN);                        // Initialize IP field to zeroes
    memcpy(my_address->ip, argv[1], strlen(argv[1]));         // Set IP from command line argument
    my_address->port = atoi(argv[2]);                         // Set port from command line argument   

    // Validate IP address format
    if (!is_valid_ip(my_address->ip)) { 
        fprintf(stderr, "Invalid IP\n");    
        exit(1);    
    }
    // Validate port number
    if (!is_valid_port(my_address->port)) {
        fprintf(stderr, "Invalid port\n");
        exit(1);
    }

    // Ask user for a password used to derive own signature
    char password[PASSWORD_LEN];                              // Buffer for password
    printf("Create a password to proceed: ");
    scanf("%15s", password);                                  // Read password

    // Pad rest of password buffer with '\0'
    for (int i = strlen(password); i < PASSWORD_LEN; i++)
        password[i] = '\0';

    char salt[SALT_LEN + 1] = "0123456789ABCDEF";             // Example fixed salt for simplicity
    memcpy(my_address->salt, salt, SALT_LEN);                 // Set salt in my_address

    // Generate signature for this peer from password + salt
    get_signature(password, PASSWORD_LEN, my_address->salt, 
                  &my_address->signature);

    // Initialize network with only ourselves as the first entry
    network = malloc(sizeof(NetworkAddress_t *));             // Allocate array for 1 pointer
    network[0] = my_address;                                  // Store own address as first peer
    peer_count = 1;                                           // Only one peer known (self)

    pthread_t client_thread_id;                               // Thread ID for client thread
    pthread_t server_thread_id;                               // Thread ID for server thread

    // Create both client and server threads
    pthread_create(&client_thread_id, NULL, client_thread, NULL); // Create interactive client thread
    pthread_create(&server_thread_id, NULL, server_thread, NULL); // Create server thread for incoming connections

    pthread_detach(client_thread_id);                         // Detach client thread
    pthread_join(server_thread_id, NULL);                     // Block until server thread finishes

    return 0;                                              
}