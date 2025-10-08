#include "semantic_context.h"

#include "common/containers/vec.h"
#include "compiler_error.h"
#include "sema/symbol_table.h"

#include <assert.h>

semantic_context_t* semantic_context_create()
{
    semantic_context_t* ctx = malloc(sizeof(*ctx));

    symbol_table_t* global_scope = symbol_table_create(nullptr, SCOPE_GLOBAL);

    *ctx = (semantic_context_t){
        .global = global_scope,
        .current = global_scope,
        .scope_stack = VEC_INIT(symbol_table_destroy_void),
        .error_nodes = VEC_INIT(nullptr),    // we do not own these nodes
        .warning_nodes = VEC_INIT(nullptr),  // we do not own these nodes
    };

    vec_push(&ctx->scope_stack, global_scope);

    return ctx;
}

void semantic_context_destroy(semantic_context_t* ctx)
{
    if (ctx == nullptr)
        return;

    vec_deinit(&ctx->scope_stack);
    vec_deinit(&ctx->error_nodes);
    free(ctx);
}

void semantic_context_push_scope(semantic_context_t* ctx, scope_kind_t kind)
{
    symbol_table_t* scope = symbol_table_create(ctx->current, kind);
    vec_push(&ctx->scope_stack, scope);
    ctx->current = scope;
}

void semantic_context_pop_scope(semantic_context_t* ctx)
{
    assert(ctx->current != ctx->global);
    symbol_table_destroy(vec_pop(&ctx->scope_stack));
    ctx->current = vec_top(&ctx->scope_stack);
}

void semantic_context_add_error(semantic_context_t* ctx, void* ast_node, const char* description)
{
    compiler_error_create_for_ast(false, description, ast_node);
    vec_push(&ctx->error_nodes, ast_node);
}

void semantic_context_add_warning(semantic_context_t* ctx, void* ast_node, const char* description)
{
    compiler_error_create_for_ast(true, description, ast_node);
    vec_push(&ctx->warning_nodes, ast_node);
}
