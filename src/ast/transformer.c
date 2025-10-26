#include "transformer.h"
#include "ast/node.h"

static void* ast_transformer_transform_root(void* self_, ast_root_t* root, void* out_)
{
    for (size_t i = 0; i < vec_size(&root->tl_defs); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &root->tl_defs, i, out_);
    return root;
}

static void* ast_transformer_transform_param_decl(void* self_, ast_param_decl_t* param_decl, void* out_)
{
    (void)self_;
    (void)param_decl;
    (void)out_;
    return param_decl;
}

static void* ast_transformer_transform_var_decl(void* self_, ast_var_decl_t* var_decl, void* out_)
{
    if (var_decl->init_expr != nullptr)
        var_decl->init_expr = ast_transformer_transform(self_, var_decl->init_expr, out_);
    return var_decl;
}

static void* ast_transformer_transform_member_decl(void* self_, ast_member_decl_t* member_decl, void* out_)
{
    if (member_decl->base.init_expr != nullptr)
        member_decl->base.init_expr = ast_transformer_transform(self_, member_decl->base.init_expr, out_);
    return member_decl;
}

static void* ast_transformer_transform_class_def(void* self_, ast_class_def_t* class_def, void* out_)
{
    for (size_t i = 0; i < vec_size(&class_def->members); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &class_def->members, i, out_);
    for (size_t i = 0; i < vec_size(&class_def->methods); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &class_def->methods, i, out_);
    return class_def;
}

static void* ast_transformer_transform_method_def(void* self_, ast_method_def_t* method_def, void* out_)
{
    method_def->base.body = ast_transformer_transform(self_, method_def->base.body, out_);
    return method_def;
}

static void* ast_transformer_transform_fn_def(void* self_, ast_fn_def_t* fn_def, void* out_)
{
    fn_def->body = ast_transformer_transform(self_, fn_def->body, out_);
    return fn_def;
}

static void* ast_transformer_transform_import_def(void* self_, ast_import_def_t* import_def, void* out_)
{
    (void)self_;
    (void)out_;
    return import_def;
}

static void* ast_transformer_transform_access_expr(void* self_, ast_access_expr_t* access_expr, void* out_)
{
    access_expr->outer = ast_transformer_transform(self_, access_expr->outer, out_);
    access_expr->inner = ast_transformer_transform(self_, access_expr->inner, out_);
    return access_expr;
}

static void* ast_transformer_transform_array_lit(void* self_, ast_array_lit_t* lit, void* out_)
{
    for (size_t i = 0; i < vec_size(&lit->exprs); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &lit->exprs, i, out_);
    return lit;
}

static void* ast_transformer_transform_array_slice(void* self_, ast_array_slice_t* array_slice, void* out_)
{
    array_slice->array = ast_transformer_transform(self_, array_slice->array, out_);
    if (array_slice->start != nullptr)
        array_slice->start = ast_transformer_transform(self_, array_slice->start, out_);
    if (array_slice->end != nullptr)
        array_slice->end = ast_transformer_transform(self_, array_slice->end, out_);
    return array_slice;
}

static void* ast_transformer_transform_array_subscript(void* self_, ast_array_subscript_t* array_subscript, void* out_)
{
    array_subscript->array = ast_transformer_transform(self_, array_subscript->array, out_);
    array_subscript->index = ast_transformer_transform(self_, array_subscript->index, out_);
    return array_subscript;
}

static void* ast_transformer_transform_bin_op(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    bin_op->lhs = ast_transformer_transform(self_, bin_op->lhs, out_);
    bin_op->rhs = ast_transformer_transform(self_, bin_op->rhs, out_);
    return bin_op;
}

static void* ast_transformer_transform_call_expr(void* self_, ast_call_expr_t* call_expr, void* out_)
{
    call_expr->function = ast_transformer_transform(self_, call_expr->function, out_);

    for (size_t i = 0; i < vec_size(&call_expr->arguments); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &call_expr->arguments, i, out_);
    return call_expr;
}

static void* ast_transformer_transform_cast_expr(void* self_, ast_cast_expr_t* cast, void* out_)
{
    cast->expr = ast_transformer_transform(self_, cast->expr, out_);
    return cast;
}

static void* ast_transformer_transform_coercion_expr(void* self_, ast_coercion_expr_t* coercion, void* out_)
{
    coercion->expr = ast_transformer_transform(self_, coercion->expr, out_);
    return coercion;
}

static void* ast_transformer_transform_construct_expr(void* self_, ast_construct_expr_t* construct_expr, void* out_)
{
    for (size_t i = 0; i < vec_size(&construct_expr->member_inits); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &construct_expr->member_inits, i, out_);
    return construct_expr;
}

static void* ast_transformer_transform_bool_lit(void* self_, ast_bool_lit_t* bool_lit, void* out_)
{
    (void)self_;
    (void)bool_lit;
    (void)out_;
    return bool_lit;
}

static void* ast_transformer_transform_float_lit(void* self_, ast_float_lit_t* float_lit, void* out_)
{
    (void)self_;
    (void)float_lit;
    (void)out_;
    return float_lit;
}

static void* ast_transformer_transform_int_lit(void* self_, ast_int_lit_t* int_lit, void* out_)
{
    (void)self_;
    (void)int_lit;
    (void)out_;
    return int_lit;
}

static void* ast_transformer_transform_member_access(void* self_, ast_member_access_t* member_access, void* out_)
{
    member_access->instance = ast_transformer_transform(self_, member_access->instance, out_);
    return member_access;
}

static void* ast_transformer_transform_method_call(void* self_, ast_method_call_t* method_call, void* out_)
{
    method_call->instance = ast_transformer_transform(self_, method_call->instance, out_);
    for (size_t i = 0; i < vec_size(&method_call->arguments); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &method_call->arguments, i, out_);
    return method_call;
}

static void* ast_transformer_transform_member_init(void* self_, ast_member_init_t* member_init, void* out_)
{
    member_init->init_expr = ast_transformer_transform(self_, member_init->init_expr, out_);
    return member_init;
}

static void* ast_transformer_transform_null_lit(void* self_, ast_null_lit_t* null_lit, void* out_)
{
    (void)self_;
    (void)null_lit;
    (void)out_;
    return null_lit;
}

static void* ast_transformer_transform_str_lit(void* self_, ast_str_lit_t* str_lit, void* out_)
{
    (void)self_;
    (void)str_lit;
    (void)out_;
    return str_lit;
}

static void* ast_transformer_transform_uninit_lit(void* self_, ast_uninit_lit_t* uninit_lit, void* out_)
{
    (void)self_;
    (void)uninit_lit;
    (void)out_;
    return uninit_lit;
}

static void* ast_transformer_transform_unary_op(void* self_, ast_unary_op_t* unary_op, void* out_)
{
    unary_op->expr = ast_transformer_transform(self_, unary_op->expr, out_);
    return unary_op;
}

static void* ast_transformer_transform_paren_expr(void* self_, ast_paren_expr_t* paren_expr, void* out_)
{
    paren_expr->expr = ast_transformer_transform(self_, paren_expr->expr, out_);
    return paren_expr;
}

static void* ast_transformer_transform_ref_expr(void* self_, ast_ref_expr_t* ref_expr, void* out_)
{
    (void)self_;
    (void)ref_expr;
    (void)out_;
    return ref_expr;
}

static void* ast_transformer_transform_self_expr(void* self_, ast_self_expr_t* self_expr, void* out_)
{
    (void)self_;
    (void)self_expr;
    (void)out_;
    return self_expr;
}

static void* ast_transformer_transform_compound_stmt(void* self_, ast_compound_stmt_t* compound_stmt, void* out_)
{
    for (size_t i = 0; i < vec_size(&compound_stmt->inner_stmts); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(self_, &compound_stmt->inner_stmts, i, out_);
    return compound_stmt;
}

static void* ast_transformer_transform_decl_stmt(void* self_, ast_decl_stmt_t* decl_stmt, void* out_)
{
    decl_stmt->decl = ast_transformer_transform(self_, decl_stmt->decl, out_);
    return decl_stmt;
}

static void* ast_transformer_transform_expr_stmt(void* self_, ast_expr_stmt_t* expr_stmt, void* out_)
{
    expr_stmt->expr = ast_transformer_transform(self_, expr_stmt->expr, out_);
    return expr_stmt;
}

static void* ast_transformer_transform_for_stmt(void* self_, ast_for_stmt_t* for_stmt, void* out_)
{
    if (for_stmt->init_stmt != nullptr)
        for_stmt->init_stmt = ast_transformer_transform(self_, for_stmt->init_stmt, out_);
    if (for_stmt->cond_expr != nullptr)
        for_stmt->cond_expr = ast_transformer_transform(self_, for_stmt->cond_expr, out_);
    if (for_stmt->post_stmt != nullptr)
        for_stmt->post_stmt = ast_transformer_transform(self_, for_stmt->post_stmt, out_);
    for_stmt->body = ast_transformer_transform(self_, for_stmt->body, out_);
    return for_stmt;
}

static void* ast_transformer_transform_if_stmt(void* self_, ast_if_stmt_t* if_stmt, void* out_)
{
    if_stmt->condition = ast_transformer_transform(self_, if_stmt->condition, out_);
    if_stmt->then_branch = ast_transformer_transform(self_, if_stmt->then_branch, out_);
    if (if_stmt->else_branch != nullptr)
        if_stmt->else_branch = ast_transformer_transform(self_, if_stmt->else_branch, out_);
    return if_stmt;
}

static void* ast_transformer_transform_inc_dec_stmt(void* self_, ast_inc_dec_stmt_t* inc_dec_stmt, void* out_)
{
    inc_dec_stmt->operand = ast_transformer_transform(self_, inc_dec_stmt->operand, out_);
    return inc_dec_stmt;
}

static void* ast_transformer_transform_return_stmt(void* self_, ast_return_stmt_t* return_stmt, void* out_)
{
    return_stmt->value_expr = ast_transformer_transform(self_, return_stmt->value_expr, out_);
    return return_stmt;
}

static void* ast_transformer_transform_while_stmt(void* self_, ast_while_stmt_t* while_stmt, void* out_)
{
    while_stmt->condition = ast_transformer_transform(self_, while_stmt->condition, out_);
    while_stmt->body = ast_transformer_transform(self_, while_stmt->body, out_);
    return while_stmt;
}

static void* ast_transformer_transform_break_stmt(void* self_, ast_break_stmt_t* break_stmt, void* out_)
{
    (void)self_;
    (void)out_;
    return break_stmt;
}

static void* ast_transformer_transform_continue_stmt(void* self_, ast_continue_stmt_t* continue_stmt, void* out_)
{
    (void)self_;
    (void)out_;
    return continue_stmt;
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
        .transform_import_def = ast_transformer_transform_import_def,
        // Expressions
        .transform_access_expr = ast_transformer_transform_access_expr,
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
        .transform_break_stmt = ast_transformer_transform_break_stmt,
        .transform_compound_stmt = ast_transformer_transform_compound_stmt,
        .transform_continue_stmt = ast_transformer_transform_continue_stmt,
        .transform_decl_stmt = ast_transformer_transform_decl_stmt,
        .transform_expr_stmt = ast_transformer_transform_expr_stmt,
        .transform_for_stmt = ast_transformer_transform_for_stmt,
        .transform_if_stmt = ast_transformer_transform_if_stmt,
        .transform_inc_dec_stmt = ast_transformer_transform_inc_dec_stmt,
        .transform_return_stmt = ast_transformer_transform_return_stmt,
        .transform_while_stmt = ast_transformer_transform_while_stmt,
    };
}

void* ast_transformer_transform(void* self_, void* node_, void* out_)
{
    ast_transformer_t* transformer = self_;
    ast_node_t* node = node_;
    return node->vtable->accept_transformer(node, transformer, out_);
}
