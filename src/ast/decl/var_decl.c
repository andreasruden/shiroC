#include "var_decl.h"

#include "ast/node.h"
#include "ast/type.h"
#include "ast/visitor.h"

#include <stdlib.h>
#include <string.h>

static void ast_var_decl_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_var_decl_destroy(void* self_);

static ast_node_vtable_t ast_var_decl_vtable =
{
    .accept = ast_var_decl_accept,
    .destroy = ast_var_decl_destroy
};

ast_decl_t* ast_var_decl_create(const char* name, ast_type_t* type, ast_expr_t* init_expr)
{
    ast_var_decl_t* var_decl = malloc(sizeof(*var_decl));

    *var_decl = (ast_var_decl_t){
        .name = strdup(name),
        .type = type,
        .init_expr = init_expr,
    };
    AST_NODE(var_decl)->vtable = &ast_var_decl_vtable;
    AST_NODE(var_decl)->kind = AST_DECL_VAR;

    return (ast_decl_t*)var_decl;
}

ast_var_decl_t* ast_var_decl_create_mandatory(const char* name)
{
    ast_var_decl_t* var_decl = malloc(sizeof(*var_decl));
    *var_decl = (ast_var_decl_t){
        .name = strdup(name),
    };
    AST_NODE(var_decl)->vtable = &ast_var_decl_vtable;
    return var_decl;
}

static void ast_var_decl_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_var_decl_t* self = self_;
    visitor->visit_var_decl(visitor, self, out);
}

static void ast_var_decl_destroy(void* self_)
{
    ast_var_decl_t* self = self_;
    if (self == nullptr)
        return;

    ast_decl_deconstruct((ast_decl_t*)self);
    free(self->name);
    ast_node_destroy(self->init_expr);
    free(self);
}
