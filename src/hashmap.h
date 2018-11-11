#ifndef HASHMAP_H
#define HASHMAP_H

#include <pthread.h>            /* pthread_mutex_* */
#include <stdbool.h>            /* bool */
#include <stdlib.h>             /* size_t */


/* Change this function prototype to change unlinker function type. */
typedef int (*hashmap_unlinker)(const char *);

/* A string-string hash map entry. */
typedef struct hashmap_entry {
    struct hashmap_entry *next; /* pointer to next entry in linked list */
    const char *key;            /* the key that was hashed */
    const char *value;          /* the mapped value */
    unsigned long timestamp;    /* timestamp for cache expiration */
} hashmap_entry_t;


typedef struct hashmap {
    hashmap_entry_t **bucket;   /* the hash map's array */
    size_t size;                /* size of the array */
    pthread_mutex_t lock;       /* map lock for multithreading support */
    unsigned long timeout;      /* age in secs to delete entry (0 = never) */
    hashmap_unlinker unlinker;  /* if non-NULL, call unlinker(value) on del */
} hashmap_t;

/* Initialize a hash map with the requested bucket size. Return -1 for OOM. */
int hashmap_init(hashmap_t *map, size_t size);
/* Destroy a hash map and all  */
void hashmap_destroy(hashmap_t *map);
/* Return the index where the key was added or -1 for out of memory. */
int hashmap_add(hashmap_t *map, const char *key, const char *value);
/*
 * Get the `value` associated with `key`.
 *
 * If `key` exists in the map, return the index where it was found and set
 * `value` to point to a heap-allocated copy of the value in the map. The user
 * is responsible for freeing this string. If the hashmap has a non-zero
 * `timeout` value, update the entry's timestamp.
 *
 * If `key` doesn't exist in the map, return -1 and `value' is set to NULL;
 *
 * Touch timestamp if key already exists and map->timeout is non-zero.
 */
int hashmap_get(hashmap_t *map, const char *key, char **value);
/* Return the index where the deleted key was found or -1 for not found. */
int hashmap_del(hashmap_t *map, const char *key);
/* Garbage collect entries older then `timeout` seconds old. */
void hashmap_gc(hashmap_t *map);


#endif  /* HASHMAP_H */
