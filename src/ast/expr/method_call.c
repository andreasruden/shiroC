#include "method_call.h"

#include "ast/expr/expr.h"
#include "ast/node.h"
#include "ast/visitor.h"
#include "common/containers/vec.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static void ast_method_call_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_method_call_destroy(void* self_);

static ast_node_vtable_t ast_member_call_vtable =
{
    .accept = ast_method_call_accept,
    .destroy = ast_method_call_destroy
};

ast_expr_t* ast_method_call_create(ast_expr_t* instance, const char* member_name, vec_t* arguments)
{
    ast_method_call_t* member_call = malloc(sizeof(*member_call));

    *member_call = (ast_method_call_t){
        .base = AST_EXPR_INIT,
        .instance = instance,
        .member_name = strdup(member_name),
    };
    vec_move(&member_call->arguments, arguments);
    AST_NODE(member_call)->vtable = &ast_member_call_vtable;
    AST_NODE(member_call)->kind = AST_EXPR_MEMBER_CALL;

    return (ast_expr_t*)member_call;
}

ast_expr_t* ast_method_call_create_va(ast_expr_t* instance, const char* member_name, ...)
{
    vec_t arguments = VEC_INIT(ast_node_destroy);
    va_list args;
    va_start(args, member_name);
    ast_expr_t* expr;
    while ((expr = va_arg(args, ast_expr_t*)) != nullptr) {
        vec_push(&arguments, expr);
    }
    va_end(args);

    return ast_method_call_create(instance, member_name, &arguments);
}

static void ast_method_call_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_method_call_t* self = self_;
    visitor->visit_method_call(visitor, self, out);
}

static void ast_method_call_destroy(void* self_)
{
    ast_method_call_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    ast_node_destroy(self->instance);
    free(self->member_name);
    vec_deinit(&self->arguments);
    free(self);
}
