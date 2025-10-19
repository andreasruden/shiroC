#include "param_decl.h"

#include "ast/decl/decl.h"
#include "ast/transformer.h"
#include "ast/type.h"
#include "ast/visitor.h"

#include <stdlib.h>
#include <string.h>

static void ast_param_decl_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_param_decl_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_param_decl_destroy(void* self_);

static ast_node_vtable_t ast_param_decl_vtable =
{
    .accept = ast_param_decl_accept,
    .accept_transformer = ast_param_decl_accept_transformer,
    .destroy = ast_param_decl_destroy
};

ast_decl_t* ast_param_decl_create(const char* name, ast_type_t* type)
{
    ast_param_decl_t* param_decl = malloc(sizeof(*param_decl));

    *param_decl = (ast_param_decl_t){
        .name = strdup(name),
        .type = type,
    };
    AST_NODE(param_decl)->vtable = &ast_param_decl_vtable;
    AST_NODE(param_decl)->kind = AST_DECL_PARAM;

    return (ast_decl_t*)param_decl;
}

static void ast_param_decl_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_param_decl_t* self = self_;
    visitor->visit_param_decl(visitor, self, out);
}

static void* ast_param_decl_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_param_decl_t* self = self_;
    return transformer->transform_param_decl(transformer, self, out);
}

static void ast_param_decl_destroy(void* self_)
{
    ast_param_decl_t* self = self_;
    if (self == nullptr)
        return;

    ast_decl_deconstruct((ast_decl_t*)self);
    free(self->name);
    free(self);
}
