#include "construct_expr.h"

#include "ast/expr/expr.h"
#include "ast/expr/member_init.h"
#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"
#include "common/containers/vec.h"

#include <stdarg.h>
#include <stdlib.h>

static void ast_construct_expr_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_construct_expr_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_construct_expr_destroy(void* self_);

static ast_node_vtable_t ast_construct_expr_vtable =
{
    .accept = ast_construct_expr_accept,
    .accept_transformer = ast_construct_expr_accept_transformer,
    .destroy = ast_construct_expr_destroy
};

ast_expr_t* ast_construct_expr_create(ast_type_t* class_type, vec_t* member_inits)
{
    ast_construct_expr_t* construct_expr = malloc(sizeof(*construct_expr));

    *construct_expr = (ast_construct_expr_t){
        .base = AST_EXPR_INIT,
        .class_type = class_type,
    };
    vec_move(&construct_expr->member_inits, member_inits);
    AST_NODE(construct_expr)->vtable = &ast_construct_expr_vtable;
    AST_NODE(construct_expr)->kind = AST_EXPR_CONSTRUCT;

    return (ast_expr_t*)construct_expr;
}

ast_expr_t* ast_construct_expr_create_va(ast_type_t* class_type, ...)
{
    vec_t member_inits = VEC_INIT(ast_node_destroy);
    va_list args;
    va_start(args, class_type);
    ast_member_init_t* init;
    while ((init = va_arg(args, ast_member_init_t*)) != nullptr) {
        vec_push(&member_inits, init);
    }
    va_end(args);

    return ast_construct_expr_create(class_type, &member_inits);
}

static void ast_construct_expr_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_construct_expr_t* self = self_;
    visitor->visit_construct_expr(visitor, self, out);
}

static void ast_construct_expr_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_construct_expr_t** self = self_;
    transformer->transform_construct_expr(transformer, self, out);
}

static void ast_construct_expr_destroy(void* self_)
{
    ast_construct_expr_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    vec_deinit(&self->member_inits);
    free(self);
}
