#include "presenter.h"

#include "ast/def/fn_def.h"
#include "ast/type.h"
#include "ast/visitor.h"
#include "common/containers/string.h"
#include "common/util/ssprintf.h"
#include "parser/lexer.h"

#include <stdint.h>
#include <stdlib.h>

struct ast_presenter
{
    ast_visitor_t base;
};

#define PRELUDE \
    string_t* out = out_; \
    ast_presenter_t* self = self_; \
    (void)out; \
    (void)self;

static void present_root(void* self_, ast_root_t* root, void* out_)
{
    PRELUDE

    string_append_cstr(out, ssprintf("Source: %s", AST_NODE(root)->source_begin.filename));
}

static void present_param_decl(void* self_, ast_param_decl_t* param_decl, void* out_)
{
    PRELUDE

    string_append_cstr(out, ssprintf("%s: %s", param_decl->name, ast_type_string(param_decl->type)));
}

static void present_var_decl(void* self_, ast_var_decl_t* var_decl, void* out_)
{
    PRELUDE

    string_append_cstr(out, ssprintf("var %s", var_decl->name));
    if (var_decl->type != nullptr)
        string_append_cstr(out, ssprintf(": %s", ast_type_string(var_decl->type)));
    if (var_decl->init_expr != nullptr) {
        string_append_cstr(out, " = ");
        ast_visitor_visit(self, var_decl->init_expr, out);
    }
}

static void present_fn_def(void* self_, ast_fn_def_t* fn_def, void* out_)
{
    PRELUDE

    string_append_cstr(out, ssprintf("fn %s(", fn_def->base.name));
    size_t params = vec_size(&fn_def->params);
    for (size_t i = 0; i < params; ++i)
    {
        ast_visitor_visit(self, vec_get(&fn_def->params, i), out);

        if (i + 1 < params)
            string_append_cstr(out, ", ");
    }
    string_append_cstr(out, ")");
    if (fn_def->return_type)
        string_append_cstr(out, ssprintf(" -> %s", ast_type_string(fn_def->return_type)));
}

static void present_array_lit(void* self_, ast_array_lit_t* lit, void* out_)
{
    PRELUDE

    string_append_char(out, '[');
    size_t args = vec_size(&lit->exprs);
    for (size_t i = 0; i < args; ++i)
    {
        ast_visitor_visit(self, vec_get(&lit->exprs, i), out);

        if (i + 1 < args)
            string_append_cstr(out, ", ");
    }
    string_append_char(out, ']');
}

static void present_array_subscript(void* self_, ast_array_subscript_t* array_subscript, void* out_)
{
    PRELUDE

    ast_visitor_visit(self, array_subscript->array, out);
    string_append_char(out, '[');
    ast_visitor_visit(self, array_subscript->index, out);
    string_append_char(out, ']');
}

static void present_bin_op(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    PRELUDE

    ast_visitor_visit(self, bin_op->lhs, out);
    string_append_cstr(out, ssprintf(" %s ", token_type_str(bin_op->op)));
    ast_visitor_visit(self, bin_op->rhs, out);
}

static void present_call_expr(void* self_, ast_call_expr_t* call_expr, void* out_)
{
    PRELUDE

    ast_visitor_visit(self, call_expr->function, out);
    string_append_cstr(out, "(");
    size_t args = vec_size(&call_expr->arguments);
    for (size_t i = 0; i < args; ++i)
    {
        ast_visitor_visit(self, vec_get(&call_expr->arguments, i), out);

        if (i + 1 < args)
            string_append_cstr(out, ", ");
    }
    string_append_cstr(out, ")");
}

static void present_cast_expr(void* self_, ast_cast_expr_t* cast, void* out_)
{
    PRELUDE

    ast_visitor_visit(self, cast->expr, out);
    string_append_cstr(out, ssprintf(" as %s", ast_type_string(cast->target)));
}

static void present_coercion_expr(void* self_, ast_coercion_expr_t* coercion, void* out_)
{
    PRELUDE
    (void)coercion;

    // Do not present anything for compiler-injected node
}

static void present_bool_lit(void* self_, ast_bool_lit_t* bool_lit, void* out_)
{
    PRELUDE

    string_append_cstr(out, ssprintf("%s", bool_lit->value ? "true" : "false"));
}

static void present_float_lit(void* self_, ast_float_lit_t* float_lit, void* out_)
{
    PRELUDE

    string_append_cstr(out, ssprintf("%lf", float_lit->value));
}

static void present_int_lit(void* self_, ast_int_lit_t* int_lit, void* out_)
{
    PRELUDE

    if (ast_type_is_signed(int_lit->base.type))
        string_append_cstr(out, ssprintf("%ld", int_lit->value.as_signed));
    else
        string_append_cstr(out, ssprintf("%ld", int_lit->value.as_unsigned));
}

static void present_null_lit(void* self_, ast_null_lit_t* lit, void* out_)
{
    PRELUDE
    (void)lit;

    string_append_cstr(out, "null");
}

static void present_uninit_lit(void* self_, ast_uninit_lit_t* lit, void* out_)
{
    PRELUDE
    (void)lit;

    string_append_cstr(out, "uninit");
}

static void present_str_lit(void* self_, ast_str_lit_t* str_lit, void* out_)
{
    PRELUDE

    string_append_cstr(out, ssprintf("%s", str_lit->value));
}
static void present_unary_op(void* self_, ast_unary_op_t* unary_op, void* out_)
{
    PRELUDE

    string_append_cstr(out, ssprintf("%s", token_type_str(unary_op->op)));
    ast_visitor_visit(self, unary_op->expr, out);
}

static void present_paren_expr(void* self_, ast_paren_expr_t* paren_expr, void* out_)
{
    PRELUDE

    string_append_cstr(out, "(");
    ast_visitor_visit(self, paren_expr->expr, out);
    string_append_cstr(out, ")");
}

static void present_ref_expr(void* self_, ast_ref_expr_t* ref_expr, void* out_)
{
    PRELUDE

    string_append_cstr(out, ssprintf("%s", ref_expr->name));
}

static void present_compound_stmt(void* self_, ast_compound_stmt_t* compound_stmt, void* out_)
{
    PRELUDE

    // Nothing to output
    (void)compound_stmt;
}

static void present_decl_stmt(void* self_, ast_decl_stmt_t* decl_stmt, void* out_)
{
    PRELUDE

    // Nothing to output
    (void)decl_stmt;
}

static void present_expr_stmt(void* self_, ast_expr_stmt_t* expr_stmt, void* out_)
{
    PRELUDE

    // Nothing to output
    (void)expr_stmt;
}

static void present_if_stmt(void* self_, ast_if_stmt_t* if_stmt, void* out_)
{
    PRELUDE

    // Nothing to output
    (void)if_stmt;
}

static void present_return_stmt(void* self_, ast_return_stmt_t* return_stmt, void* out_)
{
    PRELUDE

    string_append_cstr(out, "return ");
    ast_visitor_visit(self, return_stmt->value_expr, out);
}

static void present_while_stmt(void* self_, ast_while_stmt_t* while_stmt, void* out_)
{
    PRELUDE

    // Nothing to output
    (void)while_stmt;
}

ast_presenter_t* ast_presenter_create()
{
    ast_presenter_t* presenter = malloc(sizeof(*presenter));

    // NOTE: We do not need to init the visitor because we override every implementation
    *presenter = (ast_presenter_t){
        .base = (ast_visitor_t){
            .visit_root = present_root,
            // Declarations
            .visit_param_decl = present_param_decl,
            .visit_var_decl = present_var_decl,
            // Definitions
            .visit_fn_def = present_fn_def,
            // Expressions
            .visit_array_lit = present_array_lit,
            .visit_array_subscript = present_array_subscript,
            .visit_bin_op = present_bin_op,
            .visit_bool_lit = present_bool_lit,
            .visit_call_expr = present_call_expr,
            .visit_cast_expr = present_cast_expr,
            .visit_coercion_expr = present_coercion_expr,
            .visit_float_lit = present_float_lit,
            .visit_int_lit = present_int_lit,
            .visit_null_lit = present_null_lit,
            .visit_paren_expr = present_paren_expr,
            .visit_ref_expr = present_ref_expr,
            .visit_str_lit = present_str_lit,
            .visit_unary_op = present_unary_op,
            .visit_uninit_lit = present_uninit_lit,
            // Statements
            .visit_compound_stmt = present_compound_stmt,
            .visit_decl_stmt = present_decl_stmt,
            .visit_expr_stmt = present_expr_stmt,
            .visit_if_stmt = present_if_stmt,
            .visit_return_stmt = present_return_stmt,
            .visit_while_stmt = present_while_stmt,
        },
    };

    return presenter;
}

void ast_presenter_destroy(ast_presenter_t* presenter)
{
    if (presenter != nullptr)
        free(presenter);
}

char* ast_presenter_present_node(ast_presenter_t* presenter, ast_node_t* node)
{
    string_t out = STRING_INIT;
    ast_visitor_visit(presenter, node, &out);
    return string_release(&out);
}
