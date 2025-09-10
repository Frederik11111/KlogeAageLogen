#include <stdio.h>
#include <stdlib.h>

int main(int argc, char argv[]) {
    // Check arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]); 
        return EXIT_FAILURE;
    }

    // Get filename from arguments
    char filename = argv[1];

    // Try to open the file

    FILEfp = fopen(filename, "rb"); // Open in binary mode
    if (fp == NULL) {
        printf("%s: No such file or directory\n", filename);
        return EXIT_FAILURE;
    }

    // File opened successfully
    printf("s: data\n", filename);

    // Close the file and return success
    fclose(fp);
    return EXIT_SUCCESS;
}