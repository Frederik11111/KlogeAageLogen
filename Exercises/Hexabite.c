#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char **argv) {
    // Must have exactly 1 argument (the filename)
    assert(argc == 2);

    char *filename = argv[1];
    FILE *fp = fopen(filename, "rb");  // binary mode
    assert(fp != NULL);

    unsigned char byte;  // use unsigned char to hold raw bytes

    // fread returns number of elements successfully read
    while (fread(&byte, 1, 1, fp) == 1) {
        printf("%.2x\n", byte);
    }

    int read_int(FILE *fp, int *out);

    fclose(fp);
    return EXIT_SUCCESS;
}
