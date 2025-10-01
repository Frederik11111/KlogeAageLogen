#ifndef ID_QUERY_NAIVE_H
#define ID_QUERY_NAIVE_H

#include "record.h"

// Create a naive index
void* mk_naive(const struct record* rs, int n);

// Free a naive index
void free_naive(void* data);

// Lookup an ID in the naive index
const struct record* lookup_naive(void* data, int64_t needle);

#endif