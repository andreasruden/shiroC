#ifndef SEMA_DECL_COLLECTOR__H
#define SEMA_DECL_COLLECTOR__H

#include "sema/semantic_context.h"

/*
 * The decl_collector implements the first pass of the SEMantic Analysis.
 */

typedef struct decl_collector decl_collector_t;

decl_collector_t* decl_collector_create(semantic_context_t* ctx);

void decl_collector_destroy(decl_collector_t* collector);

// Returns false if decl symbols are not valid (ctx contains errors)
bool decl_collector_run(decl_collector_t* collector, ast_node_t* root);

#endif
