#ifndef SEMA_SEMANTIC_CONTEXT__H
#define SEMA_SEMANTIC_CONTEXT__H

#include "common/containers/vec.h"
#include "sema/symbol_table.h"

typedef struct semantic_context
{
    symbol_table_t* global;
    symbol_table_t* current;
    vec_t scope_stack;
    vec_t error_nodes;  // vec<ast_node_t*>: nodes that have semantic errors, we do not own these nodes (AST does)
    vec_t warning_nodes;  // vec<ast_node_t*>: nodes that have semantic warnings, we do not own these nodes (AST does)
} semantic_context_t;

semantic_context_t* semantic_context_create();

void semantic_context_destroy(semantic_context_t* ctx);

void semantic_context_push_scope(semantic_context_t* ctx, scope_kind_t kind);

void semantic_context_pop_scope(semantic_context_t* ctx);

void semantic_context_add_error(semantic_context_t* ctx, void* ast_node, const char* description);

void semantic_context_add_warning(semantic_context_t* ctx, void* ast_node, const char* description);

#endif
