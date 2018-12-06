#include <assert.h>             /* assert */
#include <string.h>             /* str* */
#include <time.h>               /* time */

#include "hashmap.h"
#include "printl.h"


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


static inline int hashmap_entry_init(hashmap_entry_t *entry,
                                     const char *key, const char *value)
{
    entry->next = NULL;
    entry->key = strdup(key);
    entry->value = strdup(value);
    entry->timestamp = time(NULL);

    if (entry->key == NULL || entry->value == NULL) /* out of memory */
        return -1;

    return 0;
}


static inline void hashmap_entry_destroy(hashmap_entry_t *entry)
{
    assert(entry->key != NULL);
    free((char *)entry->key);

    assert(entry->value != NULL);
    free((char *)entry->value);
}


int hashmap_init(hashmap_t *map, size_t bucket_size)
{
    int rval = 0;
    pthread_mutexattr_t mutexattr;

    if (!bucket_size)
        return -1;

    map->bucket = calloc(bucket_size, sizeof(hashmap_entry_t *));
    map->bucket_size = bucket_size;

    if (map->bucket == NULL)    /* out of memory */
        return -1;

    map->size = 0;

    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(&map->lock, &mutexattr))
        rval = -1;

    pthread_mutexattr_destroy(&mutexattr);

    return rval;
}


void hashmap_destroy(hashmap_t *map)
{
    hashmap_entry_t *current, *next;

    if (map == NULL || map->bucket == NULL)
        return;

    /* free entries */
    for (size_t i = 0; i < map->bucket_size; i++) {
        if (map->bucket[i] != NULL) {
            current = map->bucket[i];
            do {
                next = current->next;
                if (map->unlinker) {
                    printl(LOG_DEBUG "Unlinking %s\n", current->value);
                    map->unlinker(current->value);
                }
                hashmap_entry_destroy(current);
                free(current);
                current = next;
            } while (next != NULL);
        }
    }

    free(map->bucket);
    pthread_mutex_destroy(&map->lock);
}


int hashmap_add(hashmap_t *map, const char *key, const char *value)
{
    assert(map != NULL);
    assert(map->bucket_size > 0);
    assert(key != NULL);
    assert(value != NULL);

    bool entry_exists = false;
    hashmap_entry_t *last_entry, *entry;
    hash_t key_hash = hash((unsigned char *)key);
    int idx = key_hash % map->bucket_size;

    last_entry = entry = map->bucket[idx];

    pthread_mutex_lock(&map->lock);

    /* Look through existing entries to see if key is already added */
    while (entry != NULL) {
        if (!strcmp(key, entry->key)) {
            entry_exists = true;
            break;
        }
        last_entry = entry;
        entry = entry->next;
    }

    if (entry_exists) {
        /* Update existing entry */
        if (strcmp(entry->value, value)) {
            free((void *)entry->value);
            entry->value = strdup(value);
        }
        if (map->timeout)
            entry->timestamp = time(NULL);
    } else {
        /* Add new entry */
        entry = malloc(sizeof(hashmap_entry_t));
        if (entry == NULL)
            return -1;

        hashmap_entry_init(entry, key, value);
        if (last_entry == NULL)
            map->bucket[idx] = entry; /* add new entry at head of list */
        else
            last_entry->next = entry; /* add new entry at tail of list */

        map->size++;
    }

    pthread_mutex_unlock(&map->lock);

    return idx;
}


int hashmap_get(hashmap_t *map, const char *key, char **value)
{
    assert(map != NULL);
    assert(key != NULL);

    int rval;
    bool entry_exists = false;
    hashmap_entry_t *entry;
    hash_t key_hash = hash((unsigned char *)key);
    int idx = key_hash % map->bucket_size;

    entry = map->bucket[idx];

    pthread_mutex_lock(&map->lock);

    while (entry != NULL) {
        if (!strcmp(key, entry->key)) {
            entry_exists = true;
            break;
        }
        entry = entry->next;
    }

    if (entry_exists) {
        rval = idx;
        if (value != NULL)
            *value = strdup(entry->value);
        if (map->timeout)
            entry->timestamp = time(NULL);
    } else {
        rval = -1;
        if (value != NULL)
            *value = NULL;
    }

    pthread_mutex_unlock(&map->lock);

    return rval;
}


int hashmap_del(hashmap_t *map, const char *key)
{
    assert(map != NULL);
    assert(key != NULL);

    int rval = -1;
    bool entry_exists = false;
    hashmap_entry_t *last_entry, *entry;
    hash_t key_hash = hash((unsigned char *)key);
    int idx = key_hash % map->bucket_size;

    last_entry = entry = map->bucket[idx];

    pthread_mutex_lock(&map->lock);

    while (entry != NULL) {
        if (!strcmp(key, entry->key)) {
            entry_exists = true;
            break;
        }
        last_entry = entry;
        entry = entry->next;
    }

    if (entry_exists) {
        if (entry == map->bucket[idx])
            /* replace head of linked list */
            map->bucket[idx] = entry->next;
        else
            /* replace non-head entry */
            last_entry->next = entry->next;

        if (map->unlinker) {
            printl(LOG_DEBUG "Unlinking %s\n", entry->value);
            map->unlinker(entry->value);
        }

        hashmap_entry_destroy(entry);
        free(entry);
        map->size--;
        rval = idx;
    }

    pthread_mutex_unlock(&map->lock);

    return rval;
}


void hashmap_gc(hashmap_t *map)
{
    assert(map != NULL);

    hashmap_entry_t *current, *next;
    const char msg[] = LOG_DEBUG "Removing cache entry %s\n";
    unsigned long timeout, now = time(NULL);

    pthread_mutex_lock(&map->lock);

    timeout = map->timeout;

    if (!timeout)               /* noop */
        return;

    for (size_t i = 0; i < map->bucket_size; i++) {
        if (map->bucket[i] != NULL) {
            current = map->bucket[i];
            do {
                next = current->next;
                if (now - current->timestamp > timeout) {
                    printl(msg, current->key);
                    hashmap_del(map, current->key);
                }
                current = next;
            } while (next != NULL);
        }
    }

    pthread_mutex_unlock(&map->lock);
}
