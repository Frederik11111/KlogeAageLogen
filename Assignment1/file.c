#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

// ENUM + navnetabel
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
    "ISO-8859 text",
    "UTF-8 Unicode text",
    "data"
};

// ---------------- ASCII ----------------
bool is_ascii(FILE *fp) {
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == 0 || ch > 0x7F) {
            rewind(fp);
            return false;
        }
        if (ch < 0x20 && ch != '\n' && ch != '\r' && ch != '\t') {
            rewind(fp);
            return false; // kontroltegn → ikke tekst
        }
    }
    rewind(fp);
    return true;
}

// ---------------- ISO-8859-1 ----------------
bool is_iso8859(FILE *fp) {
    int ch;
    bool found_basic = false;   // almindelige ASCII-tegn
    bool found_high  = false;   // high bytes
    bool found_any   = false;

    while ((ch = fgetc(fp)) != EOF) {
        found_any = true;

        if (ch == 0) { rewind(fp); return false; }

        // afvis kontroltegn undtagen newline/tab/carriage return
        if (ch < 0x20 && ch != '\n' && ch != '\r' && ch != '\t') {
            rewind(fp); return false;
        }
        if (ch == 127) { rewind(fp); return false; }

        if (ch >= 160 && ch <= 255) {
            found_high = true;
        }
        if ((ch >= 32 && ch <= 126) || ch == '\n' || ch == '\r' || ch == '\t') {
            found_basic = true;
        }
    }
    rewind(fp);

    // kræv: mindst ét ASCII-tegn + evt. nogle high bytes
    return found_any && found_basic && found_high;
}



// ---------------- UTF-8 ----------------
bool is_UTF8(FILE *fp) {
    int ch;
    bool found_printable = false;

    while ((ch = fgetc(fp)) != EOF) {
        if (ch == 0) {
            rewind(fp);
            return false;
        }

        if ((ch & 0x80) == 0x00) {
            // 1-byte ASCII
            if ((ch >= 32 && ch <= 126) || ch == '\n' || ch == '\r' || ch == '\t') {
                found_printable = true;
            } else if (ch < 0x20) {
                rewind(fp);
                return false; // kontroltegn afvist
            }
        } else if ((ch & 0xE0) == 0xC0) {
            int b1 = fgetc(fp);
            if (b1 == EOF || (b1 & 0xC0) != 0x80) { rewind(fp); return false; }
            int codepoint = ((ch & 0x1F) << 6) | (b1 & 0x3F);
            if (codepoint < 0x80) { rewind(fp); return false; } // overlong
            if (codepoint >= 32) found_printable = true;
        } else if ((ch & 0xF0) == 0xE0) {
            int b1 = fgetc(fp), b2 = fgetc(fp);
            if (b2 == EOF || (b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) { rewind(fp); return false; }
            int codepoint = ((ch & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
            if (codepoint < 0x800) { rewind(fp); return false; } // overlong
            if (codepoint >= 32) found_printable = true;
        } else if ((ch & 0xF8) == 0xF0) {
            int b1 = fgetc(fp), b2 = fgetc(fp), b3 = fgetc(fp);
            if (b3 == EOF || (b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) {
                rewind(fp); return false;
            }
            int codepoint = ((ch & 0x07) << 18) |
                            ((b1 & 0x3F) << 12) |
                            ((b2 & 0x3F) << 6) |
                            (b3 & 0x3F);
            if (codepoint < 0x10000) { rewind(fp); return false; } // overlong
            if (codepoint >= 32) found_printable = true;
        } else {
            rewind(fp);
            return false; // invalid start byte
        }
    }
    rewind(fp);

    return found_printable; // kræv mindst ét printbart tegn
}



// ---------------- Type detection ----------------
FileType detectType(FILE *fp, long filesize) {
    if (filesize == 0) return EMPTY;
    if (is_ascii(fp)) return ASCII;
    if (is_UTF8(fp)) return UTF8;
    if (is_iso8859(fp)) return ISO8859_1;
    return DATA;
}

// ---------------- Error helper ----------------
int printError(char *path, int errnum) {
    return fprintf(stdout, "%s: cannot determine (%s)\n",
                   path, strerror(errnum));
}

// ---------------- Main ----------------
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

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    FileType type = detectType(fp, filesize);
    printf("%s: %s\n", filename, FileTypeNames[type]);

    fclose(fp);
    return EXIT_SUCCESS;
}
