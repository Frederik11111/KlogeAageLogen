#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include "record.h"
#include "id_query.h"
#include "id_query_naive.h"
#include "id_query_indexed.h"
#include "id_query_binsort.h"

// ---------------- Timing helper ----------------
long elapsed_us(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) * 1000000L +
           (end.tv_usec - start.tv_usec);
}

// ---------------- Benchmark helper ----------------
typedef void* (*mk_fn)(const struct record*, int);
typedef void  (*free_fn)(void*);
typedef const struct record* (*lookup_fn)(void*, int64_t);

void benchmark(const char *name,
               mk_fn mk, free_fn fre, lookup_fn lookup,
               struct record *rs, int n, const char *idfile) {
    printf("=== %s ===\n", name);

    void *index = mk(rs, n);
    if (!index) { printf("Index build failed.\n"); return; }

    FILE *f = fopen(idfile, "r");
    if (!f) { perror("fopen"); fre(index); return; }

    int64_t id;
    long total_time = 0;
    int count = 0;
    struct timeval start, end;

    while (fscanf(f, "%ld", &id) == 1) {
        gettimeofday(&start, NULL);
        const struct record *rec = lookup(index, id);
        gettimeofday(&end, NULL);

        total_time += elapsed_us(start, end);
        count++;

        // Optional correctness check:
        if (!rec) {
            // printf("ID %ld not found\n", id);
        }
    }
    fclose(f);

    printf("Queries: %d\n", count);
    printf("Total time: %ld us\n", total_time);
    if (count > 0)
        printf("Average: %.2f us/query\n\n", (double)total_time / count);

    fre(index);
}

// ---------------- main ----------------
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <dataset.tsv> <id_list.txt>\n", argv[0]);
        return 1;
    }

    int n;
    struct record *rs = read_records(argv[1], &n);

    benchmark("Naive", mk_naive, free_naive, lookup_naive, rs, n, argv[2]);
    benchmark("Indexed", mk_indexed, free_indexed, lookup_indexed, rs, n, argv[2]);
    benchmark("Binsort", mk_binsort, free_binsort, lookup_binsort, rs, n, argv[2]);

    free(rs);
    return 0;
}