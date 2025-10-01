#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>

#include "record.h"
#include "id_query.h"

struct index_record {
    int64_t osm_id;
    const struct record *record;
};

struct binsort_data {
    struct index_record *irs;
    int n;
};

int cmp_index_record_by_id(const void *a, const void *b) {
    const struct index_record *ia = (const struct index_record *)a;
    const struct index_record *ib = (const struct index_record *)b;
    if (ia ->osm_id < ib->osm_id) return -1;
    if (ia ->osm_id > ib->osm_id) return  1;
    return 0;
}

void* mk_binsort(const struct record* rs, int n) {
    struct binsort_data *d = malloc(sizeof (struct index_record));
    if (!d) return NULL;
    d->n = n;
    d->irs = malloc((size_t)n * sizeof (struct binsort_data));

    for (int i = 0; i < n; i++) {
        d->irs[i].osm_id = rs[i].osm_id;
        d->irs[i].record = &rs[i];
    }

    qsort(d->irs, (size_t)n, sizeof *d->irs, cmp_index_record_by_id);
    return d;
}

void free_binsort(void* data) {
    struct binsort_data *d = (struct binsort_data*)data;
    if (!d) return;
    free(d->irs);
    free(d);
}

const struct record* lookup_binsort(void* data, int64_t needle) {
    const struct binsort_data *d = (const struct binsort_data*)data;
    if (!d || !d->irs) return NULL;

    int lo = 0, hi = d->n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int64_t key = d->irs[mid].osm_id;
        if (needle < key) {
            hi = mid - 1;
        } else if (needle > key) {
            lo = mid + 1;
        } else {
            return d->irs[mid].record;
        }
    }
    return NULL;
}


int main(int argc, char** argv) {
  return id_query_loop(argc, argv,
                    (mk_index_fn)mk_binsort,
                    (free_index_fn)free_binsort,
                    (lookup_fn)lookup_binsort);
}
