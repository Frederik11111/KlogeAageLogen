// This program reads a newline-separated sequence of integers from
// standard input.  For each such integer, the corresponding Fibonacci
// number is printed.  This is similar to the programs we saw at the
// November 20 lecture.

// Setting _DEFAULT_SOURCE is necessary to activate visibility of
// certain header file contents on GNU/Linux systems.
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

// err.h contains various nonstandard BSD extensions, but they are
// very handy.
#include <err.h>

#include "job_queue.h"

// Whenever we print to the screen, we will first lock this mutex.
// This ensures that multiple threads do not try to print
// concurrently.
pthread_mutex_t stdout_mutex = PTHREAD_MUTEX_INITIALIZER;

// A simple recursive (inefficient) implementation of the Fibonacci
// function.
int fib (int n) {
  if (n < 2) {
    return 1;
  } else {
    return fib(n-1) + fib(n-2);
  }
}

// This function converts a line to an integer, computes the
// corresponding Fibonacci number, then prints the result to the
// screen.
void fib_line(const char *line) {
  int n = atoi(line);
  int fibn = fib(n);
  assert(pthread_mutex_lock(&stdout_mutex) == 0);
  printf("fib(%d) = %d\n", n, fibn);
  assert(pthread_mutex_unlock(&stdout_mutex) == 0);
}

// Each thread will run this function.  The thread argument is a
// pointer to a job queue.
void* worker(void *arg) {
  struct job_queue *jq = arg;

  while (1) {
    char *line;
    if (job_queue_pop(jq, (void**)&line) == 0) {
      fib_line(line);
      free(line);
    } else {
      // If job_queue_pop() returned non-zero, that means the queue is
      // being killed (or some other error occured).  In any case,
      // that means it's time for this thread to die.
      break;
    }
  }

  return NULL;
}

int main(int argc, char * const *argv) {
  int num_threads = 1;

  if (argc == 3 && strcmp(argv[1], "-n") == 0) {
    // Since atoi() simply returns zero on syntax errors, we cannot
    // distinguish between the user entering a zero, or some
    // non-numeric garbage.  In fact, we cannot even tell whether the
    // given option is suffixed by garbage, i.e. '123foo' returns
    // '123'.  A more robust solution would use strtol(), but its
    // interface is more complicated, so here we are.
    num_threads = atoi(argv[2]);

    if (num_threads < 1) {
      err(1, "invalid thread count: %s", argv[2]);
    }
  }

  // Create job queue.
  struct job_queue jq;
  job_queue_init(&jq, 64);

  // Start up the worker threads.
  pthread_t *threads = calloc(num_threads, sizeof(pthread_t));
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&threads[i], NULL, &worker, &jq) != 0) {
      err(1, "pthread_create() failed");
    }
  }


  // Now read lines from stdin until EOF.
  char *line = NULL;
  ssize_t line_len;
  size_t buf_len = 0;
  while ((line_len = getline(&line, &buf_len, stdin)) != -1) {
    job_queue_push(&jq, (void*)strdup(line));
  }
  free(line);

  // Destroy the queue.
  job_queue_destroy(&jq);

  // Wait for all threads to finish.  This is important, at some may
  // still be working on their job.
  for (int i = 0; i < num_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      err(1, "pthread_join() failed");
    }
  }
  free(threads);
}
