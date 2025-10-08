#ifndef CONTAINERS_HASH_TABLE__H
#define CONTAINERS_HASH_TABLE__H

#include <stddef.h>
#include <stdint.h>

/*
 * A simble bucket-based hash-table.
 * Only supports string keys currently.
 */

typedef struct hash_table_entry hash_table_entry_t;
typedef void (*hash_table_delete_fn)(void* value);
static constexpr size_t HASH_TABLE_INITIAL_BUCKETS = 16;

typedef struct hash_table
{
    size_t size;
    size_t num_buckets;  // must be power of 2
    hash_table_entry_t** buckets;
    hash_table_delete_fn delete_fn;
} hash_table_t;

struct hash_table_entry
{
    char* key;
    void* value;
    hash_table_entry_t* next;
};

#define HASH_TABLE_INIT(del_fn) (hash_table_t){ \
    .num_buckets = HASH_TABLE_INITIAL_BUCKETS, \
    .buckets = calloc(HASH_TABLE_INITIAL_BUCKETS, sizeof(hash_table_entry_t*)), \
    .delete_fn = del_fn, \
}

void hash_table_deinit(hash_table_t* table);

hash_table_t* hash_table_create(hash_table_delete_fn delete_fn);

void hash_table_destroy(hash_table_t* table);

/* Hash-table stores the given value pointer and makes a copy of the key string.
 * It is the caller's responsibility to ensure that the value outlives the table.
 * If the table has a delete_fn, the table will assume ownership and clean up the
 * memory when the entry is removed (via hash_table_remove) or when the table
 * is destroyed. Otherwise, ownership remains with the caller.
 *
 * NOTE: It is invalid to insert a key that already exists in the table, but this
 *       is not protected against by the table itself.
 */
void hash_table_insert(hash_table_t* table, const char* key, void* value);

void* hash_table_find(hash_table_t* table, const char* key);

void hash_table_remove(hash_table_t* table, const char* key);

#endif
