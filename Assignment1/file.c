#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

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
        if (ch == 0 || ch < 0 || ch > 0x7F) {
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
        if (ch == 0 || (ch >= 128 && ch <= 159)) {
            rewind(fp);
            return false; 
        }
    }
    rewind(fp);
    return true;
}

bool is_UTF8(FILE *fp) {
    int ch, next, i;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == 0) { 
            rewind(fp);
            return false;
        }
        if ((ch & 0x80) == 0x00) {  // ASCII
        } else if ((ch & 0xE0) == 0xC0) { // 2 byte
            next = fgetc(fp);
            if (next == EOF || (next & 0xC0) != 0x80) {
                rewind(fp);
                return false;
            }
        } else if ((ch & 0xF0) == 0xE0) { // 3 byte
            for (i = 0; i < 2; i++) {
                next = fgetc(fp);
                if (next == EOF || (next & 0xC0) != 0x80) {
                    rewind(fp);
                    return false;
                }
            }
        } else if ((ch & 0xF8) == 0xF0) { // 4 byte
            for (i = 0; i < 3; i++) {
                next = fgetc(fp);
                if (next == EOF || (next & 0xC0) != 0x80) {
                    rewind(fp);
                    return false;
                }
            }
        } else {
            rewind(fp);
            return false; // invalid start byte
        }
    }
    rewind(fp);
    return true;
}

    FileType detectType(FILE *fp, long filesize) {
        if (filesize == 0) return EMPTY;
        if (is_ascii(fp)) return ASCII;
        if (is_iso8859(fp)) return ISO8859_1;
        if (is_UTF8(fp)) return UTF8;
        return DATA;
    }

    // Error call
    int printError(char *path, int errnum) {
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
        printError(filename, errno);
        return EXIT_SUCCESS;
    }

    // Find filens størrelse
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);  // flyt tilbage til start i tilfælde af, vi skal læse senere

    // detectType call 
    FileType type = detectType(fp, filesize);
    printf("%s: %s\n", filename, FileTypeNames[type]);


    fclose(fp);
    return EXIT_SUCCESS;
}
