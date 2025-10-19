#include "float_lit.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>
#include <string.h>

static void ast_float_lit_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_float_lit_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_float_lit_destroy(void* self_);

static ast_node_vtable_t ast_float_lit_vtable =
{
    .accept = ast_float_lit_accept,
    .accept_transformer = ast_float_lit_accept_transformer,
    .destroy = ast_float_lit_destroy
};

ast_expr_t* ast_float_lit_create(double value, const char* suffix)
{
    ast_float_lit_t* float_lit = malloc(sizeof(*float_lit));

    *float_lit = (ast_float_lit_t){
        .base = AST_EXPR_INIT,
        .value = value,
        .suffix = strdup(suffix),
    };
    AST_NODE(float_lit)->vtable = &ast_float_lit_vtable;
    AST_NODE(float_lit)->kind = AST_EXPR_FLOAT_LIT;

    return (ast_expr_t*)float_lit;
}

static void ast_float_lit_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_float_lit_t* self = self_;
    visitor->visit_float_lit(visitor, self, out);
}

static void ast_float_lit_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_float_lit_t** self = self_;
    transformer->transform_float_lit(transformer, self, out);
}

static void ast_float_lit_destroy(void* self_)
{
    ast_float_lit_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    free(self->suffix);
    free(self);
}
