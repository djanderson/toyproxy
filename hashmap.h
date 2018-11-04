#ifndef HASHMAP_H
#define HASHMAP_H

#include <pthread.h>            /* pthread_mutex_* */
#include <stdbool.h>            /* bool */
#include <stdlib.h>             /* size_t */


/* A string-string hash map entry. */
typedef struct hashmap_entry {
    struct hashmap_entry *next; /* pointer to next entry in linked list */
    char *key;                   /* the key that was hashed */
    char *value;                 /* the mapped value */
    unsigned long timestamp;     /* optional timestamp for cache expiration */
    bool is_valid;               /* flag that marks the entry valid */
} hashmap_entry_t;


typedef struct hashmap {
    hashmap_entry_t **bucket;   /* the hash map's array */
    size_t size;                 /* size of the array */
    pthread_mutex_t lock;        /* map lock for multithreading support */
} hashmap_t;


/* Initialize a hash map with the requested bucket size. Return -1 for OOM. */
int hashmap_init(hashmap_t *map, size_t size);
/* Destroy a hash map and all  */
void hashmap_destroy(hashmap_t *map);
/*
 * Return the index where the key was added or -1 for out of memory.
 *
 * Touch timestamp if key already exists.
 */
int hashmap_add(hashmap_t *map, const char *key, const char *value);
/*
 * Return the index where the retrieved key was found or -1 for not found.
 *
 * Touch timestamp if key already exists.
 */
int hashmap_get(hashmap_t *map, const char *key, char **value);
/* Return the index where the deleted key was found or -1 for not found. */
int hashmap_del(hashmap_t *map, const char *key);


#endif  /* HASHMAP_H */
