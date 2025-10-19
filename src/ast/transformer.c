#include "transformer.h"
#include "ast/node.h"

static void ast_transformer_transform_root(void* self_, ast_root_t** root, void* out_)
{
    for (size_t i = 0; i < vec_size(&(*root)->tl_defs); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &(*root)->tl_defs, i, out_);
}

static void ast_transformer_transform_param_decl(void* self_, ast_param_decl_t** param_decl, void* out_)
{
    (void)self_;
    (void)param_decl;
    (void)out_;
}

static void ast_transformer_transform_var_decl(void* self_, ast_var_decl_t** var_decl, void* out_)
{
    if ((*var_decl)->init_expr != nullptr)
        ast_transformer_transform(self_, &(*var_decl)->init_expr, out_);
}

static void ast_transformer_transform_member_decl(void* self_, ast_member_decl_t** member_decl, void* out_)
{
    if ((*member_decl)->base.init_expr != nullptr)
        ast_transformer_transform(self_, &(*member_decl)->base.init_expr, out_);
}

static void ast_transformer_transform_class_def(void* self_, ast_class_def_t** class_def, void* out_)
{
    for (size_t i = 0; i < vec_size(&(*class_def)->members); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &(*class_def)->members, i, out_);
    for (size_t i = 0; i < vec_size(&(*class_def)->methods); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &(*class_def)->methods, i, out_);
}

static void ast_transformer_transform_method_def(void* self_, ast_method_def_t** method_def, void* out_)
{
    ast_transformer_transform(self_, &(*method_def)->base.body, out_);
}

static void ast_transformer_transform_fn_def(void* self_, ast_fn_def_t** fn_def, void* out_)
{
    ast_transformer_transform(self_, &(*fn_def)->body, out_);
}

static void ast_transformer_transform_array_lit(void* self_, ast_array_lit_t** lit, void* out_)
{
    for (size_t i = 0; i < vec_size(&(*lit)->exprs); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &(*lit)->exprs, i, out_);
}

static void ast_transformer_transform_array_slice(void* self_, ast_array_slice_t** array_slice, void* out_)
{
    ast_transformer_transform(self_, &(*array_slice)->array, out_);
    if ((*array_slice)->start != nullptr)
        ast_transformer_transform(self_, &(*array_slice)->start, out_);
    if ((*array_slice)->end != nullptr)
        ast_transformer_transform(self_, &(*array_slice)->end, out_);
}

static void ast_transformer_transform_array_subscript(void* self_, ast_array_subscript_t** array_subscript, void* out_)
{
    ast_transformer_transform(self_, &(*array_subscript)->array, out_);
    ast_transformer_transform(self_, &(*array_subscript)->index, out_);
}

static void ast_transformer_transform_bin_op(void* self_, ast_bin_op_t** bin_op, void* out_)
{
    ast_transformer_transform(self_, &(*bin_op)->lhs, out_);
    ast_transformer_transform(self_, &(*bin_op)->rhs, out_);
}

static void ast_transformer_transform_call_expr(void* self_, ast_call_expr_t** call_expr, void* out_)
{
    ast_transformer_transform(self_, &(*call_expr)->function, out_);

    for (size_t i = 0; i < vec_size(&(*call_expr)->arguments); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &(*call_expr)->arguments, i, out_);
}

static void ast_transformer_transform_cast_expr(void* self_, ast_cast_expr_t** cast, void* out_)
{
    ast_transformer_transform(self_, &(*cast)->expr, out_);
}

static void ast_transformer_transform_coercion_expr(void* self_, ast_coercion_expr_t** coercion, void* out_)
{
    ast_transformer_transform(self_, &(*coercion)->expr, out_);
}

static void ast_transformer_transform_construct_expr(void* self_, ast_construct_expr_t** construct_expr, void* out_)
{
    for (size_t i = 0; i < vec_size(&(*construct_expr)->member_inits); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &(*construct_expr)->member_inits, i, out_);
}

static void ast_transformer_transform_bool_lit(void* self_, ast_bool_lit_t** bool_lit, void* out_)
{
    (void)self_;
    (void)bool_lit;
    (void)out_;
}

static void ast_transformer_transform_float_lit(void* self_, ast_float_lit_t** float_lit, void* out_)
{
    (void)self_;
    (void)float_lit;
    (void)out_;
}

static void ast_transformer_transform_int_lit(void* self_, ast_int_lit_t** int_lit, void* out_)
{
    (void)self_;
    (void)int_lit;
    (void)out_;
}

static void ast_transformer_transform_member_access(void* self_, ast_member_access_t** member_access, void* out_)
{
    ast_transformer_transform(self_, &(*member_access)->instance, out_);
}

static void ast_transformer_transform_method_call(void* self_, ast_method_call_t** method_call, void* out_)
{
    ast_transformer_transform(self_, &(*method_call)->instance, out_);
    for (size_t i = 0; i < vec_size(&(*method_call)->arguments); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &(*method_call)->arguments, i, out_);
}

static void ast_transformer_transform_member_init(void* self_, ast_member_init_t** member_init, void* out_)
{
    ast_transformer_transform(self_, &(*member_init)->init_expr, out_);
}

static void ast_transformer_transform_null_lit(void* self_, ast_null_lit_t** null_lit, void* out_)
{
    (void)self_;
    (void)null_lit;
    (void)out_;
}

static void ast_transformer_transform_str_lit(void* self_, ast_str_lit_t** str_lit, void* out_)
{
    (void)self_;
    (void)str_lit;
    (void)out_;
}

static void ast_transformer_transform_uninit_lit(void* self_, ast_uninit_lit_t** uninit_lit, void* out_)
{
    (void)self_;
    (void)uninit_lit;
    (void)out_;
}

static void ast_transformer_transform_unary_op(void* self_, ast_unary_op_t** unary_op, void* out_)
{
    ast_transformer_transform(self_, &(*unary_op)->expr, out_);
}

static void ast_transformer_transform_paren_expr(void* self_, ast_paren_expr_t** paren_expr, void* out_)
{
    ast_transformer_transform(self_, &(*paren_expr)->expr, out_);
}

static void ast_transformer_transform_ref_expr(void* self_, ast_ref_expr_t** ref_expr, void* out_)
{
    (void)self_;
    (void)ref_expr;
    (void)out_;
}

static void ast_transformer_transform_self_expr(void* self_, ast_self_expr_t** self_expr, void* out_)
{
    (void)self_;
    (void)self_expr;
    (void)out_;
}

static void ast_transformer_transform_compound_stmt(void* self_, ast_compound_stmt_t** compound_stmt, void* out_)
{
    for (size_t i = 0; i < vec_size(&(*compound_stmt)->inner_stmts); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &(*compound_stmt)->inner_stmts, i, out_);
}

static void ast_transformer_transform_decl_stmt(void* self_, ast_decl_stmt_t** decl_stmt, void* out_)
{
    ast_transformer_transform(self_, &(*decl_stmt)->decl, out_);
}

static void ast_transformer_transform_expr_stmt(void* self_, ast_expr_stmt_t** expr_stmt, void* out_)
{
    ast_transformer_transform(self_, &(*expr_stmt)->expr, out_);
}

static void ast_transformer_transform_if_stmt(void* self_, ast_if_stmt_t** if_stmt, void* out_)
{
    ast_transformer_transform(self_, &(*if_stmt)->condition, out_);
    ast_transformer_transform(self_, &(*if_stmt)->then_branch, out_);
    if ((*if_stmt)->else_branch != nullptr)
        ast_transformer_transform(self_, &(*if_stmt)->else_branch, out_);
}

static void ast_transformer_transform_return_stmt(void* self_, ast_return_stmt_t** return_stmt, void* out_)
{
    ast_transformer_transform(self_, &(*return_stmt)->value_expr, out_);
}

static void ast_transformer_transform_while_stmt(void* self_, ast_while_stmt_t** while_stmt, void* out_)
{
    ast_transformer_transform(self_, &(*while_stmt)->condition, out_);
    ast_transformer_transform(self_, &(*while_stmt)->body, out_);
}

void ast_transformer_init(ast_transformer_t* transformer)
{
    *transformer = (ast_transformer_t){
        .transform_root = ast_transformer_transform_root,
        // Declarations
        .transform_member_decl = ast_transformer_transform_member_decl,
        .transform_param_decl = ast_transformer_transform_param_decl,
        .transform_var_decl = ast_transformer_transform_var_decl,
        // Definitions
        .transform_class_def = ast_transformer_transform_class_def,
        .transform_fn_def = ast_transformer_transform_fn_def,
        .transform_method_def = ast_transformer_transform_method_def,
        // Expressions
        .transform_array_lit = ast_transformer_transform_array_lit,
        .transform_array_slice = ast_transformer_transform_array_slice,
        .transform_array_subscript = ast_transformer_transform_array_subscript,
        .transform_bin_op = ast_transformer_transform_bin_op,
        .transform_bool_lit = ast_transformer_transform_bool_lit,
        .transform_call_expr = ast_transformer_transform_call_expr,
        .transform_cast_expr = ast_transformer_transform_cast_expr,
        .transform_coercion_expr = ast_transformer_transform_coercion_expr,
        .transform_construct_expr = ast_transformer_transform_construct_expr,
        .transform_float_lit = ast_transformer_transform_float_lit,
        .transform_int_lit = ast_transformer_transform_int_lit,
        .transform_member_access = ast_transformer_transform_member_access,
        .transform_member_init = ast_transformer_transform_member_init,
        .transform_method_call = ast_transformer_transform_method_call,
        .transform_null_lit = ast_transformer_transform_null_lit,
        .transform_paren_expr = ast_transformer_transform_paren_expr,
        .transform_ref_expr = ast_transformer_transform_ref_expr,
        .transform_self_expr = ast_transformer_transform_self_expr,
        .transform_str_lit = ast_transformer_transform_str_lit,
        .transform_unary_op = ast_transformer_transform_unary_op,
        .transform_uninit_lit = ast_transformer_transform_uninit_lit,
        // Statements
        .transform_compound_stmt = ast_transformer_transform_compound_stmt,
        .transform_decl_stmt = ast_transformer_transform_decl_stmt,
        .transform_expr_stmt = ast_transformer_transform_expr_stmt,
        .transform_if_stmt = ast_transformer_transform_if_stmt,
        .transform_return_stmt = ast_transformer_transform_return_stmt,
        .transform_while_stmt = ast_transformer_transform_while_stmt,
    };
}

void ast_transformer_transform(void* self_, void* node_inout_, void* out_)
{
    ast_transformer_t* transformer = self_;
    ast_node_t** node = node_inout_;
    (*node)->vtable->accept_transformer(node_inout_, transformer, out_);
}
