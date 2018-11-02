#include <assert.h>             /* assert */
#include <string.h>             /* strdup */
#include <time.h>               /* time */

#include "hash_map.h"


typedef unsigned long hash_t;


/* XOR DJB2 algorithm. */
static inline hash_t hash(unsigned char *str)
{
    hash_t hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) ^ c; /* hash(i - 1) * 33 ^ c */

    return hash;
}


static inline int hash_map_entry_init(hash_map_entry_t *entry,
                                      const char *key, const char *value)
{
    entry->next = NULL;
    entry->key = strdup(key);
    entry->value = strdup(value);
    entry->timestamp = time(NULL);
    entry->is_valid = true;

    if (entry->key == NULL || entry->value == NULL) /* out of memory */
        return -1;

    return 0;
}


static inline void hash_map_entry_destroy(hash_map_entry_t *entry)
{
    assert(entry->key != NULL);
    free(entry->key);

    assert(entry->value != NULL);
    free(entry->value);
}


int hash_map_init(hash_map_t *map, size_t size)
{
    map->bucket = calloc(size, sizeof(hash_map_entry_t *));
    map->size = size;

    if (map->bucket == NULL)    /* out of memory */
        return -1;

    return 0;
}


void hash_map_destroy(hash_map_t *map)
{
    hash_map_entry_t *current, *next;

    if (map == NULL || map->bucket == NULL)
        return;

    for (size_t i = 0; i < map->size; i++) {
        if (map->bucket[i] != NULL) {
            current = map->bucket[i];
            do {
                next = current->next;
                hash_map_entry_destroy(current);
                free(current);
                current = next;
            } while (next != NULL);
        }
    }

    free(map->bucket);
}


int hash_map_add(hash_map_t *map, const char *key, const char *value)
{
    assert(map != NULL);
    assert(key != NULL);
    assert(value != NULL);

    bool entry_exists = false;
    hash_map_entry_t *last_entry, *entry;
    hash_t key_hash = hash((unsigned char *) key);
    int idx = key_hash % map->size;

    last_entry = entry = map->bucket[idx];

    /* Look through existing entries to see if key is already added */
    while (entry != NULL) {
        if (entry->is_valid && !strcmp(key, entry->key)) {
            entry_exists = true;
            break;
        }

        last_entry = entry;
        entry = entry->next;
    }

    if (entry_exists) {
        entry->timestamp = time(NULL);
        return idx;
    }

    /* Add new entry */
    entry = malloc(sizeof(hash_map_entry_t));
    if (entry == NULL)          /* out of memory */
        return -1;

    hash_map_entry_init(entry, key, value);

    if (last_entry == NULL)
        map->bucket[idx] = entry; /* add new entry at head of list */
    else
        last_entry->next = entry; /* add new entry at tail of list */

    return idx;
}


int hash_map_get(hash_map_t *map, const char *key, char **value)
{
    assert(map != NULL);
    assert(key != NULL);

    bool entry_exists = false;
    hash_map_entry_t *entry;
    hash_t key_hash = hash((unsigned char *) key);
    int idx = key_hash % map->size;

    entry = map->bucket[idx];

    while (entry != NULL) {
        if (entry->is_valid && !strcmp(key, entry->key)) {
            entry_exists = true;
            break;
        }

        entry = entry->next;
    }

    if (entry_exists) {
        *value = entry->value;
        entry->timestamp = time(NULL);
        return idx;
    }

    return -1;
}


int hash_map_del(hash_map_t *map, const char *key)
{
    assert(map != NULL);
    assert(key != NULL);

    bool entry_exists = false;
    hash_map_entry_t *last_entry, *entry;
    hash_t key_hash = hash((unsigned char *) key);
    int idx = key_hash % map->size;

    last_entry = entry = map->bucket[idx];

    while (entry != NULL) {
        if (entry->is_valid && !strcmp(key, entry->key)) {
            entry_exists = true;
            break;
        }

        last_entry = entry;
        entry = entry->next;
    }

    if (!entry_exists)
        return -1;

    if (entry == map->bucket[idx])
        /* replace head of linked list */
        map->bucket[idx] = entry->next;
    else
        /* replace non-head entry */
        last_entry->next = entry->next;

    hash_map_entry_destroy(entry);
    free(entry);

    return idx;
}
