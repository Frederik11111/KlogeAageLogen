#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>

#include "record.h"
#include "coord_query.h"

struct naive_data {
  const struct record *rs;
  int n;
};

struct naive_data* mk_naive(const struct record* rs, int n) {
  struct naive_data *d = malloc(sizeof(struct naive_data));
if (!d) {
    fprintf(stderr, "Failed to allocate memory for naive_data (errno: %s)\n", strerror(errno));
    exit(1);
}
  d->rs = rs;
  d->n = n;
  return d;
}

void free_naive(struct naive_data* data) {
  free(data);
}

const struct record* lookup_naive(struct naive_data *data, double lon, double lat) {
  const struct record* best = NULL;
  double best_dist = 1e308; // A very large number

  for (int i = 0; i < data->n; i++) {
    const struct record *r = &data->rs[i];
    double dx = r->lon - lon;
    double dy = r->lat - lat;
    double dist = dx*dx + dy*dy; // We can compare squared distances
    if (dist < best_dist) {
      best_dist = dist;
      best = r;
    }
  }

  return best;
}

int main(int argc, char** argv) {
  return coord_query_loop(argc, argv,
                          (mk_index_fn)mk_naive,
                          (free_index_fn)free_naive,
                          (lookup_fn)lookup_naive);
}
