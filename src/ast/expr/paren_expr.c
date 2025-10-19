#include "paren_expr.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_paren_expr_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_paren_expr_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_paren_expr_destroy(void* self_);

static ast_node_vtable_t ast_paren_expr_vtable =
{
    .accept = ast_paren_expr_accept,
    .accept_transformer = ast_paren_expr_accept_transformer,
    .destroy = ast_paren_expr_destroy
};

ast_expr_t* ast_paren_expr_create(ast_expr_t* expr)
{
    ast_paren_expr_t* paren_expr = malloc(sizeof(*paren_expr));

    *paren_expr = (ast_paren_expr_t) {
        .base = AST_EXPR_INIT,
        .expr = expr,
    };
    AST_NODE(paren_expr)->vtable = &ast_paren_expr_vtable;
    AST_NODE(paren_expr)->kind = AST_EXPR_PAREN;

    return (ast_expr_t*)paren_expr;
}

static void ast_paren_expr_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_paren_expr_t* self = self_;
    visitor->visit_paren_expr(visitor, self, out);
}

static void* ast_paren_expr_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_paren_expr_t* self = self_;
    return transformer->transform_paren_expr(transformer, self, out);
}

static void ast_paren_expr_destroy(void* self_)
{
    ast_paren_expr_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    ast_node_destroy(self->expr);
    free(self);
}
