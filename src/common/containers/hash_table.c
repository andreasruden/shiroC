#include "hash_table.h"

#include <stdlib.h>
#include <string.h>

static constexpr float LOAD_FACTOR_THRESHOLD = 0.75f;

hash_table_entry_t* hash_table_entry_destroy(hash_table_entry_t* entry, hash_table_delete_fn delete_fn)
{
    hash_table_entry_t* next = entry->next;
    free(entry->key);
    if (delete_fn != nullptr)
        delete_fn(entry->value);
    free(entry);
    return next;
}

void hash_table_deinit(hash_table_t* table)
{
    if (table == nullptr)
        return;

    for (size_t i = 0; i < table->num_buckets; ++i)
    {
        hash_table_entry_t* entry = table->buckets[i];
        while (entry != nullptr)
            entry = hash_table_entry_destroy(entry, table->delete_fn);
    }

    free(table->buckets);
}

hash_table_t* hash_table_create(hash_table_delete_fn delete_fn)
{
    hash_table_t* table = malloc(sizeof(*table));
    *table = HASH_TABLE_INIT(delete_fn);
    return table;
}

void hash_table_destroy(hash_table_t* table)
{
    if (table == nullptr)
        return;
    hash_table_deinit(table);
    free(table);
}

static uint64_t hash_table_hash_str(const char* str)
{
    // Algorithm: FNV-1a
    uint64_t hash = 0xcbf29ce484222325ULL;  // FNV offset basis

    while (*str)
    {
        hash ^= (uint64_t)(unsigned char)*str;
        hash *= 0x100000001b3ULL;  // FNV prime
        ++str;
    }

    return hash;
}

static void hash_table_insert_impl(hash_table_entry_t** buckets, size_t num_buckets, hash_table_entry_t* entry)
{
    size_t bucket = hash_table_hash_str(entry->key) & (num_buckets - 1);
    hash_table_entry_t* tail = buckets[bucket];
    while (tail != nullptr && tail->next != nullptr)
        tail = tail->next;

    if (tail == nullptr)
        buckets[bucket] = entry;
    else
        tail->next = entry;
}

static void hash_table_grow(hash_table_t* table)
{
    size_t new_num_buckets = table->num_buckets << 1;  // must keep size as power of 2
    hash_table_entry_t** new_buckets = calloc(new_num_buckets, sizeof(hash_table_entry_t*));

    for (size_t i = 0; i < table->num_buckets; ++i)
    {
        hash_table_entry_t* entry = table->buckets[i];
        while (entry != nullptr)
        {
            hash_table_entry_t* next = entry->next;
            entry->next = nullptr;
            hash_table_insert_impl(new_buckets, new_num_buckets, entry);
            entry = next;
        }
    }

    free(table->buckets);
    table->buckets = new_buckets;
    table->num_buckets = new_num_buckets;
}

void hash_table_insert(hash_table_t* table, const char* key, void* value)
{
    if ((float)table->size / table->num_buckets >= LOAD_FACTOR_THRESHOLD)
        hash_table_grow(table);

    hash_table_entry_t* new_entry = malloc(sizeof(*new_entry));
    *new_entry = (hash_table_entry_t){
        .key = strdup(key),
        .value = value,
    };

    hash_table_insert_impl(table->buckets, table->num_buckets, new_entry);
    ++table->size;
}

void* hash_table_find(hash_table_t* table, const char* key)
{
    size_t bucket = hash_table_hash_str(key) & (table->num_buckets - 1);
    hash_table_entry_t* entry = table->buckets[bucket];
    while (entry != nullptr)
    {
        if (strcmp(key, entry->key) == 0)
            return entry->value;
        entry = entry->next;
    }
    return nullptr;
}

bool hash_table_contains(hash_table_t* table, const char* key)
{
    size_t bucket = hash_table_hash_str(key) & (table->num_buckets - 1);
    hash_table_entry_t* entry = table->buckets[bucket];
    while (entry != nullptr)
    {
        if (strcmp(key, entry->key) == 0)
            return true;
        entry = entry->next;
    }
    return false;
}

void hash_table_remove(hash_table_t* table, const char* key)
{
    size_t bucket = hash_table_hash_str(key) & (table->num_buckets - 1);
    bool found = false;
    hash_table_entry_t* entry = table->buckets[bucket];
    hash_table_entry_t* prev_entry = nullptr;

    while (entry != nullptr)
    {
        if (strcmp(key, entry->key) == 0)
        {
            found = true;
            break;
        }
        prev_entry = entry;
        entry = entry->next;
    }

    if (found)
    {
        if (prev_entry != nullptr)
            prev_entry->next = entry->next;
        else
            table->buckets[bucket] = nullptr;
        hash_table_entry_destroy(entry, table->delete_fn);
        --table->size;
    }
}

void hash_table_clone(hash_table_t* dst, hash_table_t* src, hash_table_clone_value_fn clone_value_fn)
{
    *dst = HASH_TABLE_INIT(src->delete_fn);

    for (size_t i = 0; i < src->num_buckets; ++i)
    {
        hash_table_entry_t* entry = src->buckets[i];
        while (entry != nullptr)
        {
            hash_table_insert(dst, entry->key, clone_value_fn(entry->value));
            entry = entry->next;
        }
    }
}
