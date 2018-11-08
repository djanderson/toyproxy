#include <assert.h>             /* assert */

#include "queue.h"


void queue_put(queue_t *q, queue_type_t item)
{
    queue_wait_if_full(q);
    pthread_mutex_lock(&q->lock);
    q->buffer[q->in_idx] = item;
    q->in_idx = (q->in_idx + 1) % q->buffer_size;
    q->size++;
    pthread_mutex_unlock(&q->lock);
    queue_signal_available(q);
    assert(q->size <= q->buffer_size);
}


queue_type_t queue_get(queue_t *q)
{
    queue_type_t item;

    assert(q->size > 0);
    pthread_mutex_lock(&q->lock);
    item = q->buffer[q->out_idx];
    q->out_idx = (q->out_idx + 1) % q->buffer_size;
    q->size--;
    pthread_mutex_unlock(&q->lock);
    queue_signal_consumed(q);

    return item;
}


void queue_init(queue_t *q, size_t qsize)
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


void queue_destroy(queue_t *q)
{
    sem_destroy(&q->full);
    sem_destroy(&q->empty);
    pthread_mutex_destroy(&q->lock);
    free(q->buffer);
}
