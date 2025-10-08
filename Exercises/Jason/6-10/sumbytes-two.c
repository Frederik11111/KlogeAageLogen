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
    size_t mid = size / 2;

    // ---------------------- Prepare thread payloads ----------------------
    struct worker_payload p1 = { data, 0, mid, &sum, &sum_mutex };
    struct worker_payload p2 = { data, mid, size, &sum, &sum_mutex };

    // ---------------------- Create and run threads ----------------------
    pthread_t t1, t2;
    pthread_create(&t1, NULL, thread, &p1);
    pthread_create(&t2, NULL, thread, &p2);

    // ---------------------- Wait for both threads ----------------------
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    // ---------------------- Print result ----------------------
    printf("Sum: %ld\n", sum);

    // ---------------------- Clean up ----------------------
    munmap(data, size);
    pthread_mutex_destroy(&sum_mutex);

    return 0;
}