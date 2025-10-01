CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=gnu99 -g
LDFLAGS=-lm

PROGRAMS = random_ids \
           id_query_naive \
           id_query_indexed \
           id_query_binsort \
           coord_query_naive \
           bench_all

TESTS = ..

.PHONY: all test clean ../src.zip

all: $(PROGRAMS)



random_ids: random_ids.o record.o
	$(CC) -o $@ $^ $(LDFLAGS)

id_query_%: id_query_%.o record.o id_query.o
	$(CC) -o $@ $^ $(LDFLAGS)

coord_query_%: coord_query_%.o record.o coord_query.o
	$(CC) -o $@ $^ $(LDFLAGS)

bench_all: bench_all.o record.o id_query.o id_query_naive.o id_query_indexed.o id_query_binsort.o
	$(CC) -o $@ $^ $(LDFLAGS)



id_query.o: id_query.c id_query.h
	$(CC) -c $< $(CFLAGS)

coord_query.o: coord_query.c coord_query.h
	$(CC) -c $< $(CFLAGS)

record.o: record.c record.h
	$(CC) -c $< $(CFLAGS)

id_query_naive.o: id_query_naive.c id_query_naive.h
	$(CC) -c $< $(CFLAGS)

id_query_indexed.o: id_query_indexed.c id_query_indexed.h
	$(CC) -c $< $(CFLAGS)

id_query_binsort.o: id_query_binsort.c id_query_binsort.h
	$(CC) -c $< $(CFLAGS)

bench_all.o: bench_all.c id_query_naive.h id_query_indexed.h id_query_binsort.h
	$(CC) -c $< $(CFLAGS)

sort.o: sort.c
	$(CC) -c $< $(CFLAGS)

# ------------------- Testing / Cleanup -------------------

test: $(TESTS)
	@set e; for test in $(TESTS); do echo ./$$test; ./$$test; done

clean:
	rm -rf core *.o $(PROGRAMS)

planet-latest-geonames.tsv:
	wget https://github.com/OSMNames/OSMNames/releases/download/v2.0.4/planet-latest_geonames.tsv.gz
	gunzip planet-latest_geonames.tsv.gz

../src.zip:
	make clean
	cd .. && zip src.zip -r src

.SECONDARY: