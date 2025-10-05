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

ast_decl_t* ast_param_decl_create(const char* type, const char* name)
{
    ast_param_decl_t* param_decl = calloc(1, sizeof(*param_decl));

    AST_NODE(param_decl)->vtable = &ast_param_decl_vtable;
    param_decl->type = strdup(type);
    param_decl->name = strdup(name);

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
    free(self->type);
    free(self->name);
    free(self);
}
