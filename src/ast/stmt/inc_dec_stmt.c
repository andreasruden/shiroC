#include "inc_dec_stmt.h"

#include "ast/node.h"
#include "ast/stmt/stmt.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_inc_dec_stmt_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_inc_dec_stmt_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_inc_dec_stmt_destroy(void* self_);

static ast_node_vtable_t ast_inc_dec_stmt_vtable =
{
    .accept = ast_inc_dec_stmt_accept,
    .accept_transformer = ast_inc_dec_stmt_accept_transformer,
    .destroy = ast_inc_dec_stmt_destroy
};

ast_stmt_t* ast_inc_dec_stmt_create(ast_expr_t* operand, bool increment)
{
    ast_inc_dec_stmt_t* inc_dec_stmt = malloc(sizeof(*inc_dec_stmt));

    *inc_dec_stmt = (ast_inc_dec_stmt_t){
        .operand = operand,
        .increment = increment,
    };
    AST_NODE(inc_dec_stmt)->vtable = &ast_inc_dec_stmt_vtable;
    AST_NODE(inc_dec_stmt)->kind = AST_STMT_INC_DEC;

    return (ast_stmt_t*)inc_dec_stmt;
}

static void ast_inc_dec_stmt_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_inc_dec_stmt_t* self = self_;
    visitor->visit_inc_dec_stmt(visitor, self, out);
}

static void* ast_inc_dec_stmt_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_inc_dec_stmt_t* self = self_;
    return transformer->transform_inc_dec_stmt(transformer, self, out);
}

static void ast_inc_dec_stmt_destroy(void* self_)
{
    ast_inc_dec_stmt_t* self = (ast_inc_dec_stmt_t*)self_;

    if (self == nullptr)
        return;

    ast_stmt_deconstruct((ast_stmt_t*)self);
    ast_node_destroy(self->operand);
    free(self);
}
