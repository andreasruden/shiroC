#include "while_stmt.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdarg.h>
#include <stdlib.h>

static void ast_while_stmt_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_while_stmt_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_while_stmt_destroy(void* self_);

static ast_node_vtable_t ast_while_stmt_vtable =
{
    .accept = ast_while_stmt_accept,
    .accept_transformer = ast_while_stmt_accept_transformer,
    .destroy = ast_while_stmt_destroy
};

ast_stmt_t* ast_while_stmt_create(ast_expr_t* condition, ast_stmt_t* body)
{
    ast_while_stmt_t* while_stmt = malloc(sizeof(*while_stmt));

    *while_stmt = (ast_while_stmt_t){
        .condition = condition,
        .body = body,
    };
    AST_NODE(while_stmt)->vtable = &ast_while_stmt_vtable;
    AST_NODE(while_stmt)->kind = AST_STMT_WHILE;

    return (ast_stmt_t*)while_stmt;
}

static void ast_while_stmt_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_while_stmt_t* self = self_;
    visitor->visit_while_stmt(visitor, self, out);
}

static void* ast_while_stmt_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_while_stmt_t* self = self_;
    return transformer->transform_while_stmt(transformer, self, out);
}

static void ast_while_stmt_destroy(void* self_)
{
    ast_while_stmt_t* self = self_;
    if (self == nullptr)
        return;

    ast_stmt_deconstruct((ast_stmt_t*)self);
    ast_node_destroy(self->condition);
    ast_node_destroy(self->body);
    free(self);
}
