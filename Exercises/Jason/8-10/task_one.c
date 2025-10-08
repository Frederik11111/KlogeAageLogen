#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

struct thread_args {
    int id;
    int naptime;
};


void* thread_function(void* arg) {
    struct thread_args payload = *(struct thread_args*)arg;  // copy data

    printf("Thread %d starting (sleeping %d seconds)\n", payload.id, payload.naptime);
    sleep(payload.naptime);  // simulate work
    printf("Thread %d finished\n", payload.id);

    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_threads>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_threads = atoi(argv[1]);
    pthread_t threads[num_threads];
    struct thread_args args[num_threads];

    for (int i = 0; i < num_threads; i++) {
        args[i].id = i;
        args[i].naptime = 1; // time

        pthread_create(&threads[i], NULL, thread_function, &args[i]);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("All threads completed.\n");
    return 0;
}
