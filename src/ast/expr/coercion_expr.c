#include "coercion_expr.h"

#include "ast/node.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_coercion_expr_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_coercion_expr_destroy(void* self_);

static ast_node_vtable_t ast_coercion_expr_vtable =
{
    .accept = ast_coercion_expr_accept,
    .destroy = ast_coercion_expr_destroy
};

ast_expr_t* ast_coercion_expr_create(ast_expr_t* expr, ast_type_t* target)
{
    ast_coercion_expr_t* coercion_expr = calloc(1, sizeof(*coercion_expr));

    *coercion_expr = (ast_coercion_expr_t){
        .base = AST_EXPR_INIT,
        .expr = expr,
        .target = target,
    };
    AST_NODE(coercion_expr)->vtable = &ast_coercion_expr_vtable;
    AST_NODE(coercion_expr)->kind = AST_EXPR_REF;

    return (ast_expr_t*)coercion_expr;
}

static void ast_coercion_expr_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_coercion_expr_t* self = self_;
    visitor->visit_coercion_expr(visitor, self, out);
}

static void ast_coercion_expr_destroy(void* self_)
{
    ast_coercion_expr_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    ast_node_destroy(self->expr);
    free(self);
}
