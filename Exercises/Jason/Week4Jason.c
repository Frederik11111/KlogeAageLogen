#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s path\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *filename = argv[1];
    FILE *fp = fopen(filename, "w+");   // open for writing
    if (fp == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    int x = 100;   // set before fork
    printf("hello (pid:%d), x = %d\n", (int)getpid(), x);

    int rc = fork();
    if (rc < 0) {
        fprintf(stderr, "fork failed\n");
        exit(1);
    } else if (rc == 0) {
        printf("child (pid:%d), x = %d\n", (int)getpid(), x);
        x = 200;
        printf("child changed x to %d\n", x);

        fprintf(fp, "child was here\n");
        fflush(fp);
    } else {
        printf("parent of %d (pid:%d), x = %d\n", rc, (int)getpid(), x);
        x = 300;
        printf("parent changed x to %d\n", x);

        fprintf(fp, "parent was here\n");
        fflush(fp);
    }


    fclose(fp);
    return 0;
}