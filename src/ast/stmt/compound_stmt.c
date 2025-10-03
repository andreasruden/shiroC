#include "compound_stmt.h"

#include "ast/node.h"
#include "ast/stmt/stmt.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_compound_stmt_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_compound_stmt_destroy(void* self_);

static ast_node_vtable_t ast_compound_stmt_vtable =
{
    .accept = ast_compound_stmt_accept,
    .destroy = ast_compound_stmt_destroy
};

ast_compound_stmt_t* ast_compound_stmt_create(ast_stmt_t* inner_stmt)
{
    ast_compound_stmt_t* compound_stmt = malloc(sizeof(*compound_stmt));

    AST_NODE(compound_stmt)->vtable = &ast_compound_stmt_vtable;
    compound_stmt->inner_stmts = inner_stmt;

    return compound_stmt;
}

static void ast_compound_stmt_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_compound_stmt_t* self = self_;
    visitor->visit_compound_stmt(visitor, self, out);
}

static void ast_compound_stmt_destroy(void* self_)
{
    ast_compound_stmt_t* self = self_;

    if (self != nullptr)
    {
        ast_stmt_deconstruct((ast_stmt_t*)self);
        free(self);
    }
}
