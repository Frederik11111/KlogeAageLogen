#include <stdio.h>
#include <assert.h>

int main(int argc, char** argv) {
    assert(argc == 1);

    FILE *f = fopen("hexabyte", "r");
    assert(f != NULL);
    
    char c;

    while (fread(&c, 1, 1, f) == 1) {
        printf("%.2x\n ", (int)c);
    }
}

