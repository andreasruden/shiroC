#include "bin_op.h"

#include "ast/node.h"
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
    ast_bin_op_t* bin_op = malloc(sizeof(*bin_op));

    *bin_op = (ast_bin_op_t){
        .base = AST_EXPR_INIT,
        .op = op,
        .lhs = lhs,
        .rhs = rhs,
    };
    AST_NODE(bin_op)->vtable = &ast_bin_op_vtable;
    AST_NODE(bin_op)->kind = AST_EXPR_BIN_OP;

    return (ast_expr_t*)bin_op;
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
