#include "init_tracker.h"

#include "common/containers/hash_table.h"
#include "common/debug/panic.h"
#include "common/util/ssprintf.h"

#include <string.h>

#define SYMBOL_KEY(symbol) (ssprintf("%p", symbol))

init_tracker_t* init_tracker_create()
{
    init_tracker_t* tracker = malloc(sizeof(*tracker));
    panic_if(tracker == nullptr);

    *tracker = (init_tracker_t){
        .symbol_state = HASH_TABLE_INIT(nullptr),
    };

    return tracker;
}

void init_tracker_destroy(init_tracker_t* tracker)
{
    if (tracker == nullptr)
        return;

    hash_table_deinit(&tracker->symbol_state);

    free(tracker);
}

static void* clone_symbol_state(void* value)
{
    return value;
}

init_tracker_t* init_tracker_clone(init_tracker_t* tracker)
{
    init_tracker_t* new_tracker = init_tracker_create();
    hash_table_deinit(&new_tracker->symbol_state);
    hash_table_clone(&new_tracker->symbol_state, &tracker->symbol_state, clone_symbol_state);
    return new_tracker;
}

void init_tracker_set_initialized(init_tracker_t* tracker, symbol_t* symbol, bool initialized)
{
    hash_table_remove(&tracker->symbol_state, SYMBOL_KEY(symbol));
    hash_table_insert(&tracker->symbol_state, SYMBOL_KEY(symbol), (void*)(uintptr_t)initialized);
}

bool init_tracker_is_initialized(init_tracker_t* tracker, symbol_t* symbol)
{
    void* state = hash_table_find(&tracker->symbol_state, SYMBOL_KEY(symbol));
    return (bool)(uintptr_t)state;  // not found == nullptr => false
}

static void merge_add_symbols_from(init_tracker_t* dst, init_tracker_t* src, init_tracker_t* control)
{
    // TODO: hash_table_t should have some kind of iterator that we use
    for (size_t i = 0; i < src->symbol_state.num_buckets; ++i)
    {
        hash_table_entry_t* entry = src->symbol_state.buckets[i];
        while (entry != nullptr)
        {
            if (hash_table_contains(&dst->symbol_state, entry->key))
            {
                entry = entry->next;
                continue;  // Skip if already processed
            }

            bool init_in_src = (bool)(uintptr_t)entry->value;
            bool init_in_ctrl = (bool)(uintptr_t)hash_table_find(&control->symbol_state, entry->key);

            hash_table_insert(&dst->symbol_state, entry->key, (void*)(uintptr_t)(init_in_src && init_in_ctrl));
            entry = entry->next;
        }
    }
}

init_tracker_t* init_tracker_merge(init_tracker_t** tracker1, init_tracker_t** tracker2)
{
    panic_if(*tracker1 == *tracker2);

    init_tracker_t* new_tracker = init_tracker_create();

    merge_add_symbols_from(new_tracker, *tracker1, *tracker2);
    merge_add_symbols_from(new_tracker, *tracker2, *tracker1);

    init_tracker_destroy(*tracker1);
    *tracker1 = nullptr;
    init_tracker_destroy(*tracker2);
    *tracker2 = nullptr;

    return new_tracker;
}
