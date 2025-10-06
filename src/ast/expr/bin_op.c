#include "bin_op.h"

#include "ast/visitor.h"

#include <stdlib.h>

static void ast_bin_op_accept(void* self_, ast_visitor_t* visitor, void* out_);
static void ast_bin_op_destroy(void* self_);

static ast_node_vtable_t ast_bin_op_vtable =
{
    .accept = ast_bin_op_accept,
    .destroy = ast_bin_op_destroy
};

ast_expr_t* ast_bin_op_create(token_type_t op, ast_expr_t* lhs, ast_expr_t* rhs)
{
    ast_bin_op_t* expr = calloc(1, sizeof(*expr));

    AST_NODE(expr)->vtable = &ast_bin_op_vtable;
    expr->op = op;
    expr->lhs = lhs;
    expr->rhs = rhs;

    return (ast_expr_t*)expr;
}

static void ast_bin_op_accept(void* self_, ast_visitor_t* visitor, void* out_)
{
    ast_bin_op_t* self = self_;
    visitor->visit_bin_op(visitor, self, out_);
}

static void ast_bin_op_destroy(void* self_)
{
    ast_bin_op_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    if (self->lhs != nullptr)
        ast_node_destroy(self->lhs);
    if (self->rhs != nullptr)
        ast_node_destroy(self->rhs);
    free(self);
}
