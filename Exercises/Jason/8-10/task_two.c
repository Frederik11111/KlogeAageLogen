#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

struct thread_args {
    long *sum;                 // shared counter
    pthread_mutex_t *mutex;    // protects *sum
    unsigned int m;            // increments per thread
    int id;                    // optional: for prints
};



void* thread_function(void* arg) {
    struct thread_args payload = *(struct thread_args*)arg;  // copy data
    for (unsigned int i = 0; i < payload.m; i++) {
            pthread_mutex_lock(payload.mutex);
            (*payload.sum)++;               // critical section
            pthread_mutex_unlock(payload.mutex);
        }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_threads>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_threads = atoi(argv[1]);
    struct thread_args args[num_threads];
    long sum = 0;
    unsigned int m = atoi(argv[2]);
    pthread_mutex_t sum_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_t *threads = malloc(sizeof(*threads) * num_threads);
    struct thread_args *args = malloc(sizeof(*args) * num_threads);


    for (int i = 0; i < num_threads; i++) {
            args[i].sum   = &sum;
            args[i].mutex = &sum_mutex;
            args[i].m     = m;
            args[i].id    = i;

            int rc = pthread_create(&threads[i], NULL, thread_function, &args[i]);
            if (rc != 0) {
                errno = rc;
                perror("pthread_create");
                return EXIT_FAILURE;
            }
        }

     for (int i = 0; i < num_threads; i++) {
        int rc = pthread_join(threads[i], NULL);
        if (rc != 0) {
            errno = rc;
            perror("pthread_join");
            return EXIT_FAILURE;
        }
    }

    // Expected value: N * M
    long expected = (long)num_threads * (long)m;
    printf("sum = %ld (expected %ld)\n", sum, expected);

    pthread_mutex_destroy(&sum_mutex);
    free(threads);
    free(args);
    return 0;
}
