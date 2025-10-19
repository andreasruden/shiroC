#include "unary_op.h"

#include "ast/expr/expr.h"
#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"
#include "common/containers/vec.h"

#include <stdarg.h>
#include <stdlib.h>

static void ast_unary_op_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_unary_op_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_unary_op_destroy(void* self_);

static ast_node_vtable_t ast_unary_op_vtable =
{
    .accept = ast_unary_op_accept,
    .accept_transformer = ast_unary_op_accept_transformer,
    .destroy = ast_unary_op_destroy
};

ast_expr_t* ast_unary_op_create(token_type_t op, ast_expr_t* expr)
{
    ast_unary_op_t* unary_op = malloc(sizeof(*unary_op));

    *unary_op = (ast_unary_op_t){
        .base = AST_EXPR_INIT,
        .op = op,
        .expr = expr,
    };
    AST_NODE(unary_op)->vtable = &ast_unary_op_vtable;
    AST_NODE(unary_op)->kind = AST_EXPR_UNARY_OP;

    return (ast_expr_t*)unary_op;
}

static void ast_unary_op_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_unary_op_t* self = self_;
    visitor->visit_unary_op(visitor, self, out);
}

static void* ast_unary_op_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_unary_op_t* self = self_;
    return transformer->transform_unary_op(transformer, self, out);
}

static void ast_unary_op_destroy(void* self_)
{
    ast_unary_op_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    ast_node_destroy(self->expr);
    free(self);
}
