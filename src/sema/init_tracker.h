#ifndef SEMA_INIT_TRACKER__H
#define SEMA_INIT_TRACKER__H

#include "common/containers/hash_table.h"
#include "sema/symbol.h"

/*
 * The init_tracker implements Definite Assignment Analysis.
 * It is used to detect read-access from uninitialized variables, so that the semantic_analyzer
 * can emit errors for these kinds of cases.
 */

typedef struct init_tracker
{
    hash_table_t symbol_state;  // symbol_t* -> is_initialized
} init_tracker_t;

init_tracker_t* init_tracker_create();

void init_tracker_destroy(init_tracker_t* tracker);

// Clone the tracker state (used for branching control flow)
init_tracker_t* init_tracker_clone(init_tracker_t* tracker);

void init_tracker_set_initialized(init_tracker_t* tracker, symbol_t* symbol, bool initialized);

bool init_tracker_is_initialized(init_tracker_t* tracker, symbol_t* symbol);

// Merge two tracker states (used at control flow join points)
// A variable is initialized in the result only if it's initialized in BOTH trackers
// The input trackers are destroyed & their pointers assigned nullptr.
init_tracker_t* init_tracker_merge(init_tracker_t** tracker1, init_tracker_t** tracker2);

#endif
