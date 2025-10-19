#include "return_stmt.h"

#include "ast/node.h"
#include "ast/stmt/stmt.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_return_stmt_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_return_stmt_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_return_stmt_destroy(void* self_);

static ast_node_vtable_t ast_return_stmt_vtable =
{
    .accept = ast_return_stmt_accept,
    .accept_transformer = ast_return_stmt_accept_transformer,
    .destroy = ast_return_stmt_destroy
};

ast_stmt_t* ast_return_stmt_create(ast_expr_t* value_expr)
{
    ast_return_stmt_t* return_stmt = malloc(sizeof(*return_stmt));

    *return_stmt = (ast_return_stmt_t){
        .value_expr = value_expr,
    };
    AST_NODE(return_stmt)->vtable = &ast_return_stmt_vtable;
    AST_NODE(return_stmt)->kind = AST_STMT_RETURN;

    return (ast_stmt_t*)return_stmt;
}

static void ast_return_stmt_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_return_stmt_t* self = self_;
    visitor->visit_return_stmt(visitor, self, out);
}

static void* ast_return_stmt_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_return_stmt_t* self = self_;
    return transformer->transform_return_stmt(transformer, self, out);
}

static void ast_return_stmt_destroy(void* self_)
{
    ast_return_stmt_t* self = (ast_return_stmt_t*)self_;

    if (self == nullptr)
        return;

    ast_stmt_deconstruct((ast_stmt_t*)self);
    ast_node_destroy(self->value_expr);
    free(self);
}
