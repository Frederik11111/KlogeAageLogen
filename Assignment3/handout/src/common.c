#include "common.h"

/*
 * Gets a sha256 hash of specified data, sourcedata. The hash itself is
 * placed into the given variable 'hash'. Any size can be created, but a
 * a normal size for the hash would be given by the global variable
 * 'SHA256_HASH_SIZE', that has been defined in sha256.h
 */
void get_data_sha(const char* sourcedata, hashdata_t hash, uint32_t data_size, 
    int hash_size)
{
    SHA256_CTX shactx;
    unsigned char shabuffer[hash_size];
    sha256_init(&shactx);
    sha256_update(&shactx, sourcedata, data_size);
    sha256_final(&shactx, shabuffer);

    for (int i=0; i<hash_size; i++)
    {
        hash[i] = shabuffer[i];
    }
}

/*
 * Gets a sha256 hash of specified data file, sourcefile. The hash itself is
 * placed into the given variable 'hash'. Any size can be created, but a
 * a normal size for the hash would be given by the global variable
 * 'SHA256_HASH_SIZE', that has been defined in sha256.h
 */
void get_file_sha(const char* sourcefile, hashdata_t hash, int size)
{
    int casc_file_size;

    FILE* fp = fopen(sourcefile, "rb");
    if (fp == 0)
    {
        printf("Failed to open source: %s\n", sourcefile);
        return;
    }

    fseek(fp, 0L, SEEK_END);
    casc_file_size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    char buffer[casc_file_size];
    fread(buffer, casc_file_size, 1, fp);
    fclose(fp);

    get_data_sha(buffer, hash, casc_file_size, size);
}

/*
 * Generate a random salt of the length given by the SALT_LEN variable. The
 * salt is written to the 'salt' pointer which should alread by allocated as
 * needed
 */
void generate_random_salt(char *salt) 
{
    srand(time(0));
    for (int i=0; i<SALT_LEN; i++)
    {
        salt[i] = 'a' + (random() % 26);
    }
}

/*
 * returns 1 if ip_string is a valid IP address; else 0
 */
int is_valid_ip(char *ip_string) {
  int ip[4];
  int num_parsed = sscanf(ip_string, "%d.%d.%d.%d", ip+0, ip+1, ip+2, ip+3);

  if (num_parsed < 0) {
    fprintf(stderr, "sscanf() error: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  } 

  else if (num_parsed != 4)
    return string_equal(ip_string, "localhost");

  for (int i = 0; i < 4; i++)
    if (ip[i] > 255 || ip[i] < 0)
      return 0;

  return 1;
}


/*
 * returns 1 if port_string is a valid port number; else 0
 */
int is_valid_port_str(char *port_string) {
  int port;
  int num_parsed = sscanf(port_string, "%d", &port);

  if (num_parsed < 0) {
    fprintf(stderr, "sscanf() error: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  else if (num_parsed != 1) {
    return 0;  
  }
  return is_valid_port(port);
}

/*
 * returns 1 if port is a valid port number; else 0
 */
int is_valid_port(int port) {
  if (port < 0 || port > 65535)
    return 0;

  return 1;
}

/*
 * returns 1 if a starts with b; else 0
 */
int starts_with(const char *a, const char *b)
{
  if(strncmp(a, b, strlen(b)) == 0) return 1;
  return 0;
}
