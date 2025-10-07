#ifndef AST_VISITOR__H
#define AST_VISITOR__H

#include "ast/decl/param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/paren_expr.h"
#include "ast/expr/ref_expr.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/def/fn_def.h"
#include "ast/expr/int_lit.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/if_stmt.h"
#include "ast/stmt/return_stmt.h"

/* Visitor for any AST node.

The ast_visitor needs to be inherited from (put it at the start of a child struct),
and then function pointers for node types that we want to visit need to be setup.
Any node-type that is visited via a user-provided function pointer leaves traversal
up to the user. If a node is visited, but the user has not provided any implementation,
then the default is to visit every child.
*/
typedef struct ast_visitor ast_visitor_t;
struct ast_visitor
{
    void (*visit_root)(void* self_, ast_root_t* root, void *out_);

    // Declarations
    void (*visit_param_decl)(void* self, ast_param_decl_t* fn_def, void *out_);
    void (*visit_var_decl)(void* self_, ast_var_decl_t* var_decl, void *out_);

    // Definitions
    void (*visit_fn_def)(void* self_, ast_fn_def_t* fn_def, void *out_);

    // Expressions
    void (*visit_bin_op)(void* self_, ast_bin_op_t* bin_op, void *out_);
    void (*visit_call_expr)(void* self_, ast_call_expr_t* call_expr, void *out_);
    void (*visit_int_lit)(void* self_, ast_int_lit_t* int_lit, void *out_);
    void (*visit_paren_expr)(void* self_, ast_paren_expr_t* paren_expr, void *out_);
    void (*visit_ref_expr)(void* self_, ast_ref_expr_t* ref_expr, void *out_);

    // Statements
    void (*visit_compound_stmt)(void* self_, ast_compound_stmt_t* compound_stmt, void *out_);
    void (*visit_decl_stmt)(void* self_, ast_decl_stmt_t* decl_stmt, void *out_);
    void (*visit_expr_stmt)(void* self_, ast_expr_stmt_t* expr_stmt, void *out_);
    void (*visit_if_stmt)(void* self_, ast_if_stmt_t* if_stmt, void *out_);
    void (*visit_return_stmt)(void* self_, ast_return_stmt_t* return_stmt, void *out_);
};

// Sets up default visitor implementations.
void ast_visitor_init(ast_visitor_t* self);

void ast_visitor_visit(void* self_, void* node_, void* out_);

#endif
