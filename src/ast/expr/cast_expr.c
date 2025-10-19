#include "cast_expr.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_cast_expr_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_cast_expr_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_cast_expr_destroy(void* self_);

static ast_node_vtable_t ast_cast_expr_vtable =
{
    .accept = ast_cast_expr_accept,
    .accept_transformer = ast_cast_expr_accept_transformer,
    .destroy = ast_cast_expr_destroy
};

ast_expr_t* ast_cast_expr_create(ast_expr_t* expr, ast_type_t* target)
{
    ast_cast_expr_t* cast_expr = calloc(1, sizeof(*cast_expr));

    *cast_expr = (ast_cast_expr_t){
        .base = AST_EXPR_INIT,
        .expr = expr,
        .target = target,
    };
    AST_NODE(cast_expr)->vtable = &ast_cast_expr_vtable;
    AST_NODE(cast_expr)->kind = AST_EXPR_CAST;

    return (ast_expr_t*)cast_expr;
}

static void ast_cast_expr_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_cast_expr_t* self = self_;
    visitor->visit_cast_expr(visitor, self, out);
}

static void* ast_cast_expr_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_cast_expr_t* self = self_;
    return transformer->transform_cast_expr(transformer, self, out);
}

static void ast_cast_expr_destroy(void* self_)
{
    ast_cast_expr_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    ast_node_destroy(self->expr);
    free(self);
}
