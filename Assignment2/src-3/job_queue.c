#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h> 

#include "job_queue.h"

int job_queue_init(struct job_queue *job_queue, int capacity) {
  job_queue->buffer = malloc(capacity * sizeof(void*));
  if (!job_queue->buffer) return -1;

  job_queue->capacity = capacity;
  job_queue->head = 0;
  job_queue->tail = 0;
  job_queue->count = 0;
  job_queue->shutting_down = 0;

  pthread_mutex_init(&job_queue->mutex, NULL);
  pthread_cond_init(&job_queue->not_empty, NULL);
  pthread_cond_init(&job_queue->not_full, NULL);
  return 0;
}

int job_queue_destroy(struct job_queue *job_queue) {
  pthread_mutex_lock(&job_queue->mutex);

  job_queue->shutting_down = 1; //Flag we are shutting down
  pthread_cond_broadcast(&job_queue->not_empty); // wake any waiting poppers

  while (job_queue->count > 0) {
      pthread_cond_wait(&job_queue->not_full, &job_queue->mutex); //wait intill a not full has been flaged
  }

  pthread_mutex_unlock(&job_queue->mutex);

  //shutting all down
  free(job_queue->buffer);
  pthread_mutex_destroy(&job_queue->mutex);
  pthread_cond_destroy(&job_queue->not_empty);
  pthread_cond_destroy(&job_queue->not_full);
  return 0;
}


int job_queue_push(struct job_queue *job_queue, void *data) {
  pthread_mutex_lock(&job_queue->mutex); //Locks the mutex

  while (job_queue->count == job_queue->capacity){
    pthread_cond_wait(&job_queue->not_full, &job_queue->mutex); //Waits if the capacity is hit
  }

  job_queue->buffer[job_queue->tail] = data; //Inserts the data at the tail
  job_queue->tail=(job_queue->tail + 1) % job_queue->capacity; //Increment tail and use modulo if end is reached
  job_queue->count ++; //Increment count

  pthread_cond_signal(&job_queue->not_empty); //Mark not empty
  pthread_mutex_unlock(&job_queue->mutex); //Unlock mutex
  return 0;
}

int job_queue_pop(struct job_queue *job_queue, void **data) {
  pthread_mutex_lock(&job_queue->mutex); //Locks the mutex

  while (job_queue->count == 0) { //Wait if the count is 0
      if (job_queue->shutting_down) { //If shut down is called, unlock so other can push the last.
          pthread_mutex_unlock(&job_queue->mutex);
          return -1;
      }
      pthread_cond_wait(&job_queue->not_empty, &job_queue->mutex);
  }
  *data = job_queue->buffer[job_queue->head]; //Give the thread the pointer to the job before finishing 
  job_queue->head = (job_queue->head + 1) % job_queue->capacity; //Change the header
  job_queue->count --; //Decrement count

  pthread_cond_signal(&job_queue->not_full); //Mark not full
  pthread_mutex_unlock(&job_queue->mutex); //Unlock mutex
  return 0;
  }

