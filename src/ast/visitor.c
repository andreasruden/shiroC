#include "visitor.h"
#include "ast/node.h"

static void ast_visitor_visit_root(void* self_, ast_root_t* root, void* out_)
{
    for (size_t i = 0; i < vec_size(&root->tl_defs); ++i)
        ast_visitor_visit(self_, vec_get(&root->tl_defs, i), out_);
}

static void ast_visitor_visit_param_decl(void* self_, ast_param_decl_t* param_decl, void* out_)
{
    (void)self_;
    (void)param_decl;
    (void)out_;
}

static void ast_visitor_visit_var_decl(void* self_, ast_var_decl_t* var_decl, void* out_)
{
    if (var_decl->init_expr != nullptr)
        ast_visitor_visit(self_, var_decl->init_expr, out_);
}

static void ast_visitor_visit_fn_def(void* self_, ast_fn_def_t* fn_def, void* out_)
{
    ast_visitor_visit(self_, fn_def->body, out_);
}

static void ast_visitor_visit_bin_op(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    ast_visitor_visit(self_, bin_op->lhs, out_);
    ast_visitor_visit(self_, bin_op->rhs, out_);
}

static void ast_visitor_visit_call_expr(void* self_, ast_call_expr_t* call_expr, void* out_)
{
    ast_visitor_visit(self_, call_expr->function, out_);

    for (size_t i = 0; i < vec_size(&call_expr->arguments); ++i)
        ast_visitor_visit(self_, vec_get(&call_expr->arguments, i), out_);
}

static void ast_visitor_visit_int_lit(void* self_, ast_int_lit_t* int_lit, void* out_)
{
    (void)self_;
    (void)int_lit;
    (void)out_;
}

static void ast_visitor_visit_paren_expr(void* self_, ast_paren_expr_t* paren_expr, void* out_)
{
    ast_visitor_visit(self_, paren_expr->expr, out_);
}

static void ast_visitor_visit_ref_expr(void* self_, ast_ref_expr_t* ref_expr, void* out_)
{
    (void)self_;
    (void)ref_expr;
    (void)out_;
}

static void ast_visitor_visit_compound_stmt(void* self_, ast_compound_stmt_t* compound_stmt, void* out_)
{
    for (size_t i = 0; i < vec_size(&compound_stmt->inner_stmts); ++i)
        ast_visitor_visit(self_, vec_get(&compound_stmt->inner_stmts, i), out_);
}

static void ast_visitor_visit_decl_stmt(void* self_, ast_decl_stmt_t* decl_stmt, void* out_)
{
    ast_visitor_visit(self_, decl_stmt->decl, out_);
}

static void ast_visitor_visit_expr_stmt(void* self_, ast_expr_stmt_t* expr_stmt, void* out_)
{
    ast_visitor_visit(self_, expr_stmt->expr, out_);
}

static void ast_visitor_visit_if_stmt(void* self_, ast_if_stmt_t* if_stmt, void* out_)
{
    ast_visitor_visit(self_, if_stmt->condition, out_);
    ast_visitor_visit(self_, if_stmt->then_branch, out_);
    if (if_stmt->else_branch != nullptr)
        ast_visitor_visit(self_, if_stmt->else_branch, out_);
}

static void ast_visitor_visit_return_stmt(void* self_, ast_return_stmt_t* return_stmt, void* out_)
{
    ast_visitor_visit(self_, return_stmt->value_expr, out_);
}

static void ast_visitor_visit_while_stmt(void* self_, ast_while_stmt_t* while_stmt, void* out_)
{
    ast_visitor_visit(self_, while_stmt->condition, out_);
    ast_visitor_visit(self_, while_stmt->body, out_);
}

void ast_visitor_init(ast_visitor_t* visitor)
{
    *visitor = (ast_visitor_t){
        .visit_root = ast_visitor_visit_root,
        // Declarations
        .visit_param_decl = ast_visitor_visit_param_decl,
        .visit_var_decl = ast_visitor_visit_var_decl,
        // Definitions
        .visit_fn_def = ast_visitor_visit_fn_def,
        // Expressions
        .visit_bin_op = ast_visitor_visit_bin_op,
        .visit_call_expr = ast_visitor_visit_call_expr,
        .visit_int_lit = ast_visitor_visit_int_lit,
        .visit_paren_expr = ast_visitor_visit_paren_expr,
        .visit_ref_expr = ast_visitor_visit_ref_expr,
        // Statements
        .visit_compound_stmt = ast_visitor_visit_compound_stmt,
        .visit_decl_stmt = ast_visitor_visit_decl_stmt,
        .visit_expr_stmt = ast_visitor_visit_expr_stmt,
        .visit_if_stmt = ast_visitor_visit_if_stmt,
        .visit_return_stmt = ast_visitor_visit_return_stmt,
        .visit_while_stmt = ast_visitor_visit_while_stmt,
    };
}

void ast_visitor_visit(void* self_, void* node_, void* out_)
{
    ast_visitor_t* visitor = self_;
    ast_node_t* node = node_;
    node->vtable->accept(node_, visitor, out_);
}
