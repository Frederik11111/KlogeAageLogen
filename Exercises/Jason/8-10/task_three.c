#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
struct job {
    int producer_id;
    int job_id;
};

struct queue {
    int total_capacity;     // total allocated space
    int current_capacity;   // number of jobs currently in queue
    struct job **jobs;      // array of pointers to jobs
    pthread_mutex_t mutex;  // protects the queue
};

struct queue* queue_init(int capacity) {
    struct queue *q = malloc(sizeof(struct queue));
    q->total_capacity = capacity;
    q->current_capacity = 0;
    q->jobs = malloc(sizeof(struct job*) * capacity);
    pthread_mutex_init(&q->mutex, NULL);
    return q;
}

struct producer_payload {
    int producer_id;
    int num_jobs;
    struct queue *shared_q;
};

void* producer_thread(void* arg) {
    struct producer_payload *p = arg;

    for (int i = 0; i < p->num_jobs; i++) {
        struct job *j = malloc(sizeof(struct job));
        j->producer_id = p->producer_id;
        j->job_id = i;
        push_job(p->shared_q, j);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_threads> <num_jobs_per_thread>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_threads = atoi(argv[1]);
    int num_jobs = atoi(argv[2]);

    struct queue *shared_q = queue_init(num_threads * num_jobs);

    pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
    struct producer_payload *payloads = malloc(sizeof(struct producer_payload) * num_threads);

    for (int i = 0; i < num_threads; i++) {
        payloads[i].producer_id = i;
        payloads[i].num_jobs = num_jobs;
        payloads[i].shared_q = shared_q;
        pthread_create(&threads[i], NULL, producer_thread, &payloads[i]);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("All producers done. Queue size: %d\n", shared_q->current_capacity);

    // Cleanup
    for (int i = 0; i < shared_q->current_capacity; i++) {
        free(shared_q->jobs[i]);
    }
    free(shared_q->jobs);
    pthread_mutex_destroy(&shared_q->mutex);
    free(shared_q);
    free(threads);
    free(payloads);
}
