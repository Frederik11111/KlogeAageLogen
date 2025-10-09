#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "job_queue.h"

struct job_queue {
    void **buffer;              // array of void* (the jobs)
    int capacity;               // max number of jobs
    int head;                   // index of next item to pop
    int tail;                   // index of next slot to push
    int count;                  // current number of items

    pthread_mutex_t mutex;      // protects shared state
    pthread_cond_t not_empty;   // signaled when a job is added
    pthread_cond_t not_full;    // signaled when a job is removed
    int shutting_down;          // flag used for destroy()
};


int job_queue_init(struct job_queue *job_queue, int capacity) {
  int head, tail, count, shutting_down = 0;
  capacity = capacity;

  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  assert(0);
}

int job_queue_destroy(struct job_queue *job_queue) {
  assert(0);
}

int job_queue_push(struct job_queue *job_queue, void *data) {
  assert(0);
}

int job_queue_pop(struct job_queue *job_queue, void **data) {
  assert(0);
}

