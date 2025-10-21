#ifndef SEMA_SEMANTIC_ANALYZER__H
#define SEMA_SEMANTIC_ANALYZER__H

#include "ast/def/class_def.h"
#include "ast/transformer.h"
#include "sema/init_tracker.h"
#include "sema/semantic_context.h"

/*
 * The semantic_analyzer implements the second pass of the SEMantic Analysis.
 * The second pass annotates the expression part of the AST with types, and verifies
 * that the AST has no semantic inconsistencies that prevents compiling the source.
 * For useful analysis decl_collector needs to run first, with a shared semantic_context.
 */

typedef struct semantic_analyzer
{
    ast_transformer_t base;
    semantic_context_t* ctx;  // decl_collector does not own ctx
    symbol_t* current_class;
    symbol_t* current_function;
    symbol_t* current_method;
    symbol_table_t* current_function_scope;  // set if current_function != nullptr or current_method != nullptr
    init_tracker_t* init_tracker;
    bool is_lvalue_context;
    int loop_depth;  // Track nesting depth of loops for break/continue validation
} semantic_analyzer_t;

semantic_analyzer_t* semantic_analyzer_create(semantic_context_t* ctx);

void semantic_analyzer_destroy(semantic_analyzer_t* sema);

// Returns false if decl symbols are not valid (ctx contains errors)
bool semantic_analyzer_run(semantic_analyzer_t* sema, ast_node_t* root);

#endif
