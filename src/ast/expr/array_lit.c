#include "array_lit.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"
#include "common/containers/vec.h"

#include <stdarg.h>
#include <stdlib.h>

static void ast_array_lit_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_array_lit_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_array_lit_destroy(void* self_);

static ast_node_vtable_t ast_array_lit_vtable =
{
    .accept = ast_array_lit_accept,
    .accept_transformer = ast_array_lit_accept_transformer,
    .destroy = ast_array_lit_destroy
};

ast_expr_t* ast_array_lit_create(vec_t* exprs)
{
    ast_array_lit_t* array_lit = malloc(sizeof(*array_lit));

    *array_lit = (ast_array_lit_t){
        .base = AST_EXPR_INIT,
    };

    vec_move(&array_lit->exprs, exprs);

    AST_NODE(array_lit)->vtable = &ast_array_lit_vtable;
    AST_NODE(array_lit)->kind = AST_EXPR_ARRAY_LIT;

    return (ast_expr_t*)array_lit;
}

ast_expr_t* ast_array_lit_create_va(ast_expr_t* first, ...)
{
    vec_t exprs = VEC_INIT(ast_node_destroy);
    if (first != nullptr)
        vec_push(&exprs, first);

    va_list args;
    va_start(args, first);
    ast_stmt_t* def;
    while ((def = va_arg(args, ast_stmt_t*)) != nullptr) {
        vec_push(&exprs, def);
    }
    va_end(args);

    return ast_array_lit_create(&exprs);
}

ast_expr_t* ast_array_lit_create_empty()
{
    vec_t empty = VEC_INIT(ast_node_destroy);
    return ast_array_lit_create(&empty);
}

static void ast_array_lit_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_array_lit_t* self = self_;
    visitor->visit_array_lit(visitor, self, out);
}

static void* ast_array_lit_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_array_lit_t* self = self_;
    return transformer->transform_array_lit(transformer, self, out);
}

static void ast_array_lit_destroy(void* self_)
{
    ast_array_lit_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    vec_deinit(&self->exprs);
    free(self);
}
