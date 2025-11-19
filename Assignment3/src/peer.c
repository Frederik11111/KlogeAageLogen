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
    // You should never see this printed in your finished implementation
    printf("Server thread done\n");

    return NULL;
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
