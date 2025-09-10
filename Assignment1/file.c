#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int print_error(char *path, int errnum) {
    return fprintf(stdout, "%s: cannot determine (%s)\n",
                   path, strerror(errnum));
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: file path\n");
        return EXIT_FAILURE;
    }

    char *filename = argv[1];
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        print_error(filename, errno);
        return EXIT_SUCCESS;
    }

    // Find filens størrelse
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);  // flyt tilbage til start i tilfælde af, vi skal læse senere

    if (filesize == 0) {
        printf("%s: empty\n", filename);
        fclose(fp);
        return EXIT_SUCCESS;
    }

    // Midlertidigt fallback
    printf("%s: data\n", filename);

    fclose(fp);
    return EXIT_SUCCESS;
}
