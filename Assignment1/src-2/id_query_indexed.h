#ifndef ID_QUERY_INDEXED_H
#define ID_QUERY_INDEXED_H

#include "record.h"

void* mk_indexed(const struct record* rs, int n);
void free_indexed(void* data);
const struct record* lookup_indexed(void* data, int64_t needle);

#endif