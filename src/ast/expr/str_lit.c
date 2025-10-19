#include "str_lit.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>
#include <string.h>

static void ast_str_lit_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_str_lit_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_str_lit_destroy(void* self_);

static ast_node_vtable_t ast_str_lit_vtable =
{
    .accept = ast_str_lit_accept,
    .accept_transformer = ast_str_lit_accept_transformer,
    .destroy = ast_str_lit_destroy
};

ast_expr_t* ast_str_lit_create(const char* value)
{
    ast_str_lit_t* str_lit = malloc(sizeof(*str_lit));

    *str_lit = (ast_str_lit_t){
        .base = AST_EXPR_INIT,
        .value = strdup(value),
    };
    AST_NODE(str_lit)->vtable = &ast_str_lit_vtable;
    AST_NODE(str_lit)->kind = AST_EXPR_STR_LIT;

    return (ast_expr_t*)str_lit;
}

static void ast_str_lit_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_str_lit_t* self = self_;
    visitor->visit_str_lit(visitor, self, out);
}

static void* ast_str_lit_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_str_lit_t* self = self_;
    return transformer->transform_str_lit(transformer, self, out);
}

static void ast_str_lit_destroy(void* self_)
{
    ast_str_lit_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    free(self->value);
    free(self);
}
