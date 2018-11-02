#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stdbool.h>            /* bool */
#include <stdlib.h>             /* size_t */


/* A string-string hash map entry. */
typedef struct hash_map_entry {
    struct hash_map_entry *next; /* pointer to next entry in linked list */
    char *key;                   /* the key that was hashed */
    char *value;                 /* the mapped value */
    unsigned long timestamp;     /* optional timestamp for cache expiration */
    bool is_valid;               /* flag that marks the entry valid */
} hash_map_entry_t;


typedef struct hash_map {
    hash_map_entry_t **bucket;
    size_t size;
} hash_map_t;


/* Initialize a hash map with the requested bucket size. Return -1 for OOM. */
int hash_map_init(hash_map_t *map, size_t size);
/* Destroy a hash map and all  */
void hash_map_destroy(hash_map_t *map);
/*
 * Return the index where the key was added or -1 for out of memory.
 *
 * Touch timestamp if key already exists.
 */
int hash_map_add(hash_map_t *map, const char *key, const char *value);
/*
 * Return the index where the retrieved key was found or -1 for not found.
 *
 * Touch timestamp if key already exists.
 */
int hash_map_get(hash_map_t *map, const char *key, char **value);
/* Return the index where the deleted key was found or -1 for not found. */
int hash_map_del(hash_map_t *map, const char *key);


#endif  /* HASH_MAP_H */
