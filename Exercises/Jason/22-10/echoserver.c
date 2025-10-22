/*
 * echoserveri.c - An iterative echo server
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAXLINE 8192

void echo(int connfd)
{
    char buf[MAXLINE];
    ssize_t n;

    while ((n = read(connfd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0'; // Null-terminate the message so we can use strcmp

        printf("Received: %s\n", buf);

        // remove newline if using nc (so "ping\n" becomes "ping")
        if (buf[n - 1] == '\n') buf[n - 1] = '\0';
        if (buf[n - 2] == '\r') buf[n - 2] = '\0';

        if (strcmp(buf, "Caroline") == 0) {
            write(connfd, "Lind Larsen\n", 5);
        } else {
            write(connfd, "IKKE MIN KÆRESTE\n", 12);
        }
    }

    if (n < 0)
        perror("read");
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); return EXIT_FAILURE; }

    int opt = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
        return EXIT_FAILURE;
    }
#ifdef SO_REUSEPORT
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEPORT)");  // you can continue if this fails
        // return EXIT_FAILURE;
    }
#endif

    struct sockaddr_in listen_address;
    memset(&listen_address, 0, sizeof listen_address);
    listen_address.sin_family = AF_INET;
    listen_address.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_address.sin_port = htons((unsigned short)atoi(argv[1]));

    if (bind(listenfd, (struct sockaddr*)&listen_address, sizeof listen_address) < 0) {
        perror("bind");
        return EXIT_FAILURE;
    }

    if (listen(listenfd, 128) < 0) {  // larger backlog
        perror("listen");
        return EXIT_FAILURE;
    }

    for (;;) {
        struct sockaddr_storage clientaddr;
        socklen_t clientlen = sizeof clientaddr;

        int connfd = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
        if (connfd < 0) { perror("accept"); continue; }

        echo(connfd);
        close(connfd);
    }

    // Not reached, but good hygiene:
    // shutdown(listenfd, SHUT_RDWR);
    // close(listenfd);
    // return EXIT_SUCCESS;
}