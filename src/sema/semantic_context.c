#include "semantic_context.h"

#include "ast/decl/param_decl.h"
#include "ast/def/fn_def.h"
#include "ast/node.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/type.h"
#include "common/containers/hash_table.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "compiler_error.h"
#include "sema/symbol.h"
#include "sema/symbol_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void register_builtin_methods(semantic_context_t* ctx);

semantic_context_t* semantic_context_create(const char* project_name, const char* module_name)
{
    semantic_context_t* ctx = malloc(sizeof(*ctx));

    symbol_table_t* export_scope = symbol_table_create(nullptr, SCOPE_EXPORT);
    symbol_table_t* global_scope = symbol_table_create(nullptr, SCOPE_GLOBAL);

    // Determine namespace name: use "Self" for own modules, project_name for dependencies
    const char* namespace_name = (project_name != nullptr) ? project_name : "Self";

    symbol_t* self_namespace = symbol_create(namespace_name, SYMBOL_NAMESPACE, nullptr, nullptr);
    self_namespace->type = ast_type_builtin(TYPE_VOID);  // TODO: Unsure what types to give NS symbols
    symbol_table_insert(global_scope, self_namespace);
    symbol_t* module_namespace = symbol_create(module_name, SYMBOL_NAMESPACE, nullptr, self_namespace);
    module_namespace->type = ast_type_builtin(TYPE_VOID);

    *ctx = (semantic_context_t){
        .self_projectname = (project_name != nullptr) ? strdup(project_name) : strdup("Self"),
        .self_namespace = self_namespace,
        .module_namespace = module_namespace,
        .exports = export_scope,
        .global = global_scope,
        .current = global_scope,
        .scope_stack = VEC_INIT(symbol_table_destroy_void),
        .error_nodes = VEC_INIT(nullptr),    // we do not own these nodes
        .warning_nodes = VEC_INIT(nullptr),  // we do not own these nodes
        .builtin_ast_gc = VEC_INIT(ast_node_destroy),
        .imports = VEC_INIT(nullptr),        // we do not own these nodes
    };

    vec_push(&ctx->scope_stack, global_scope);

    register_builtin_methods(ctx);

    return ctx;
}

void semantic_context_destroy(semantic_context_t* ctx)
{
    if (ctx == nullptr)
        return;

    // Note: self_namespace is owned by global_scope (in scope_stack), so it will be destroyed there
    // Note: module_namespace is only in self_namespace->exports (SCOPE_EXPORT), which doesn't own symbols
    //       So we must explicitly destroy it here
    symbol_destroy(ctx->module_namespace);
    symbol_table_destroy(ctx->exports);
    free(ctx->self_projectname);
    vec_deinit(&ctx->scope_stack);
    vec_deinit(&ctx->error_nodes);
    vec_deinit(&ctx->warning_nodes);
    vec_deinit(&ctx->builtin_ast_gc);
    vec_deinit(&ctx->imports);
    for (size_t i = 0; i < TYPE_END; ++i)
        symbol_table_destroy(ctx->builtin_methods[i]);
    symbol_table_destroy(ctx->array_methods);
    symbol_table_destroy(ctx->view_methods);
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
    symbol_t* param_symbol = symbol_create(param->name, SYMBOL_PARAMETER, param, nullptr);
    param_symbol->type = param->type;

    // Register printI32(i32) -> void
    ast_def_t* fn_def = ast_fn_def_create_va("printI32", ast_type_builtin(TYPE_VOID),
        ast_compound_stmt_create_empty(), param, nullptr);
    vec_push(&ctx->builtin_ast_gc, fn_def);

    symbol_t* print_i32 = symbol_create("printI32", SYMBOL_FUNCTION, fn_def, nullptr);
    print_i32->type = ast_type_invalid();  // FIXME: should be function signature type
    print_i32->data.function.return_type = ast_type_builtin(TYPE_VOID);
    vec_push(&print_i32->data.function.parameters, param_symbol);

    symbol_table_insert(ctx->global, print_i32);
}

static void inject_symbols_into_namespace(symbol_t* namespace, symbol_table_t* symbols)
{
    panic_if(namespace->kind != SYMBOL_NAMESPACE);

    hash_table_iter_t itr;
    for (hash_table_iter_init(&itr, &symbols->map); hash_table_iter_has_elem(&itr); hash_table_iter_next(&itr))
    {
        vec_t* overloads = hash_table_iter_current(&itr)->value;
        for (size_t i = 0; i < vec_size(overloads); ++i)
        {
            symbol_t* symb = vec_get(overloads, i);
            symbol_table_insert(namespace->data.namespace.exports, symb);  // exports have no ownership
        }
    }

}

symbol_t* semantic_context_register_namespace(semantic_context_t* ctx, symbol_t* parent_namespace, const char* name,
    symbol_table_t* symbols)
{
    symbol_table_t* insert_into = (parent_namespace == nullptr) ?
        ctx->global : parent_namespace->data.namespace.exports;

    // Ignore namespace insertion if already inserted before (e.g. project namespace)
    vec_t* prev = symbol_table_overloads(insert_into, name);
    if (prev && vec_size(prev) > 0)
    {
        for (size_t i = 0; i < vec_size(prev); ++i)
        {
            symbol_t* prev_ns = vec_get(prev, i);
            if (prev_ns->kind == SYMBOL_NAMESPACE)
            {
                if (symbols)
                    inject_symbols_into_namespace(prev_ns, symbols);
                return prev_ns;
            }
        }
    }

    symbol_t* ns_symbol = symbol_create(name, SYMBOL_NAMESPACE, nullptr, parent_namespace);
    if (symbols)
        inject_symbols_into_namespace(ns_symbol, symbols);
    symbol_table_insert(ctx->global, ns_symbol);

    return ns_symbol;
}

symbol_table_t* semantic_context_builtin_methods_for_type(semantic_context_t* ctx, ast_type_t* type)
{
    switch (type->kind)
    {
        case AST_TYPE_BUILTIN:
            return ctx->builtin_methods[type->data.builtin.type];
        case AST_TYPE_ARRAY:
            return ctx->array_methods;
        case AST_TYPE_VIEW:
            return ctx->view_methods;
        default:
            return nullptr;
    }
}

static void register_builtin_methods(semantic_context_t* ctx)
{
    // TYPE_STRING methods
    symbol_table_t* string = symbol_table_create(nullptr, SCOPE_CLASS);
    symbol_t* str_len_method = symbol_create("len", SYMBOL_METHOD, nullptr, nullptr);
    str_len_method->data.function.return_type = ast_type_builtin(TYPE_USIZE);
    str_len_method->data.function.is_builtin = true;
    symbol_t* str_raw_method = symbol_create("raw", SYMBOL_METHOD, nullptr, nullptr);
    str_raw_method->data.function.return_type = ast_type_pointer(ast_type_builtin(TYPE_U8));
    str_raw_method->data.function.is_builtin = true;
    symbol_table_insert(string, str_len_method);
    symbol_table_insert(string, str_raw_method);
    ctx->builtin_methods[TYPE_STRING] = string;

    // ARRAY methods
    symbol_table_t* array = symbol_table_create(nullptr, SCOPE_CLASS);
    symbol_t* arr_len_method = symbol_create("len", SYMBOL_METHOD, nullptr, nullptr);
    arr_len_method->data.function.return_type = ast_type_builtin(TYPE_USIZE);
    arr_len_method->data.function.is_builtin = true;
    symbol_table_insert(array, arr_len_method);
    ctx->array_methods = array;

    // VIEW methods
    symbol_table_t* view = symbol_table_create(nullptr, SCOPE_CLASS);
    symbol_t* view_len_method = symbol_create("len", SYMBOL_METHOD, nullptr, nullptr);
    view_len_method->data.function.return_type = ast_type_builtin(TYPE_USIZE);
    view_len_method->data.function.is_builtin = true;
    symbol_table_insert(view, view_len_method);
    ctx->view_methods = view;
}
