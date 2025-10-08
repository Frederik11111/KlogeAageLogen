#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>

// ---------------------------------------------------------------------------
// Struct passed to each thread as its "payload"
// This lets each thread know which part of the array to sum and how to
// safely update the shared total.
// ---------------------------------------------------------------------------
struct worker_payload {
    unsigned char *data;      // Pointer to the mmap'ed byte array (file contents)
    size_t start, end;        // Range of indexes [start, end)
    long *sum;                // Pointer to the shared total sum
    pthread_mutex_t *mutex;   // Mutex to protect shared 'sum'
};

// ---------------------------------------------------------------------------
// Thread function: each thread sums its portion of the data array.
// ---------------------------------------------------------------------------
void* thread(void* p) {
    // Copy payload to a local variable
    struct worker_payload payload = *(struct worker_payload*)p;

    // Compute local sum for assigned segment
    long local_sum = 0;
    for (size_t i = payload.start; i < payload.end; i++) {
        local_sum += payload.data[i];
    }

    // Lock mutex before updating shared sum (to avoid race conditions)
    pthread_mutex_lock(payload.mutex);
    *(payload.sum) += local_sum;
    pthread_mutex_unlock(payload.mutex);

    return NULL; // Nothing to return
}

// ---------------------------------------------------------------------------
// Main program
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    // Check that a filename was provided
    assert(argc == 2);
    char *filename = argv[1];

    // ---------------------- Open the file ----------------------
    FILE *file = fopen(filename, "r");
    assert(file != NULL);

    // Determine file size
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // ---------------------- Memory map the file ----------------------
    unsigned char *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fileno(file), 0);
    assert(data != MAP_FAILED);
    fclose(file); // File descriptor no longer needed

    // ---------------------- Shared data ----------------------
    long sum = 0;  // The final shared sum
    pthread_mutex_t sum_mutex = PTHREAD_MUTEX_INITIALIZER;

    // Divide work evenly between two threads
    int num_threads = sysconf(_SC_NPROCESSORS_ONLN);
    long start_index = 0;
    size_t size_pr_thread = size / num_threads;
    long till_index = size_pr_thread; 

    // ---------------------- Prepare thread and create them payloads ----------------------
  pthread_t threads[num_threads];
  struct worker_payload payloads[num_threads];

  for (int i = 0; i < num_threads; i++) {
      size_t start = i * size_pr_thread;
      size_t end = (i == num_threads - 1) ? size : (i + 1) * size_pr_thread;

      payloads[i].data = data;
      payloads[i].start = start;
      payloads[i].end = end;
      payloads[i].sum = &sum;
      payloads[i].mutex = &sum_mutex;

      pthread_create(&threads[i], NULL, thread, &payloads[i]);
  }

    // ---------------------- wait for threads ----------------------
  for (int i = 0; i < num_threads; i++) {
      pthread_join(threads[i], NULL);
  }

    // ---------------------- Print result ----------------------
    printf("Sum: %ld\n", sum);

    // ---------------------- Clean up ----------------------
    munmap(data, size);
    pthread_mutex_destroy(&sum_mutex);

    return 0;
}