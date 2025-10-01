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

struct indexed_data {
    struct index_record *irs;
    int n;
};

void* mk_indexed(const struct record* rs, int n) {
    struct indexed_data *d = malloc(sizeof (struct indexed_data));
    assert(d != NULL);

    d ->n = n;
    d ->irs = malloc((size_t)n * sizeof (struct index_record));
    for (int i = 0; i < n; i++) {
        d ->irs[i].osm_id = rs[i].osm_id;
        d ->irs[i].record = &rs[i];
    }
    return d;
}

void free_indexed(void* data) {
    struct indexed_data *d = (struct indexed_data*)data;
    if (!d) return;
    free(d ->irs);
    free(d);
}

const struct record* lookup_indexed(void* data, int64_t needle) {
    const struct indexed_data *d = (const struct indexed_data*)data;
    if (!d || !d->irs) return NULL;

    for (int i = 0; i < d->n; i++) {
        if (d->irs[i].osm_id == needle) {
            return d->irs[i].record;
        }
    }
    return NULL;
}

int main(int argc, char** argv) {
  return id_query_loop(argc, argv,
                    (mk_index_fn)mk_indexed,
                    (free_index_fn)free_indexed,
                    (lookup_fn)lookup_indexed);
}
