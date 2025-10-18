#include "member_decl.h"

#include "ast/node.h"
#include "ast/type.h"
#include "ast/visitor.h"

#include <stdlib.h>
#include <string.h>

static void ast_member_decl_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_member_decl_destroy(void* self_);

static ast_node_vtable_t ast_member_decl_vtable =
{
    .accept = ast_member_decl_accept,
    .destroy = ast_member_decl_destroy
};

ast_decl_t* ast_member_decl_create_from(ast_var_decl_t* var)
{
    ast_expr_t* init_expr = var->init_expr;
    var->init_expr = nullptr;
    ast_decl_t* member = ast_member_decl_create(var->name, var->type, init_expr);
    ast_node_destroy(var);
    return member;
}

ast_decl_t* ast_member_decl_create(const char* name, ast_type_t* type, ast_expr_t* init_expr)
{
    ast_member_decl_t* member_decl = malloc(sizeof(*member_decl));

    *member_decl = (ast_member_decl_t){
        .base.name = strdup(name),
        .base.type = type,
        .base.init_expr = init_expr,
    };
    AST_NODE(member_decl)->vtable = &ast_member_decl_vtable;
    AST_NODE(member_decl)->kind = AST_DECL_MEMBER;

    return (ast_decl_t*)member_decl;
}

static void ast_member_decl_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_member_decl_t* self = self_;
    visitor->visit_member_decl(visitor, self, out);
}

static void ast_member_decl_destroy(void* self_)
{
    ast_member_decl_t* self = self_;
    if (self == nullptr)
        return;

    ast_decl_deconstruct((ast_decl_t*)self);
    free(self->base.name);
    ast_node_destroy(self->base.init_expr);
    free(self);
}
