#ifndef AST_TRANSFORMER__H
#define AST_TRANSFORMER__H

#include "ast/decl/member_decl.h"
#include "ast/decl/param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/class_def.h"
#include "ast/def/fn_def.h"
#include "ast/def/method_def.h"
#include "ast/def/import_def.h"
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
#include "ast/stmt/break_stmt.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/continue_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/for_stmt.h"
#include "ast/stmt/if_stmt.h"
#include "ast/stmt/inc_dec_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "ast/stmt/while_stmt.h"

/* Transformer for any AST node.

This works like visitor.h, except it returns a potentially new node (if the node was replaced during
transformation). The transformer may destroy the input node and return a new one, possibly of a different type.
*/
typedef struct ast_transformer ast_transformer_t;
struct ast_transformer
{
    void* (*transform_root)(void* self_, ast_root_t* root, void *out_);

    // Declarations
    void* (*transform_member_decl)(void* self_, ast_member_decl_t* member_decl, void *out_);
    void* (*transform_param_decl)(void* self, ast_param_decl_t* fn_def, void *out_);
    void* (*transform_var_decl)(void* self_, ast_var_decl_t* var_decl, void *out_);

    // Definitions
    void* (*transform_class_def)(void* self_, ast_class_def_t* class_def, void *out_);
    void* (*transform_fn_def)(void* self_, ast_fn_def_t* fn_def, void *out_);
    void* (*transform_method_def)(void* self_, ast_method_def_t* method_def, void *out_);
    void* (*transform_import_def)(void* self_, ast_import_def_t* import_def, void *out_);

    // Expressions
    void* (*transform_array_lit)(void* self_, ast_array_lit_t* lit, void *out_);
    void* (*transform_array_slice)(void* self_, ast_array_slice_t* array_slice, void *out_);
    void* (*transform_array_subscript)(void* self_, ast_array_subscript_t* array_subscript, void *out_);
    void* (*transform_bin_op)(void* self_, ast_bin_op_t* bin_op, void *out_);
    void* (*transform_bool_lit)(void* self_, ast_bool_lit_t* bool_lit, void *out_);
    void* (*transform_call_expr)(void* self_, ast_call_expr_t* call_expr, void *out_);
    void* (*transform_cast_expr)(void* self_, ast_cast_expr_t* cast, void *out_);
    void* (*transform_coercion_expr)(void* self_, ast_coercion_expr_t* coercion, void *out_);
    void* (*transform_construct_expr)(void* self_, ast_construct_expr_t* construct_expr, void *out_);
    void* (*transform_float_lit)(void* self_, ast_float_lit_t* float_lit, void *out_);
    void* (*transform_int_lit)(void* self_, ast_int_lit_t* int_lit, void *out_);
    void* (*transform_member_access)(void* self_, ast_member_access_t* member_access, void *out_);
    void* (*transform_member_init)(void* self_, ast_member_init_t* member_init, void *out_);
    void* (*transform_method_call)(void* self_, ast_method_call_t* member_call, void *out_);
    void* (*transform_null_lit)(void* self_, ast_null_lit_t* null_lit, void *out_);
    void* (*transform_paren_expr)(void* self_, ast_paren_expr_t* paren_expr, void *out_);
    void* (*transform_ref_expr)(void* self_, ast_ref_expr_t* ref_expr, void *out_);
    void* (*transform_self_expr)(void* self_, ast_self_expr_t* self_expr, void *out_);
    void* (*transform_str_lit)(void* self_, ast_str_lit_t* str_lit, void *out_);
    void* (*transform_unary_op)(void* self_, ast_unary_op_t* unary_op, void *out_);
    void* (*transform_uninit_lit)(void* self_, ast_uninit_lit_t* uninit_lit, void *out_);

    // Statements
    void* (*transform_break_stmt)(void* self_, ast_break_stmt_t* break_stmt, void *out_);
    void* (*transform_compound_stmt)(void* self_, ast_compound_stmt_t* compound_stmt, void *out_);
    void* (*transform_continue_stmt)(void* self_, ast_continue_stmt_t* continue_stmt, void *out_);
    void* (*transform_decl_stmt)(void* self_, ast_decl_stmt_t* decl_stmt, void *out_);
    void* (*transform_expr_stmt)(void* self_, ast_expr_stmt_t* expr_stmt, void *out_);
    void* (*transform_for_stmt)(void* self_, ast_for_stmt_t* for_stmt, void *out_);
    void* (*transform_if_stmt)(void* self_, ast_if_stmt_t* if_stmt, void *out_);
    void* (*transform_inc_dec_stmt)(void* self_, ast_inc_dec_stmt_t* inc_dec_stmt, void *out_);
    void* (*transform_return_stmt)(void* self_, ast_return_stmt_t* return_stmt, void *out_);
    void* (*transform_while_stmt)(void* self_, ast_while_stmt_t* while_stmt, void *out_);
};

// Sets up default transformer implementations.
void ast_transformer_init(ast_transformer_t* self);

// Transforms a node, potentially replacing it with a different node (possibly of a different type).
// Returns the resulting node (may be the same as input, or a new node if replaced).
// If replaced, the transformer is responsible for destroying the old node.
void* ast_transformer_transform(void* self_, void* node, void* out_);

// Helper macro for transforming vector elements with ast_transformer_transform
#define AST_TRANSFORMER_TRANSFORM_VEC(transformer, vec_ptr, index, out) \
    do { \
        ast_node_t* _old_node = vec_get(vec_ptr, index); \
        ast_node_t* _new_node = ast_transformer_transform(transformer, _old_node, out); \
        if (_new_node != _old_node) { \
            vec_replace(vec_ptr, index, _new_node); \
        } \
    } while(0)

#endif
