#include "compound_stmt.h"

#include "ast/node.h"
#include "ast/stmt/stmt.h"
#include "ast/visitor.h"
#include "common/containers/ptr_vec.h"

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

    AST_NODE(compound_stmt)->vtable = &ast_compound_stmt_vtable;
    ptr_vec_move(&compound_stmt->inner_stmts, inner_stmts);

    return (ast_stmt_t*)compound_stmt;
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
    for (size_t i = 0; i < ptr_vec_size(&self->inner_stmts); ++i)
        ast_node_destroy(AST_NODE(ptr_vec_get(&self->inner_stmts, i)));
    ptr_vec_deinit(&self->inner_stmts);
    free(self);
}
