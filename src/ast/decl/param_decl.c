#include "param_decl.h"

#include "ast/decl/decl.h"
#include "ast/visitor.h"

#include <stdlib.h>
#include <string.h>

static void ast_param_decl_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_param_decl_destroy(void* self_);

static ast_node_vtable_t ast_param_decl_vtable =
{
    .accept = ast_param_decl_accept,
    .destroy = ast_param_decl_destroy
};

ast_decl_t* ast_param_decl_create(const char* name, const char* type)
{
    ast_param_decl_t* param_decl = malloc(sizeof(*param_decl));

    *param_decl = (ast_param_decl_t){
        .name = strdup(name),
        .type = strdup(type),
    };
    AST_NODE(param_decl)->vtable = &ast_param_decl_vtable;

    return (ast_decl_t*)param_decl;
}

static void ast_param_decl_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_param_decl_t* self = self_;
    visitor->visit_param_decl(visitor, self, out);
}

static void ast_param_decl_destroy(void* self_)
{
    ast_param_decl_t* self = self_;
    if (self == nullptr)
        return;

    ast_decl_deconstruct((ast_decl_t*)self);
    free(self->name);
    free(self->type);
    free(self);
}
