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



// Global variables to be used by both the server and client side of the peer.
NetworkAddress_t *my_address;        // This peer's network address
NetworkAddress_t **network = NULL;   // List of known peers
uint32_t peer_count = 0;             // Number of known peers


// Function declarations
void *handle_connection(void *arg);                                                    // Handle incoming connections
void handle_registration(int connfd, RequestHeader_t *header);                         // Handle REGISTER command
void handle_inform(RequestHeader_t *header, int connfd);                               // Handle INFORM command

int peer_exists(char *ip, uint32_t port);                                              // Check if a peer exists in the network
void compute_saved_signature(hashdata_t incoming, char *salt, hashdata_t out);         // Compute saved signature

void send_inform(NetworkAddress_t *target, NetworkAddress_t *new_peer);                // Send INFORM command to a peer
void send_message(NetworkAddress_t peer_address, int command, char *req, int req_len); // Send a message to a peer

void update_network_from_payload(const uint8_t *payload, uint32_t len);                 // Update network from payload
int receive_reply(int fd, ReplyHeader_t *header_out, uint8_t **body_out);               // Receive reply from a peer




// Client thread function
void* client_thread()
{
    char peer_ip[IP_LEN];                               // Buffer for peer IP input
    fprintf(stdout, "Enter peer IP to connect to: ");   // Prompt for peer IP
    scanf("%15s", peer_ip);                             // Read peer IP input               

    for (int i = strlen(peer_ip); i < IP_LEN; i++)
        peer_ip[i] = '\0';                             // Null-terminate the IP string          

    char peer_port[PORT_STR_LEN];                      // Buffer for peer port input        
    fprintf(stdout, "Enter peer port to connect to: "); // Prompt for peer port 
    scanf("%7s", peer_port);                           // Read peer port input                

    for (int i = strlen(peer_port); i < PORT_STR_LEN; i++) 
        peer_port[i] = '\0';                            // Null-terminate the port string      

    NetworkAddress_t peer_address;                      // Create NetworkAddress_t for the peer
    memcpy(peer_address.ip, peer_ip, IP_LEN);           // Set peer IP
    peer_address.port = atoi(peer_port);                // Set peer port          

    // Send REGISTER command to another peer
    send_message(peer_address, COMMAND_REGISTER, NULL, 0);

    printf("Client thread done\n");
    return NULL;
}


// Server thread function
void* server_thread()
{
    char port_str[PORT_STR_LEN];                                // Buffer for port string             
    snprintf(port_str, PORT_STR_LEN, "%d", my_address->port);   // Convert port to string       

    int listenfd = compsys_helper_open_listenfd(port_str);      // Open listening socket            
    if (listenfd < 0) {
        fprintf(stderr, ">> Error opening listening socket on port %s\n", port_str);
        pthread_exit(NULL);
    }

    while (1) {
        int connfd = accept(listenfd, NULL, NULL);              // Accept incoming connection
        if (connfd < 0) continue;

        pthread_t tid;                                                              // Thread ID for handling connection          
        pthread_create(&tid, NULL, handle_connection, (void *)(intptr_t)connfd);    // Create thread to handle connection
        pthread_detach(tid);                                                        // Detach thread to free resources upon completion     
    }

    return NULL;
}

// Handle incoming connection
void* handle_connection(void *arg)
{
    int connfd = (intptr_t)arg;                  // Connection file descriptor      
    RequestHeader_t header;                      // Request header structure        

    if (compsys_helper_readn(connfd, &header, sizeof(header)) <= 0) {
        close(connfd);
        return NULL;
    }

    uint32_t command = ntohl(header.command);        // Convert command to host byte order      
    uint32_t length  = ntohl(header.length); //Convert lenght to host byte order


    if (command == COMMAND_REGISTER)
        handle_registration(connfd, &header);       // Handle REGISTER command  

    else if (command == COMMAND_INFORM)
        handle_inform(&header, connfd);                // Handle INFORM command     

    close(connfd);
    return NULL;
}

// Check if a peer exists in the network
int peer_exists(char *ip, uint32_t port)
{
    for (uint32_t i = 0; i < peer_count; i++) {
        if (strcmp(network[i]->ip, ip) == 0 && network[i]->port == port)   // Compare IP and port
            return 1;
    }
    return 0;
}

// Compute saved signature
void compute_saved_signature(hashdata_t incoming, char *salt, hashdata_t out)
{
    uint8_t buf[SHA256_HASH_SIZE + SALT_LEN];   // Buffer for concatenated hash and salt
    memcpy(buf, incoming, SHA256_HASH_SIZE);    // Copy incoming hash to buffer
    memcpy(buf + SHA256_HASH_SIZE, salt, SALT_LEN); // Copy salt to buffer

    get_data_sha((char *)buf, out, SHA256_HASH_SIZE + SALT_LEN, SHA256_HASH_SIZE); // Compute SHA256 hash of concatenated buffer
}


// Send INFORM command to a peer
void send_inform(NetworkAddress_t *target, NetworkAddress_t *new_peer)
{
    char port_str[PORT_STR_LEN];                                // Buffer for port string             
    snprintf(port_str, PORT_STR_LEN, "%u", target->port);      // Convert port to string

    int connfd = compsys_helper_open_clientfd(target->ip, port_str); // Open connection to target peer
    if (connfd < 0) {
        fprintf(stderr, ">> Failed to connect for INFORM: %s:%u\n", target->ip, target->port);
        return;
    }

    RequestHeader_t header;                     // Request header structure         
    memset(&header, 0, sizeof(header));         // Initialize header to zero

    memcpy(header.ip, my_address->ip, IP_LEN);  // Set sender IP
    header.port = htonl(my_address->port);      // Set sender port
    memcpy(header.signature, my_address->signature, SHA256_HASH_SIZE); // Set sender signature
    header.command = htonl(COMMAND_INFORM);     // Set command to INFORM
    header.length = htonl(PEER_ADDR_LEN);       // Set length of payload

    uint8_t body[PEER_ADDR_LEN];               // Body buffer for new peer information
    int offset = 0;                            // Offset for body buffer

    memcpy(body + offset, new_peer->ip, IP_LEN); // Copy new peer IP to body buffer
    offset += IP_LEN;

    uint32_t p = htonl(new_peer->port);      // Convert new peer port to network byte order
    memcpy(body + offset, &p, 4);                  // Copy new peer port to body buffer
    offset += 4;

    memcpy(body + offset, new_peer->signature, SHA256_HASH_SIZE); // Copy new peer signature to body buffer
    offset += SHA256_HASH_SIZE;

    memcpy(body + offset, new_peer->salt, SALT_LEN); // Copy new peer salt to body buffer

    compsys_helper_writen(connfd, &header, sizeof(header));  // Send header
    compsys_helper_writen(connfd, body, PEER_ADDR_LEN);     // Send body

    close(connfd);
}


// Handle INFORM command
void handle_inform(RequestHeader_t *header, int connfd)
{
    uint8_t buffer[PEER_ADDR_LEN];               // Buffer for incoming peer information
    if (compsys_helper_readn(connfd, buffer, PEER_ADDR_LEN) != PEER_ADDR_LEN) {
        return; }   // Read peer information into buffer 

    printf("[SERVER] Received INFORM\n");
    
    size_t offset = 0;
    char ip[IP_LEN];
    memcpy(ip, buffer + offset, IP_LEN);
    ip[IP_LEN - 1] = '\0';
    offset += IP_LEN;

    uint32_t port_net;
    memcpy(&port_net, buffer + offset, 4);
    uint32_t port = ntohl(port_net);
    offset += 4;

}


// Get signature from password and salt
void handle_registration(int connfd, RequestHeader_t *header)
{
    printf("[SERVER] Received REGISTER from %s:%u\n", 
           header->ip, ntohl(header->port));             // Log registration request

    // self-registration must reply gracefully
    if (strcmp(header->ip, my_address->ip) == 0 &&
        ntohl(header->port) == my_address->port)
    {
        printf("[SERVER] Ignoring self-registration.\n");
        
        ReplyHeader_t reply;                               // Reply header structure          
        reply.length = htonl(0);                          // Set length to 0   
        reply.status = htonl(STATUS_OK);                  // Set status to OK    
        reply.this_block = htonl(0);                     // Set this_block to 0     
        reply.block_count = htonl(1);                    // Set block_count to 1   
        memset(reply.block_hash, 0, SHA256_HASH_SIZE);  // Set block_hash to 0  
        memset(reply.total_hash, 0, SHA256_HASH_SIZE);  // Set total_hash to 0

        compsys_helper_writen(connfd, &reply, sizeof(reply));   // Send reply
        return;
    }

    char *ip = header->ip;                          // Extract IP from header
    uint32_t port = ntohl(header->port);          // Extract port from header
 
    if (peer_exists(ip, port)) {
        ReplyHeader_t reply;
        reply.length = htonl(0);
        reply.status = htonl(STATUS_PEER_EXISTS);
        reply.this_block = htonl(0);
        reply.block_count = htonl(1);
        memset(reply.block_hash, 0, SHA256_HASH_SIZE);
        memset(reply.total_hash, 0, SHA256_HASH_SIZE);

        compsys_helper_writen(connfd, &reply, REPLY_HEADER_LEN);
        return;
    }

    NetworkAddress_t *new_peer = malloc(sizeof(NetworkAddress_t));  // Allocate memory for new peer
    memcpy(new_peer->ip, ip, IP_LEN);                                  // Set new peer IP         
    new_peer->ip[IP_LEN - 1] = '\0';                           // Ensure null-termination
    new_peer->port = port;                                    // Set new peer port

    generate_random_salt(new_peer->salt);                      // Generate random salt for new peer
    compute_saved_signature(header->signature, new_peer->salt, new_peer->signature);    // Compute signature for new peer

    network = realloc(network, sizeof(NetworkAddress_t *) * (peer_count + 1));  // Reallocate network array to accommodate new peer
    network[peer_count] = new_peer;                                             // Add new peer to network
    peer_count++;                                                               // Increment peer count        

    printf("[SERVER] Added peer %s:%u. Total peers: %u\n",
           new_peer->ip, new_peer->port, peer_count);

    for (uint32_t i = 0; i < peer_count - 1; i++) {
        printf("[SERVER] Sending INFORM to %s:%u\n",
               network[i]->ip, network[i]->port);              
        send_inform(network[i], new_peer);                      
    }

    int payload_len = peer_count * PEER_ADDR_LEN;            // Calculate payload length
    uint8_t *payload = malloc(payload_len);                 // Allocate memory for payload  

    int offset = 0;
    for (uint32_t i = 0; i < peer_count; i++) {
        memcpy(payload + offset, network[i]->ip, IP_LEN);   // Copy peer IP to payload
        offset += IP_LEN;                                   // Update offset

        uint32_t p = htonl(network[i]->port);               // Convert peer port to network byte order
        memcpy(payload + offset, &p, 4);                    // Copy peer port to payload
        offset += 4;                                        // Update offset        

        memcpy(payload + offset, network[i]->signature, SHA256_HASH_SIZE); // Copy peer signature to payload
        offset += SHA256_HASH_SIZE;                          // Update offset           

        memcpy(payload + offset, network[i]->salt, SALT_LEN);   // Copy peer salt to payload        
        offset += SALT_LEN;                                  // Update offset       
    }

    ReplyHeader_t reply;                                // Reply header structure
    reply.length = htonl(payload_len);                  // Set length of payload
    reply.status = htonl(STATUS_OK);                    // Set status to OK
    reply.this_block = htonl(0);                        // Set this_block to 0   
    reply.block_count = htonl(1);                       // Set block_count to 1   

    get_data_sha((char *)payload, reply.block_hash, payload_len, SHA256_HASH_SIZE); // Compute block_hash from payload
    memcpy(reply.total_hash, reply.block_hash, SHA256_HASH_SIZE);                   // Set total_hash to block_hash     

    compsys_helper_writen(connfd, &reply, sizeof(reply));   // Send reply header        
    compsys_helper_writen(connfd, payload, payload_len);    // Send payload

    free(payload);
}


// Get signature from password and salt
void get_signature(char *password, int password_len, char *salt, hashdata_t *hash)
{
    uint8_t buf[password_len + SALT_LEN];   // Buffer for concatenated password and salt

    memcpy(buf, password, password_len);    // Copy password to buffer      
    memcpy(buf + password_len, salt, SALT_LEN);     // Copy salt to buffer  

    get_data_sha((char *)buf, *hash, password_len + SALT_LEN, SHA256_HASH_SIZE);    // Compute SHA256 hash of concatenated buffer
}


// Send a message to a peer
void send_message(NetworkAddress_t peer_address, int command,
                  char *body, int body_len)
{
    char port_str[PORT_STR_LEN];                            // Buffer for port string
    snprintf(port_str, PORT_STR_LEN, "%u", peer_address.port);      // Convert port to string   

    int clientfd = compsys_helper_open_clientfd(peer_address.ip, port_str);     // Open connection to peer      
    if (clientfd < 0) {
        fprintf(stderr, "Failed to connect to %s:%u\n",
                peer_address.ip, peer_address.port);
        return;
    }

    RequestHeader_t header;                                     // Request header structure       
    memset(&header, 0, sizeof(header));                         // Initialize header to zero

    memcpy(header.ip, my_address->ip, IP_LEN);                 // Set sender IP     
    header.port = htonl(my_address->port);                        // Set sender port    
    memcpy(header.signature, my_address->signature, SHA256_HASH_SIZE);  // Set sender signature     

    header.command = htonl(command);                       // Set command       
    header.length = htonl(body_len);                        // Set length of body   

    // send header
    compsys_helper_writen(clientfd, &header, REQUEST_HEADER_LEN);

    // send body
    if (body_len > 0 && body != NULL)
        compsys_helper_writen(clientfd, body, body_len);

    ReplyHeader_t reply;                        // Reply header structure
    uint8_t *reply_body = NULL;                 // Pointer for reply body

    if (receive_reply(clientfd, &reply, &reply_body) == 0) {
        if (command == COMMAND_REGISTER && reply.status == STATUS_OK && reply.length > 0) // On successful REGISTER reply
        update_network_from_payload(reply_body, reply.length); // Update network with received payload
        printf("Got reply: status=%u length=%u\n",
               reply.status, reply.length);
    }

    free(reply_body);
    close(clientfd);
}


// Update network from payload
void update_network_from_payload(const uint8_t *payload, uint32_t len)
{
    if (len % PEER_ADDR_LEN != 0) {
        fprintf(stderr, "Bad network payload length\n");
        return;
    }

    uint32_t count = len / PEER_ADDR_LEN;   // Calculate number of peers in payload
 
    network = realloc(network, count * sizeof(NetworkAddress_t *)); // Reallocate network array to accommodate new peers
    peer_count = count;                                              // Update peer count    

    size_t offset = 0;                                              // Offset for parsing payload
    for (uint32_t i = 0; i < count; i++) {
        NetworkAddress_t *na = malloc(sizeof(NetworkAddress_t));    // Allocate memory for new peer

        memcpy(na->ip, payload + offset, IP_LEN);                   // Copy peer IP from payload    
        offset += IP_LEN;                                           

        uint32_t port_net = 0;                                 // Variable for peer port in network byte order  
        memcpy(&port_net, payload + offset, 4);                 // Copy peer port from payload  
        na->port = ntohl(port_net);                              // Convert peer port to host byte order    
        offset += 4;

        memcpy(na->signature, payload + offset, SHA256_HASH_SIZE);  // Copy peer signature from payload
        offset += SHA256_HASH_SIZE;

        memcpy(na->salt, payload + offset, SALT_LEN);              // Copy peer salt from payload
        offset += SALT_LEN;

        network[i] = na;                                          // Add new peer to network
    } 

    printf("Network now has %u peers:\n", peer_count);
    for (uint32_t i = 0; i < peer_count; i++)
        printf("  %s:%u\n", network[i]->ip, network[i]->port);
}


// Receive reply from a peer
int receive_reply(int fd, ReplyHeader_t *header_out, uint8_t **body_out)
{
    ReplyHeader_t header;

    ssize_t n = compsys_helper_readn(fd, &header, REPLY_HEADER_LEN);    // Read reply header
    if (n != REPLY_HEADER_LEN)
        return -1;

    header.length = ntohl(header.length);               // Convert length to host byte order
    header.status = ntohl(header.status);               // Convert status to host byte order        
    header.this_block = ntohl(header.this_block);      // Convert this_block to host byte order
    header.block_count = ntohl(header.block_count);     // Convert block_count to host byte order   

    *header_out = header;                            // Set output header

    uint8_t *body = NULL;                           // Pointer for reply body
    if (header.length > 0) {
        body = malloc(header.length);                   // Allocate memory for reply body
        n = compsys_helper_readn(fd, body, header.length);  // Read reply body
        if (n != header.length) {
            free(body);
            return -1;
        }
    }

    *body_out = body;
    return 0;
}


int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);   // Check for correct number of arguments
        exit(1);
    }

    my_address = malloc(sizeof(NetworkAddress_t));   // Allocate memory for my_address
    memset(my_address->ip, 0, IP_LEN);                    // Initialize IP to zero
    memcpy(my_address->ip, argv[1], strlen(argv[1]));   // Set my IP from command line argument
    my_address->port = atoi(argv[2]);                   // Set my port from command line argument   

    if (!is_valid_ip(my_address->ip)) { 
        fprintf(stderr, "Invalid IP\n");    
        exit(1);    
    }
    if (!is_valid_port(my_address->port)) {
        fprintf(stderr, "Invalid port\n");
        exit(1);
    }

    char password[PASSWORD_LEN];
    printf("Create a password to proceed: ");
    scanf("%15s", password);

    for (int i = strlen(password); i < PASSWORD_LEN; i++)
        password[i] = '\0';

    char salt[SALT_LEN + 1] = "0123456789ABCDEF";   // Example fixed salt for simplicity
    memcpy(my_address->salt, salt, SALT_LEN);       // Set salt for my_address

// Generate signature for this peer
    get_signature(password, PASSWORD_LEN, my_address->salt, 
                  &my_address->signature);

    network = malloc(sizeof(NetworkAddress_t *)); // Allocate memory for network array
    network[0] = my_address;                     // Add my_address to network
    peer_count = 1;                             // Set peer count to 1

    pthread_t client_thread_id;                 // Thread ID for client thread
    pthread_t server_thread_id;                 // Thread ID for server thread

    pthread_create(&client_thread_id, NULL, client_thread, NULL); // Create client thread
    pthread_create(&server_thread_id, NULL, server_thread, NULL); // Create server thread

    pthread_detach(client_thread_id);               // Detach client thread
    pthread_join(server_thread_id, NULL);           // Wait for server thread to finish 

    return 0;
}
