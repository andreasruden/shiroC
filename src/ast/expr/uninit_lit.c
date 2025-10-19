#include "uninit_lit.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_uninit_lit_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_uninit_lit_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_uninit_lit_destroy(void* self_);

static ast_node_vtable_t ast_uninit_lit_vtable =
{
    .accept = ast_uninit_lit_accept,
    .accept_transformer = ast_uninit_lit_accept_transformer,
    .destroy = ast_uninit_lit_destroy
};

ast_expr_t* ast_uninit_lit_create()
{
    ast_uninit_lit_t* uninit_lit = malloc(sizeof(*uninit_lit));

    *uninit_lit = (ast_uninit_lit_t){
        .base = AST_EXPR_INIT,
    };
    AST_NODE(uninit_lit)->vtable = &ast_uninit_lit_vtable;
    AST_NODE(uninit_lit)->kind = AST_EXPR_UNINIT_LIT;

    return (ast_expr_t*)uninit_lit;
}

static void ast_uninit_lit_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_uninit_lit_t* self = self_;
    visitor->visit_uninit_lit(visitor, self, out);
}

static void ast_uninit_lit_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_uninit_lit_t** self = self_;
    transformer->transform_uninit_lit(transformer, self, out);
}

static void ast_uninit_lit_destroy(void* self_)
{
    ast_uninit_lit_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    free(self);
}
