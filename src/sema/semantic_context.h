#ifndef SEMA_SEMANTIC_CONTEXT__H
#define SEMA_SEMANTIC_CONTEXT__H

#include "common/containers/vec.h"
#include "sema/symbol_table.h"

typedef struct semantic_context
{
    char* self_projectname;
    symbol_t* self_namespace;
    symbol_t* module_namespace;  // nested inside project_namespace
    symbol_table_t* exports;     // points to symbols in other tables, no ownership
    symbol_table_t* global;
    symbol_table_t* current;
    vec_t imports;        // ast_import_t* decls that appear in AST
    vec_t scope_stack;
    vec_t error_nodes;    // vec<ast_node_t*>: nodes that have semantic errors, we do not own these nodes (AST does)
    vec_t warning_nodes;  // vec<ast_node_t*>: nodes that have semantic warnings, we do not own these nodes (AST does)

    vec_t builtin_ast_gc;     // AST that was injected with semantic_context_register_builtins
} semantic_context_t;

semantic_context_t* semantic_context_create(const char* project_name, const char* module_name);

void semantic_context_destroy(semantic_context_t* ctx);

void semantic_context_register_builtins(semantic_context_t* ctx);

void semantic_context_push_scope(semantic_context_t* ctx, scope_kind_t kind);

void semantic_context_pop_scope(semantic_context_t* ctx);

void semantic_context_add_error(semantic_context_t* ctx, void* ast_node, const char* description);

void semantic_context_add_warning(semantic_context_t* ctx, void* ast_node, const char* description);

// Register a new namespace
// parent_namespace: nullptr for top-level (goes in global), or parent symbol for nesting
// name: simple name like "TestProject" or "TestModule" (not qualified)
// symbols: symbol_table to use as exports, or nullptr to create empty namespace
symbol_t* semantic_context_register_namespace(semantic_context_t* ctx, symbol_t* parent_namespace, const char* name,
    symbol_table_t* symbols);

#endif
