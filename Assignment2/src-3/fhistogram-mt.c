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

#include "job_queue.h"

// Mutex for protecting global histogram and array shared among threads
pthread_mutex_t global_histogram_mutex = PTHREAD_MUTEX_INITIALIZER;
int global_histogram[8] = {0};
// err.h contains various nonstandard BSD extensions, but they are
// very handy.
#include <err.h>

#include "histogram.h"

// Worker thread function
void *worker(void *arg) {
  struct job_queue *jq = arg;

  while (1) {
    char *path;

    // Try popping a job from queue
    if (job_queue_pop(jq, (void**)&path) != 0) {
        break; // queue shutting downn
    }

    // Open file for reading
    FILE *f = fopen(path, "r");
    if (f == NULL) {
      // Print warning, but continue processing files
      warn("failed to open %s", path);
      free(path);
      continue;
    }

    // Compute local histogram 
    int local_histogram[8] = {0};
    char c;
    while (fread(&c, sizeof(c), 1, f) == 1) {
      update_histogram(local_histogram, c);
    }
    fclose(f);

    // Merge local histogram into global histogram
    //Lock to prevent data races.
    pthread_mutex_lock(&global_histogram_mutex);
    merge_histogram(local_histogram, global_histogram);

    // Print updated histogram
    print_histogram(global_histogram);
    pthread_mutex_unlock(&global_histogram_mutex);

    // Free path string allocated
    free(path);
  }
  return NULL;
}

// Function that parses arguments, sets job queue and creates worker threads.
// Traverses directories, enqueues files for processing
int main(int argc, char * const *argv) {
  if (argc < 2) {
    err(1, "usage: paths...");
    exit(1);
  }

  int num_threads = 1;
  char * const *paths = &argv[1];

  if (argc > 3 && strcmp(argv[1], "-n") == 0) {
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

    // Adjust path list
    paths = &argv[3];
  } else {
    paths = &argv[1];
  }

  // capacity for 100 jobs
  struct job_queue jq;
  job_queue_init(&jq, 100);

  // Start worker threads that will process files from the queue.
  pthread_t threads[num_threads];
  for (int i = 0; i < num_threads; i++) {
    pthread_create(&threads[i], NULL, worker, &jq);
  } // Initialise the job queue and some worker threads here.

  // FTS_LOGICAL = follow symbolic links
  // FTS_NOCHDIR = do not change the working directory of the process
  //
  // (These are not particularly important distinctions for our simple
  // uses.)
  int fts_options = FTS_LOGICAL | FTS_NOCHDIR;

  FTS *ftsp;
  if ((ftsp = fts_open(paths, fts_options, NULL)) == NULL) {
    err(1, "fts_open() failed");
    return -1;
  }

  //Traverse directory tree. Regular files found are enqueued fr processing
  FTSENT *p;
  while ((p = fts_read(ftsp)) != NULL) {
    switch (p->fts_info) {
    case FTS_D: // Ignore directory
      break;
    case FTS_F: // Regular file
      job_queue_push(&jq, strdup(p->fts_path));
      break;
    default:
      break;
    }
  }

// Done 
fts_close(ftsp);

  // Signal that no more jobs will be added.
job_queue_finish(&jq);

// Wait for all worker threads to complete their work.
for (int i = 0; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
}

// Destroy the job queue and free resources.
job_queue_destroy(&jq);

// Move the cursor back down to restore display formatting.
move_lines(9);

return 0;
}