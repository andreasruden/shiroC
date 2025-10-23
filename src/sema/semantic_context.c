#include "semantic_context.h"

#include "ast/decl/param_decl.h"
#include "ast/def/fn_def.h"
#include "ast/node.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/type.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "compiler_error.h"
#include "sema/symbol.h"
#include "sema/symbol_table.h"

semantic_context_t* semantic_context_create()
{
    semantic_context_t* ctx = malloc(sizeof(*ctx));

    symbol_table_t* export_scope = symbol_table_create(nullptr, SCOPE_GLOBAL);
    symbol_table_t* global_scope = symbol_table_create(nullptr, SCOPE_GLOBAL);

    *ctx = (semantic_context_t){
        .export = export_scope,
        .global = global_scope,
        .current = global_scope,
        .scope_stack = VEC_INIT(symbol_table_destroy_void),
        .error_nodes = VEC_INIT(nullptr),    // we do not own these nodes
        .warning_nodes = VEC_INIT(nullptr),  // we do not own these nodes
        .builtin_ast_gc = VEC_INIT(ast_node_destroy),
    };

    vec_push(&ctx->scope_stack, global_scope);

    return ctx;
}

void semantic_context_destroy(semantic_context_t* ctx)
{
    if (ctx == nullptr)
        return;

    symbol_table_destroy(ctx->export);
    vec_deinit(&ctx->scope_stack);
    vec_deinit(&ctx->error_nodes);
    vec_deinit(&ctx->warning_nodes);
    vec_deinit(&ctx->builtin_ast_gc);
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
    panic_if(ctx->current == ctx->global);
    symbol_table_destroy(vec_pop(&ctx->scope_stack));
    ctx->current = vec_last(&ctx->scope_stack);
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

void semantic_context_register_builtins(semantic_context_t* ctx)
{
    // Create parameter: value: i32
    ast_param_decl_t* param = (ast_param_decl_t*)ast_param_decl_create("value", ast_type_builtin(TYPE_I32));

    // Register printI32(i32) -> void
    ast_def_t* fn_def = ast_fn_def_create_va("printI32", ast_type_builtin(TYPE_VOID),
        ast_compound_stmt_create_empty(), param, nullptr);
    vec_push(&ctx->builtin_ast_gc, fn_def);

    symbol_t* print_i32 = symbol_create("printI32", SYMBOL_FUNCTION, fn_def);
    print_i32->type = ast_type_invalid();  // FIXME: should be function signature type
    print_i32->data.function.return_type = ast_type_builtin(TYPE_VOID);
    vec_push(&print_i32->data.function.parameters, param);

    symbol_table_insert(ctx->global, print_i32);
}
