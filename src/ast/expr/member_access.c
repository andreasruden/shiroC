#include "member_access.h"

#include "ast/expr/expr.h"
#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>
#include <string.h>

static void ast_member_access_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_member_access_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_member_access_destroy(void* self_);

static ast_node_vtable_t ast_member_access_vtable =
{
    .accept = ast_member_access_accept,
    .accept_transformer = ast_member_access_accept_transformer,
    .destroy = ast_member_access_destroy
};

ast_expr_t* ast_member_access_create(ast_expr_t* instance, const char* member_name)
{
    ast_member_access_t* member_access = malloc(sizeof(*member_access));

    *member_access = (ast_member_access_t){
        .base = AST_EXPR_INIT,
        .instance = instance,
        .member_name = strdup(member_name),
    };
    AST_NODE(member_access)->vtable = &ast_member_access_vtable;
    AST_NODE(member_access)->kind = AST_EXPR_MEMBER_ACCESS;

    return (ast_expr_t*)member_access;
}

static void ast_member_access_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_member_access_t* self = self_;
    visitor->visit_member_access(visitor, self, out);
}

static void ast_member_access_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_member_access_t** self = self_;
    transformer->transform_member_access(transformer, self, out);
}

static void ast_member_access_destroy(void* self_)
{
    ast_member_access_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    ast_node_destroy(self->instance);
    free(self->member_name);
    free(self);
}
