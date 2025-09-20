#include <stdio.h>
#include <assert.h>

int read_int(FILE *f, int *out){
    int read = fread(out, sizeof(int), 1, f);
    if (read == 1) {
        return 1;
    } else {
        return 0;
    }
}

int main(int argc, char** argv) {
    assert (argc == 1);

    FILE *f = fopen("int2ascii", "r");
    assert(f != NULL);

    int x;
    while (read_int(f, &x)) {
        printf("%d\n", x);
    }
}

