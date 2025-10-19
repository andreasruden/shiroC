#include "method_def.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/type.h"
#include "ast/visitor.h"
#include "common/containers/vec.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static void ast_method_def_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_method_def_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_method_def_destroy(void* self_);

static ast_node_vtable_t ast_method_def_vtable =
{
    .accept = ast_method_def_accept,
    .accept_transformer = ast_method_def_accept_transformer,
    .destroy = ast_method_def_destroy
};

ast_def_t* ast_method_def_create_from(ast_fn_def_t* fn_def)
{
    ast_type_t* ret_type = fn_def->return_type;

    ast_stmt_t* body = fn_def->body;
    fn_def->body = nullptr;

    ast_def_t* method_def = ast_method_def_create(fn_def->base.name, &fn_def->params, ret_type, body);
    ast_node_destroy(fn_def);
    return method_def;
}

ast_def_t* ast_method_def_create(const char* name, vec_t* params, ast_type_t* ret_type, ast_stmt_t* body)
{
    ast_method_def_t* method_def = malloc(sizeof(*method_def));

    *method_def = (ast_method_def_t){
        .base.base.name = strdup(name),
        .base.return_type = ret_type,
        .base.body = body,
    };
    vec_move(&method_def->base.params, params);
    AST_NODE(method_def)->vtable = &ast_method_def_vtable;
    AST_NODE(method_def)->kind = AST_DEF_METHOD;

    return (ast_def_t*)method_def;
}

ast_def_t* ast_method_def_create_va(const char* name, ast_type_t* ret_type, ast_stmt_t* body, ...)
{
    vec_t params = VEC_INIT(ast_node_destroy);
    va_list args;
    va_start(args, body);
    ast_param_decl_t* decl;
    while ((decl = va_arg(args, ast_param_decl_t*)) != nullptr) {
        vec_push(&params, decl);
    }
    va_end(args);

    return ast_method_def_create(name, &params, ret_type, body);
}

static void ast_method_def_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_method_def_t* self = self_;
    visitor->visit_method_def(visitor, self, out);
}

static void ast_method_def_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_method_def_t** self = self_;
    transformer->transform_method_def(transformer, self, out);
}

static void ast_method_def_destroy(void* self_)
{
    ast_method_def_t* self = self_;
    if (self == nullptr)
        return;

    ast_def_deconstruct((ast_def_t*)self);
    vec_deinit(&self->base.params);
    ast_node_destroy(self->base.body);
    free(self);
}
