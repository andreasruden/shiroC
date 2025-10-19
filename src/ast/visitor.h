#ifndef AST_VISITOR__H
#define AST_VISITOR__H

#include "ast/decl/member_decl.h"
#include "ast/decl/param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/class_def.h"
#include "ast/def/fn_def.h"
#include "ast/def/method_def.h"
#include "ast/expr/array_lit.h"
#include "ast/expr/array_slice.h"
#include "ast/expr/array_subscript.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/bool_lit.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/cast_expr.h"
#include "ast/expr/coercion_expr.h"
#include "ast/expr/construct_expr.h"
#include "ast/expr/float_lit.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/member_access.h"
#include "ast/expr/method_call.h"
#include "ast/expr/member_init.h"
#include "ast/expr/null_lit.h"
#include "ast/expr/paren_expr.h"
#include "ast/expr/ref_expr.h"
#include "ast/expr/self_expr.h"
#include "ast/expr/str_lit.h"
#include "ast/expr/unary_op.h"
#include "ast/expr/uninit_lit.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/if_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "ast/stmt/while_stmt.h"

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
    void (*visit_member_decl)(void* self_, ast_member_decl_t* member_decl, void *out_);
    void (*visit_param_decl)(void* self, ast_param_decl_t* fn_def, void *out_);
    void (*visit_var_decl)(void* self_, ast_var_decl_t* var_decl, void *out_);

    // Definitions
    void (*visit_class_def)(void* self_, ast_class_def_t* class_def, void *out_);
    void (*visit_fn_def)(void* self_, ast_fn_def_t* fn_def, void *out_);
    void (*visit_method_def)(void* self_, ast_method_def_t* method_def, void *out_);

    // Expressions
    void (*visit_array_lit)(void* self_, ast_array_lit_t* lit, void *out_);
    void (*visit_array_slice)(void* self_, ast_array_slice_t* array_slice, void *out_);
    void (*visit_array_subscript)(void* self_, ast_array_subscript_t* array_subscript, void *out_);
    void (*visit_bin_op)(void* self_, ast_bin_op_t* bin_op, void *out_);
    void (*visit_bool_lit)(void* self_, ast_bool_lit_t* bool_lit, void *out_);
    void (*visit_call_expr)(void* self_, ast_call_expr_t* call_expr, void *out_);
    void (*visit_cast_expr)(void* self_, ast_cast_expr_t* cast, void *out_);
    void (*visit_coercion_expr)(void* self_, ast_coercion_expr_t* coercion, void *out_);
    void (*visit_construct_expr)(void* self_, ast_construct_expr_t* construct_expr, void *out_);
    void (*visit_float_lit)(void* self_, ast_float_lit_t* float_lit, void *out_);
    void (*visit_int_lit)(void* self_, ast_int_lit_t* int_lit, void *out_);
    void (*visit_member_access)(void* self_, ast_member_access_t* member_access, void *out_);
    void (*visit_member_init)(void* self_, ast_member_init_t* member_init, void *out_);
    void (*visit_method_call)(void* self_, ast_method_call_t* member_call, void *out_);
    void (*visit_null_lit)(void* self_, ast_null_lit_t* null_lit, void *out_);
    void (*visit_paren_expr)(void* self_, ast_paren_expr_t* paren_expr, void *out_);
    void (*visit_ref_expr)(void* self_, ast_ref_expr_t* ref_expr, void *out_);
    void (*visit_self_expr)(void* self_, ast_self_expr_t* self_expr, void *out_);
    void (*visit_str_lit)(void* self_, ast_str_lit_t* str_lit, void *out_);
    void (*visit_unary_op)(void* self_, ast_unary_op_t* unary_op, void *out_);
    void (*visit_uninit_lit)(void* self_, ast_uninit_lit_t* uninit_lit, void *out_);

    // Statements
    void (*visit_compound_stmt)(void* self_, ast_compound_stmt_t* compound_stmt, void *out_);
    void (*visit_decl_stmt)(void* self_, ast_decl_stmt_t* decl_stmt, void *out_);
    void (*visit_expr_stmt)(void* self_, ast_expr_stmt_t* expr_stmt, void *out_);
    void (*visit_if_stmt)(void* self_, ast_if_stmt_t* if_stmt, void *out_);
    void (*visit_return_stmt)(void* self_, ast_return_stmt_t* return_stmt, void *out_);
    void (*visit_while_stmt)(void* self_, ast_while_stmt_t* while_stmt, void *out_);
};

// Sets up default visitor implementations.
void ast_visitor_init(ast_visitor_t* self);

void ast_visitor_visit(void* self_, void* node_, void* out_);

// Variant of ast_visitor_visit that allows node replacement during traversal.
// The node pointer may be updated to point to a different (replacement) node.
void ast_visitor_transform(void* self_, ast_node_t** node_inout_, void* out_);

// Helper macro for transforming vector elements with ast_visitor_transform
#define AST_VISITOR_TRANSFORM_VEC(visitor, vec_ptr, index, out) \
    do { \
        ast_node_t* _node = vec_get(vec_ptr, index); \
        ast_visitor_transform(visitor, &_node, out); \
        if (_node != vec_get(vec_ptr, index)) { \
            ast_node_destroy(vec_get(vec_ptr, index)); \
            vec_replace(vec_ptr, index, _node); \
        } \
    } while(0)

#endif
