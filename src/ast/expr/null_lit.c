#include "null_lit.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_null_lit_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_null_lit_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_null_lit_destroy(void* self_);

static ast_node_vtable_t ast_null_lit_vtable =
{
    .accept = ast_null_lit_accept,
    .accept_transformer = ast_null_lit_accept_transformer,
    .destroy = ast_null_lit_destroy
};

ast_expr_t* ast_null_lit_create()
{
    ast_null_lit_t* null_lit = malloc(sizeof(*null_lit));

    *null_lit = (ast_null_lit_t){
        .base = AST_EXPR_INIT,
    };
    AST_NODE(null_lit)->vtable = &ast_null_lit_vtable;
    AST_NODE(null_lit)->kind = AST_EXPR_NULL_LIT;

    return (ast_expr_t*)null_lit;
}

static void ast_null_lit_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_null_lit_t* self = self_;
    visitor->visit_null_lit(visitor, self, out);
}

static void* ast_null_lit_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_null_lit_t* self = self_;
    return transformer->transform_null_lit(transformer, self, out);
}

static void ast_null_lit_destroy(void* self_)
{
    ast_null_lit_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    free(self);
}
