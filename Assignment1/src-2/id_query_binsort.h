#ifndef ID_QUERY_BINSORT_H
#define ID_QUERY_BINSORT_H

#include "record.h"

void* mk_binsort(const struct record* rs, int n);
void free_binsort(void* data);
const struct record* lookup_binsort(void* data, int64_t needle);

#endif