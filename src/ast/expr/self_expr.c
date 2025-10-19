#include "self_expr.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_self_expr_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_self_expr_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_self_expr_destroy(void* self_);

static ast_node_vtable_t ast_self_expr_vtable =
{
    .accept = ast_self_expr_accept,
    .accept_transformer = ast_self_expr_accept_transformer,
    .destroy = ast_self_expr_destroy
};

ast_expr_t* ast_self_expr_create(bool implicit)
{
    ast_self_expr_t* self_expr = malloc(sizeof(*self_expr));

    *self_expr = (ast_self_expr_t){
        .base = AST_EXPR_INIT,
        .implicit = implicit,
    };
    AST_NODE(self_expr)->vtable = &ast_self_expr_vtable;
    AST_NODE(self_expr)->kind = AST_EXPR_SELF;

    return (ast_expr_t*)self_expr;
}

static void ast_self_expr_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_self_expr_t* self = self_;
    visitor->visit_self_expr(visitor, self, out);
}

static void* ast_self_expr_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_self_expr_t* self = self_;
    return transformer->transform_self_expr(transformer, self, out);
}

static void ast_self_expr_destroy(void* self_)
{
    ast_self_expr_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    free(self);
}
