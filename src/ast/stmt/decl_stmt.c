#include "decl_stmt.h"

#include "ast/decl/decl.h"
#include "ast/node.h"
#include "ast/visitor.h"

#include <stdarg.h>
#include <stdlib.h>

static void ast_decl_stmt_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_decl_stmt_destroy(void* self_);

static ast_node_vtable_t ast_decl_stmt_vtable =
{
    .accept = ast_decl_stmt_accept,
    .destroy = ast_decl_stmt_destroy
};

ast_stmt_t* ast_decl_stmt_create(ast_decl_t* decl)
{
    ast_decl_stmt_t* decl_stmt = malloc(sizeof(*decl_stmt));

    *decl_stmt = (ast_decl_stmt_t){
        .decl = decl
    };
    AST_NODE(decl_stmt)->vtable = &ast_decl_stmt_vtable;
    AST_NODE(decl_stmt)->kind = AST_STMT_DECL;

    return (ast_stmt_t*)decl_stmt;
}

static void ast_decl_stmt_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_decl_stmt_t* self = self_;
    visitor->visit_decl_stmt(visitor, self, out);
}

static void ast_decl_stmt_destroy(void* self_)
{
    ast_decl_stmt_t* self = self_;
    if (self == nullptr)
        return;

    ast_stmt_deconstruct((ast_stmt_t*)self);
    ast_node_destroy(self->decl);
    free(self);
}
