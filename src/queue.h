#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>            /* pthread_* */
#include <semaphore.h>          /* sem_* */
#include <stdlib.h>             /* size_t */

#include "request.h"


typedef request_t queue_type_t;

typedef struct queue {
    size_t buffer_size;         /* max number elements buffer can hold */
    size_t size;                /* current queue size */
    sem_t full;                 /* block adding when full */
    sem_t empty;                /* block consuming when empty */
    pthread_mutex_t lock;       /* lock the queue when modifying */
    int in_idx, out_idx;        /* indexes for last in and out */
    queue_type_t *buffer;       /* internal storage */
} queue_t;

void queue_put(queue_t *q, queue_type_t item);
queue_type_t queue_get(queue_t *q);
void queue_init(queue_t *q, size_t qsize);
void queue_destroy(queue_t *q);


static inline void queue_wait_if_empty(queue_t* q)
{
    sem_wait(&q->empty);
}


static inline void queue_wait_if_full(queue_t *q)
{
    sem_wait(&q->full);
}


static inline void queue_signal_available(queue_t *q)
{
    sem_post(&q->empty);
}


static inline void queue_signal_consumed(queue_t *q)
{
    sem_post(&q->full);
}


#endif  /* QUEUE_H */
