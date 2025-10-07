#include "expr_stmt.h"

#include "ast/node.h"
#include "ast/stmt/stmt.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_expr_stmt_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_expr_stmt_destroy(void* self_);

static ast_node_vtable_t ast_expr_stmt_vtable =
{
    .accept = ast_expr_stmt_accept,
    .destroy = ast_expr_stmt_destroy
};

ast_stmt_t* ast_expr_stmt_create(ast_expr_t* expr)
{
    ast_expr_stmt_t* expr_stmt = malloc(sizeof(*expr_stmt));

    *expr_stmt = (ast_expr_stmt_t){
        .expr = expr
    };
    AST_NODE(expr_stmt)->vtable = &ast_expr_stmt_vtable;
    AST_NODE(expr_stmt)->kind = AST_STMT_EXPR;

    return (ast_stmt_t*)expr_stmt;
}

static void ast_expr_stmt_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_expr_stmt_t* self = self_;
    visitor->visit_expr_stmt(visitor, self, out);
}

static void ast_expr_stmt_destroy(void* self_)
{
    ast_expr_stmt_t* self = (ast_expr_stmt_t*)self_;
    if (self == nullptr)
        return;

    ast_stmt_deconstruct((ast_stmt_t*)self);
    ast_node_destroy(self->expr);
    free(self);
}
