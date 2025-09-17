#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
// Error call
int printError(char *path, int errnum) {
    return fprintf(stdout, "%s: cannot determine (%s)\n",
                   path, strerror(errnum));
}

// ENUM + char table
typedef enum {
    EMPTY,
    ASCII,
    ISO8859_1,
    UTF8,
    DATA
} FileType;
const char *FileTypeNames[] = {
    "empty",
    "ASCII text",
    "ISO-8859-1 text",
    "UTF-8 Unicode text",
    "data"
};

// FileType finders
bool is_ascii(FILE *fp) {
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch < 0 || ch > 0x7F) {
            rewind(fp);
            return false;
        }
    }
    rewind(fp);
    return true;
}

bool is_iso8859(FILE *fp) {
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch >= 128 && ch <= 159) {
            rewind(fp);
            return false; 
        }
    }
    rewind(fp);
    return true;
}

bool is_UTF8 (FILE *fp) {
    //to do
}


FileType detectType(FILE *fp, long filesize) {
    if (filesize == 0) return EMPTY;
    if (is_ascii(fp) == 0) return ASCII;
    if (is_iso8859(fp) == 0) return ISO8859_1;
    if (is_UTF8(fp) == 0) return UTF8;
    return DATA;
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



    // Midlertidigt fallback
    printf("%s: data\n", filename);

    fclose(fp);
    return EXIT_SUCCESS;
}
