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
// Note the addition of mutexs to prevent race conditions.
NetworkAddress_t *my_address;

NetworkAddress_t** network = NULL;
uint32_t peer_count = 0;



// Forward declarations
void* handle_connection(void* arg);
void handle_registration(int connfd, RequestHeader_t* header);
int peer_exists(char* ip, uint32_t port);
void compute_saved_signature(hashdata_t incoming, char* salt, hashdata_t out);


/*
 * Function to act as thread for all required client interactions. This thread 
 * will be run concurrently with the server_thread. It will start by requesting
 * the IP and port for another peer to connect to. Once both have been provided
 * the thread will register with that peer and expect a response outlining the
 * complete network. The user will then be prompted to provide a file path to
 * retrieve. This file request will be sent to a random peer on the network.
 * This request/retrieve interaction is then repeated forever.
 */ 
void* client_thread()
{
    char peer_ip[IP_LEN];
    fprintf(stdout, "Enter peer IP to connect to: ");
    scanf("%16s", peer_ip);

    // Clean up password string as otherwise some extra chars can sneak in.
    for (int i=strlen(peer_ip); i<IP_LEN; i++)
    {
        peer_ip[i] = '\0';
    }

    char peer_port[PORT_STR_LEN];
    fprintf(stdout, "Enter peer port to connect to: ");
    scanf("%16s", peer_port);

    // Clean up password string as otherwise some extra chars can sneak in.
    for (int i=strlen(peer_port); i<PORT_STR_LEN; i++)
    {
        peer_port[i] = '\0';
    }

    NetworkAddress_t peer_address;
    memcpy(peer_address.ip, peer_ip, IP_LEN);
    peer_address.port = atoi(peer_port);
    send_message(peer_address, COMMAND_REGISTER, NULL, 0);


    // You should never see this printed in your finished implementation
    printf("Client thread done\n");

    return NULL;
}


/*
 * Function to act as basis for running the server thread. This thread will be
 * run concurrently with the client thread, but is infinite in nature.
 */
void* server_thread()
{
    char port_str[PORT_STR_LEN];
    snprintf(port_str, PORT_STR_LEN, "%d", my_address->port);

    int listenfd = compsys_helper_open_listenfd(port_str);
    if (listenfd < 0) {
        fprintf(stderr, ">> Error opening listening socket on port %s\n", port_str);
        pthread_exit(NULL);
    }

    while (1) {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) continue;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_connection, (void*)(intptr_t)connfd);
        pthread_detach(tid);
    }

    return NULL;
}


void* handle_connection(void* arg)
{
    int connfd = (intptr_t)arg;

    RequestHeader_t header;
    ssize_t r = compsys_helper_readn(connfd, &header, sizeof(RequestHeader_t));
    if (r <= 0) {
        close(connfd);
        return NULL;
    }

    uint32_t command = ntohl(header.command);

    if (command == 1) {
        handle_registration(connfd, &header);
    }

    close(connfd);
    return NULL;
}


int peer_exists(char* ip, uint32_t port)
{
    for (uint32_t i = 0; i < peer_count; i++) {
        if (strcmp(network[i]->ip, ip) == 0 &&
            network[i]->port == port) 
        {
            return 1;
        }
    }
    return 0;
}

void compute_saved_signature(hashdata_t incoming, char* salt, hashdata_t out)
{
    uint8_t buf[SHA256_HASH_SIZE + SALT_LEN];
    memcpy(buf, incoming, SHA256_HASH_SIZE);
    memcpy(buf + SHA256_HASH_SIZE, salt, SALT_LEN);

    get_data_sha((char*)buf, out, SHA256_HASH_SIZE + SALT_LEN, SHA256_HASH_SIZE);
}

void handle_registration(int connfd, RequestHeader_t* header)
{
    char* ip = header->ip;
    uint32_t port = ntohl(header->port);

    // STEP 1: Reject duplicates
    if (peer_exists(ip, port)) {
        // Important: registration reply MUST still be returned,
        // just with same peer list.
        // For simplicity, we accept duplicates silently here.
    }

    // STEP 2: Allocate new peer
    NetworkAddress_t* new_peer = malloc(sizeof(NetworkAddress_t));
    memcpy(new_peer->ip, ip, IP_LEN);
    new_peer->port = port;

    // Server generates salt
    generate_random_salt(new_peer->salt);

    // Saved signature = SHA256( incoming_signature + salt )
    compute_saved_signature(header->signature, new_peer->salt, new_peer->signature);

    // STEP 3: Add peer to network list
    network = realloc(network, sizeof(NetworkAddress_t*) * (peer_count + 1));
    network[peer_count] = new_peer;
    peer_count++;

    // STEP 4: Build payload (each entry = 68 bytes)
    int entry_size = IP_LEN + 4 + SHA256_HASH_SIZE + SALT_LEN;
    int payload_len = peer_count * entry_size;

    char* payload = malloc(payload_len);
    int offset = 0;

    for (uint32_t i = 0; i < peer_count; i++) {

        memcpy(payload + offset, network[i]->ip, IP_LEN);
        offset += IP_LEN;

        uint32_t p = htonl(network[i]->port);
        memcpy(payload + offset, &p, 4);
        offset += 4;

        memcpy(payload + offset, network[i]->signature, SHA256_HASH_SIZE);
        offset += SHA256_HASH_SIZE;

        memcpy(payload + offset, network[i]->salt, SALT_LEN);
        offset += SALT_LEN;
    }

    // STEP 5: Build reply header
    ReplyHeader_t reply;
    reply.length      = htonl(payload_len);
    reply.status      = htonl(1);
    reply.this_block  = htonl(0);
    reply.block_count = htonl(1);

    // Hash of block = hash(payload)
    get_data_sha(payload, reply.block_hash, payload_len, SHA256_HASH_SIZE);

    // For 1 block: total hash = block hash
    memcpy(reply.total_hash, reply.block_hash, SHA256_HASH_SIZE);

    // STEP 6: Send it
    compsys_helper_writen(connfd, &reply, sizeof(reply));
    compsys_helper_writen(connfd, payload, payload_len);

    free(payload);
}



void get_signature( char *password, int password_len, char* salt, hashdata_t* hash){
    //Cominging salt and password length
    unsigned int total_len = password_len + SALT_LEN;

    //Allocate space for the total length of password and salt in bytes
    uint8_t combined[password_len + SALT_LEN];

    // copy password
    memcpy(combined, password, password_len);

    // Merge with salt
    memcpy(combined + password_len, salt, SALT_LEN);

    // Hash using the given get_data_sha function (SHA256_HASH_SIZE=32bytes)
    get_data_sha((char*)combined, *hash, total_len, SHA256_HASH_SIZE);

}

void send_message(NetworkAddress_t peer_address, int command, char* request_body, int request_len){
    //Build string and convert to char.
    char port_str[PORT_STR_LEN];
    sprintf(port_str, "%u", peer_address.port);

    //open client socket
    int clientfd = compsys_helper_open_clientfd(peer_address.ip, port_str);
      if (clientfd < 0) {
        fprintf(stderr, "Failed to connect  %s:%s\n", peer_address.ip, port_str);
        return;
    }
    //Building the header
    RequestHeader_t header;
    memset(&header, 0, sizeof(header)); //clear the header memory space
    memcpy(header.ip, my_address->ip, IP_LEN);
    header.port = htonl(my_address->port);
    memcpy(header.signature, my_address->signature, SHA256_HASH_SIZE);
    header.command = htonl(command);
    header.length = htonl(request_len);

    //combine header and body
    int total_len = REQUEST_HEADER_LEN + request_len;
    uint8_t buffer[REQUEST_HEADER_LEN + MAX_MSG_LEN];
    memcpy(buffer, &header, REQUEST_HEADER_LEN);


    if (request_len > 0 && request_body != NULL) { // add the body if neeeded
        memcpy(buffer + REQUEST_HEADER_LEN, request_body, request_len);
    }

    if (compsys_helper_writen(clientfd, buffer, total_len) < 0) {
        perror("write");
        close(clientfd);
        return;
    }

}

int main(int argc, char **argv)
{
    // Users should call this script with a single argument describing what 
    // config to use
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        exit(EXIT_FAILURE);
    } 

    my_address = (NetworkAddress_t*)malloc(sizeof(NetworkAddress_t));
    memset(my_address->ip, '\0', IP_LEN);
    memcpy(my_address->ip, argv[1], strlen(argv[1]));
    my_address->port = atoi(argv[2]);

    if (!is_valid_ip(my_address->ip)) {
        fprintf(stderr, ">> Invalid peer IP: %s\n", my_address->ip);
        exit(EXIT_FAILURE);
    }
    
    if (!is_valid_port(my_address->port)) {
        fprintf(stderr, ">> Invalid peer port: %d\n", 
            my_address->port);
        exit(EXIT_FAILURE);
    }

    char password[PASSWORD_LEN];
    fprintf(stdout, "Create a password to proceed: ");
    scanf("%16s", password);

    // Clean up password string as otherwise some extra chars can sneak in.
    for (int i=strlen(password); i<PASSWORD_LEN; i++)
    {
        password[i] = '\0';
    }

    // Most correctly, we should randomly generate our salts, but this can make
    // repeated testing difficult so feel free to use the hard coded salt below
    char salt[SALT_LEN+1] = "0123456789ABCDEF\0";
    //generate_random_salt(salt);
    memcpy(my_address->salt, salt, SALT_LEN);

    
    // add the signature to NetworkAdress
    get_signature(password, PASSWORD_LEN, my_address->salt, &my_address->signature);

    network = malloc(sizeof(NetworkAddress_t*)); 
    network[0] = my_address;
    peer_count = 1;

    // Setup the client and server threads 
    pthread_t client_thread_id;
    pthread_t server_thread_id;
    pthread_create(&client_thread_id, NULL, client_thread, NULL);
    pthread_create(&server_thread_id, NULL, server_thread, NULL);

    // Wait for them to complete. 
    pthread_join(client_thread_id, NULL);
    pthread_join(server_thread_id, NULL);

    exit(EXIT_SUCCESS);
}

