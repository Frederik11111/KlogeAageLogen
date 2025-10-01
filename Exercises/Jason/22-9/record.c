#include "record.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Read a single record from an open file.  This is pretty tedious, as
// we handle each field explicitly.
int read_record(struct record *r, FILE *f) {
  char *line = NULL;
  size_t n;
  if (getline(&line, &n, f) == -1) {
    free(line);
    return -1;
  }

  r->line = line;
  char* start = line;
  char* end;

  // Note this methodology for reading sub strings. As each item in a line
  // is seperated by a tab we read from our starting index to the tab and 
  // assign that to a attribute, in this case the 'name'. We then update the 
  // start variable to be the next element in the line.
  
  if ((end = strstr(start, "\t"))) {
    *end = 0;
    r->name = start;  //For chars
    start = end+1;
  }

  if ((end = strstr(start, "\t"))) { 
    *end = 0;
    r->lon = atof(start); //ascii to flot
    start = end+1;
  }

    if ((end = strstr(start, "\t"))) { 
    *end = 0;
    r->lat = atof(start); //ascii to flot
    start = end+1;
    }

    if ((end = strstr(start, "\t"))) {
        *end = 0;
        r->country = start;
        start = end + 1;
    }

    if ((end = strstr(start, "\t"))) {
        *end = 0;
        r->display_name = start;
        start = end + 1;
    }

    if ((end = strstr(start, "\t"))) { 
    *end = 0;
    r->west = atof(start); //ascii to flot
    start = end+1;
    }

    if ((end = strstr(start, "\t"))) { 
    *end = 0;
    r->south = atof(start); //ascii to flot
    start = end+1;

    }    if ((end = strstr(start, "\t"))) { 
    *end = 0;
    r->east = atof(start); //ascii to flot
    start = end+1;
    }    
    
    // North (last field, ends with newline instead of tab)
    if ((end = strstr(start, "\n"))) {
        *end = 0;
        r->north = atof(start);
    } else if ((end = strstr(start, "\n"))) {
        *end = 0;
        r->north = atof(start);} 
        else {
        // No newline, just parse whatever is left
        r->north = atof(start);
    }


  return 0;
}

struct record* read_records(const char *filename, int *n) {
  FILE *f = fopen(filename, "r");
  *n = 0;

  if (f == NULL) {
    return NULL;
  }

  char *line = NULL;
  size_t len = 0;
  getline(&line, &len, f);
  free(line);

  int capacity = 100;
  int i = 0;
  struct record *rs = malloc(capacity * sizeof(struct record));
  while (read_record(&rs[i], f) == 0) {
    i++;
    if (i == capacity) {
      capacity *= 2;
      rs = realloc(rs, capacity * sizeof(struct record));
    }
  }

  *n = i;
  fclose(f);
  return rs;
}

void free_records(struct record *rs, int n) {
  for (int i = 0; i < n; i++) {
    free(rs[i].line);
  }
  free(rs);
}
