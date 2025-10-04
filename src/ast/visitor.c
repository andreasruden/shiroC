#include "visitor.h"
#include "ast/node.h"

static void ast_visitor_visit_root(void* self_, ast_root_t* root, void* out_)
{
    for (size_t i = 0; i < ptr_vec_size(&root->tl_defs); ++i)
        ast_visitor_visit(self_, AST_NODE(ptr_vec_get(&root->tl_defs, i)), out_);
}

static void ast_visitor_visit_fn_def(void* self_, ast_fn_def_t* fn_def, void* out_)
{
    ast_visitor_visit(self_, AST_NODE(fn_def->body), out_);
}

static void ast_visitor_visit_int_lit(void* self_, ast_int_lit_t* int_lit, void* out_)
{
    (void)self_;
    (void)int_lit;
    (void)out_;
}

static void ast_visitor_visit_compound_stmt(void* self_, ast_compound_stmt_t* compound_stmt, void* out_)
{
    for (size_t i = 0; i < ptr_vec_size(&compound_stmt->inner_stmts); ++i)
        ast_visitor_visit(self_, ptr_vec_get(&compound_stmt->inner_stmts, i), out_);
}

static void ast_visitor_visit_return_stmt(void* self_, ast_return_stmt_t* return_stmt, void* out_)
{
    ast_visitor_visit(self_, AST_NODE(return_stmt->value_expr), out_);
}

void ast_visitor_init(ast_visitor_t* visitor)
{
    *visitor = (ast_visitor_t){
        .visit_root = ast_visitor_visit_root,
        // Definitions
        .visit_fn_def = ast_visitor_visit_fn_def,
        // Expressions
        .visit_int_lit = ast_visitor_visit_int_lit,
        // Statements
        .visit_compound_stmt = ast_visitor_visit_compound_stmt,
        .visit_return_stmt = ast_visitor_visit_return_stmt,
    };
}

void ast_visitor_visit(void* self_, void* node_, void* out_)
{
    ast_visitor_t* visitor = self_;
    ast_node_t* node = node_;
    node->vtable->accept(node_, visitor, out_);
}
