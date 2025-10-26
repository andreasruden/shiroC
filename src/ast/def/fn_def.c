#include "fn_def.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/type.h"
#include "ast/visitor.h"
#include "common/containers/vec.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static void ast_fn_def_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_fn_def_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_fn_def_destroy(void* self_);

static ast_node_vtable_t ast_fn_def_vtable =
{
    .accept = ast_fn_def_accept,
    .accept_transformer = ast_fn_def_accept_transformer,
    .destroy = ast_fn_def_destroy
};

ast_def_t* ast_fn_def_create(const char* name, vec_t* params, ast_type_t* ret_type, ast_stmt_t* body, bool exported)
{
    ast_fn_def_t* fn_def = malloc(sizeof(*fn_def));

    *fn_def = (ast_fn_def_t){
        .base.name = strdup(name),
        .return_type = ret_type,
        .body = body,
        .exported = exported,
    };
    vec_move(&fn_def->params, params);
    AST_NODE(fn_def)->vtable = &ast_fn_def_vtable;
    AST_NODE(fn_def)->kind = AST_DEF_FN;

    return (ast_def_t*)fn_def;
}

ast_def_t* ast_fn_def_create_va(const char* name, ast_type_t* ret_type, ast_stmt_t* body, ...)
{
    vec_t params = VEC_INIT(ast_node_destroy);
    va_list args;
    va_start(args, body);
    ast_param_decl_t* decl;
    while ((decl = va_arg(args, ast_param_decl_t*)) != nullptr) {
        vec_push(&params, decl);
    }
    va_end(args);

    return ast_fn_def_create(name, &params, ret_type, body, false);
}

static void ast_fn_def_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_fn_def_t* self = self_;
    visitor->visit_fn_def(visitor, self, out);
}

static void* ast_fn_def_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_fn_def_t* self = self_;
    return transformer->transform_fn_def(transformer, self, out);
}

static void ast_fn_def_destroy(void* self_)
{
    ast_fn_def_t* self = self_;
    if (self == nullptr)
        return;

    ast_def_deconstruct((ast_def_t*)self);
    vec_deinit(&self->params);
    ast_node_destroy(self->body);
    free(self->extern_abi);
    free(self);
}
