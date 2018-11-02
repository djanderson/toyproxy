#include <assert.h>             /* assert */

#include "connectionq.h"


void connectionq_put(struct connectionq *q, int sock)
{
    connectionq_wait_if_full(q);
    pthread_mutex_lock(&q->lock);
    q->buffer[q->in_idx] = sock;
    q->in_idx = (q->in_idx + 1) % q->buffer_size;
    q->size++;
    pthread_mutex_unlock(&q->lock);
    connectionq_signal_available(q);
    assert(q->size <= q->buffer_size);
}


int connectionq_get(struct connectionq *q)
{
    int sock;

    assert(q->size > 0);
    pthread_mutex_lock(&q->lock);
    sock = q->buffer[q->out_idx];
    q->out_idx = (q->out_idx + 1) % q->buffer_size;
    q->size--;
    pthread_mutex_unlock(&q->lock);
    connectionq_signal_consumed(q);

    return sock;
}


void connectionq_init(struct connectionq *q, size_t qsize)
{
    q->buffer_size = qsize;
    q->size = 0;
    q->in_idx = 0;
    q->out_idx = 0;

    int pshared = 0;            /* share between threads, but not processes */
    sem_init(&q->full, pshared, qsize);
    sem_init(&q->empty, pshared, 0);

    pthread_mutex_init(&q->lock, NULL);

    q->buffer = malloc(qsize * sizeof(int));
}


void connectionq_destroy(struct connectionq *q)
{
    sem_destroy(&q->full);
    sem_destroy(&q->empty);
    pthread_mutex_destroy(&q->lock);
    free(q->buffer);
}
