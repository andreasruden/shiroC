#include "compound_stmt.h"

#include "ast/node.h"
#include "ast/stmt/stmt.h"
#include "ast/visitor.h"
#include "common/containers/ptr_vec.h"

#include <stdarg.h>
#include <stdlib.h>

static void ast_compound_stmt_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_compound_stmt_destroy(void* self_);

static ast_node_vtable_t ast_compound_stmt_vtable =
{
    .accept = ast_compound_stmt_accept,
    .destroy = ast_compound_stmt_destroy
};

ast_stmt_t* ast_compound_stmt_create(ptr_vec_t* inner_stmts)
{
    ast_compound_stmt_t* compound_stmt = malloc(sizeof(*compound_stmt));

    *compound_stmt = (ast_compound_stmt_t){};
    ptr_vec_move(&compound_stmt->inner_stmts, inner_stmts);
    AST_NODE(compound_stmt)->vtable = &ast_compound_stmt_vtable;
    AST_NODE(compound_stmt)->kind = AST_STMT_COMPOUND;

    return (ast_stmt_t*)compound_stmt;
}

ast_stmt_t* ast_compound_stmt_create_va(ast_stmt_t* first, ...)
{
    ptr_vec_t stmts = PTR_VEC_INIT;
    if (first != nullptr)
        ptr_vec_append(&stmts, first);

    va_list args;
    va_start(args, first);
    ast_stmt_t* def;
    while ((def = va_arg(args, ast_stmt_t*)) != nullptr) {
        ptr_vec_append(&stmts, def);
    }
    va_end(args);

    return ast_compound_stmt_create(&stmts);
}

ast_stmt_t* ast_compound_stmt_create_empty()
{
    ptr_vec_t stmts = PTR_VEC_INIT;
    return ast_compound_stmt_create(&stmts);
}

static void ast_compound_stmt_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_compound_stmt_t* self = self_;
    visitor->visit_compound_stmt(visitor, self, out);
}

static void ast_compound_stmt_destroy(void* self_)
{
    ast_compound_stmt_t* self = self_;
    if (self == nullptr)
        return;

    ast_stmt_deconstruct((ast_stmt_t*)self);
    ptr_vec_deinit(&self->inner_stmts, ast_node_destroy);
    free(self);
}
