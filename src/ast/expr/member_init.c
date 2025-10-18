#include "member_init.h"

#include "ast/expr/expr.h"
#include "ast/node.h"
#include "ast/visitor.h"

#include <stdlib.h>
#include <string.h>

static void ast_member_init_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_member_init_destroy(void* self_);

static ast_node_vtable_t ast_member_init_vtable =
{
    .accept = ast_member_init_accept,
    .destroy = ast_member_init_destroy
};

ast_member_init_t* ast_member_init_create(const char* member_name, ast_expr_t* init_expr)
{
    ast_member_init_t* member_init = malloc(sizeof(*member_init));

    *member_init = (ast_member_init_t){
        .base = (ast_node_t){},
        .member_name = strdup(member_name),
        .init_expr = init_expr,
    };
    AST_NODE(member_init)->vtable = &ast_member_init_vtable;
    AST_NODE(member_init)->kind = AST_EXPR_MEMBER_INIT;

    return member_init;
}

static void ast_member_init_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_member_init_t* self = self_;
    visitor->visit_member_init(visitor, self, out);
}

static void ast_member_init_destroy(void* self_)
{
    ast_member_init_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    free(self->member_name);
    ast_node_destroy(self->init_expr);
    free(self);
}
