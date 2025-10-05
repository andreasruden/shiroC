#include "call_expr.h"

#include "ast/expr/expr.h"
#include "ast/node.h"
#include "ast/visitor.h"
#include "common/containers/ptr_vec.h"

#include <stdarg.h>
#include <stdlib.h>

static void ast_call_expr_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_call_expr_destroy(void* self_);

static ast_node_vtable_t ast_call_expr_vtable =
{
    .accept = ast_call_expr_accept,
    .destroy = ast_call_expr_destroy
};

ast_expr_t* ast_call_expr_create(ast_expr_t* function, ptr_vec_t* arguments)
{
    ast_call_expr_t* call_expr = calloc(1, sizeof(*call_expr));

    AST_NODE(call_expr)->vtable = &ast_call_expr_vtable;
    call_expr->function = function;
    ptr_vec_move(&call_expr->arguments, arguments);

    return (ast_expr_t*)call_expr;
}

ast_expr_t* ast_call_expr_create_va(ast_expr_t* function, ...)
{
    ptr_vec_t arglist = PTR_VEC_INIT;
    va_list args;
    va_start(args, function);
    ast_expr_t* arg_expr;
    while ((arg_expr = va_arg(args, ast_expr_t*)) != nullptr) {
        ptr_vec_append(&arglist, arg_expr);
    }
    va_end(args);

    return ast_call_expr_create(function, &arglist);
}

static void ast_call_expr_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_call_expr_t* self = self_;
    visitor->visit_call_expr(visitor, self, out);
}

static void ast_call_expr_destroy(void* self_)
{
    ast_call_expr_t* self = self_;

    if (self != nullptr)
    {
        ast_expr_deconstruct((ast_expr_t*)self);
        ast_node_destroy(self->function);
        ptr_vec_deinit(&self->arguments, ast_node_destroy);
        free(self);
    }
}
