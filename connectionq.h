#ifndef CONNECTIONQ_H
#define CONNECTIONQ_H

#include <pthread.h>            /* pthread_* */
#include <semaphore.h>          /* sem_* */
#include <stdlib.h>             /* size_t */


struct connectionq {
    size_t buffer_size;         /* max number of connections buffer can hold */
    size_t size;                /* current queue size */
    sem_t full;                 /* block adding when full */
    sem_t empty;                /* block consuming when empty */
    pthread_mutex_t lock;       /* lock the queue when modifying */
    int in_idx, out_idx;        /* indexes for last in and out */
    int *buffer;                /* connection buffer */
};


void connectionq_put(struct connectionq *q, int sock);
/* Return the sockfd. */
int connectionq_get(struct connectionq *q);
void connectionq_init(struct connectionq *q, size_t qsize);
void connectionq_destroy(struct connectionq *q);


static inline void connectionq_wait_if_empty(struct connectionq* q)
{
    sem_wait(&q->empty);
}


static inline void connectionq_wait_if_full(struct connectionq *q)
{
    sem_wait(&q->full);
}


static inline void connectionq_signal_available(struct connectionq *q)
{
    sem_post(&q->empty);
}


static inline void connectionq_signal_consumed(struct connectionq *q)
{
    sem_post(&q->full);
}


#endif  /* CONNECTIONQ_H */
