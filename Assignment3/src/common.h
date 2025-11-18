#pragma once
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "compsys_helpers.h"
#include "sha256.h"

/*
 * add macros and function declarations which should be 
 * common between the name server and peer programs in this file.
 */

#define COMMAND_REGISTER        1
#define COMMAND_RETREIVE        2
#define COMMAND_INFORM          3

#define STATUS_OK               1
#define STATUS_PEER_EXISTS      2
#define STATUS_PEER_MISSING     3
#define STATUS_BAD_PASSWORD     4
#define STATUS_BAD_REQUEST      5
#define STATUS_OTHER            6
#define STATUS_MALFORMED        7

#define IP_LEN                  16
#define PORT_LEN                4
#define PORT_STR_LEN            8

#define REQUEST_HEADER_LEN      60
#define REPLY_HEADER_LEN        80 
#define PEER_ADDR_LEN           68

#define MAX_MSG_LEN             8196

#define PATH_LEN                128
#define SALT_LEN                16
#define PASSWORD_LEN            16

// container for hasher
typedef uint8_t hashdata_t[SHA256_HASH_SIZE];

/* slightly less awkward string equality check */
#define string_equal(x, y) (strncmp((x), (y), strlen(y)) == 0)

/* functions to get sha hashes */
void get_data_sha(const char* sourcedata, hashdata_t hash, uint32_t data_size, 
    int hash_size);
void get_file_sha(const char* sourcefile, hashdata_t hash, int size);

/* Get a random string to use as a salt */
void generate_random_salt(char *salt);

/*
 * naive validity checks of IP addresses and port numbers given as strings,
 * eg. at command line. both return zero on invalid IP/port, else non-zero.
 */
int is_valid_ip(char *ip_string);
int is_valid_port_str(char *port_string);
int is_valid_port(int port);
int starts_with(const char *a, const char *b);
