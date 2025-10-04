# ---------------- Compiler & Flags ----------------
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=gnu99 -g
LDFLAGS = -lm

# ---------------- Programs to Build ----------------
PROGRAMS = random_ids \
           id_query_naive \
           id_query_indexed \
           id_query_binsort \
           coord_query_naive \
           coord_query_kdtree \
           bench_all

.PHONY: all clean test

all: $(PROGRAMS)

# ---------------- Generic Rule ----------------
%.o: %.c
	$(CC) -c $< $(CFLAGS)

# ---------------- Linking ----------------
random_ids: random_ids.o record.o
	$(CC) -o $@ $^ $(LDFLAGS)

id_query_%: id_query_%.o record.o id_query.o
	$(CC) -o $@ $^ $(LDFLAGS)

coord_query_%: coord_query_%.o record.o coord_query.o
	$(CC) -o $@ $^ $(LDFLAGS)

# bench_all links with the *_funcs.o versions (no main functions!)
bench_all: bench_all.o record.o id_query.o \
            id_query_naive_funcs.o id_query_indexed_funcs.o id_query_binsort_funcs.o
	$(CC) -o $@ $^ $(LDFLAGS)

# ---------------- Object File Dependencies ----------------
record.o: record.c record.h
id_query.o: id_query.c id_query.h
coord_query.o: coord_query.c coord_query.h

id_query_naive.o: id_query_naive.c id_query_naive.h
id_query_indexed.o: id_query_indexed.c id_query_indexed.h
id_query_binsort.o: id_query_binsort.c id_query_binsort.h

id_query_naive_funcs.o: id_query_naive_funcs.c id_query_naive.h
id_query_indexed_funcs.o: id_query_indexed_funcs.c id_query_indexed.h
id_query_binsort_funcs.o: id_query_binsort_funcs.c id_query_binsort.h

coord_query_naive.o: coord_query_naive.c coord_query.h record.h
coord_query_kdtree.o: coord_query_kdtree.c coord_query.h record.h

bench_all.o: bench_all.c id_query_naive.h id_query_indexed.h id_query_binsort.h

# ---------------- Cleanup ----------------
clean:
	rm -f core *.o $(PROGRAMS)

