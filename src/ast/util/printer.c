#include "printer.h"

#include "ast/decl/member_decl.h"
#include "ast/def/class_def.h"
#include "ast/def/fn_def.h"
#include "ast/def/method_def.h"
#include "ast/def/import_def.h"
#include "ast/type.h"
#include "ast/visitor.h"
#include "common/containers/string.h"
#include "common/util/ssprintf.h"
#include "parser/lexer.h"

#include <stdint.h>
#include <stdlib.h>

struct ast_printer
{
    ast_visitor_t base;
    int indentation;
    bool show_source_loc;
};

static constexpr int PRINT_INDENTATION_WIDTH = 2;

static void print_root(void* self_, ast_root_t* root, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, "Root\n");
    self->indentation += PRINT_INDENTATION_WIDTH;
    for (size_t i = 0; i < vec_size(&root->tl_defs); ++i)
        ast_visitor_visit(self_, vec_get(&root->tl_defs, i), out_);
}

static void print_source_location(ast_printer_t* self, void* node, string_t* out)
{
    ast_node_t* ast_node = node;
    if (!self->show_source_loc)
        return;
    string_append_cstr(out, ssprintf(" <%s:%d:%d, %s:%d:%d>", ast_node->source_begin.filename,
        ast_node->source_begin.line, ast_node->source_begin.column, ast_node->source_end.filename,
        ast_node->source_end.line, ast_node->source_end.column));
}

static void print_param_decl(void* self_, ast_param_decl_t* param_decl, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sParamDecl '%s' '%s'", self->indentation, "", param_decl->name,
        ast_type_string(param_decl->type)));
    print_source_location(self, param_decl, out);
    string_append_cstr(out, "\n");
}

static void print_var_decl(void* self_, ast_var_decl_t* var_decl, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sVarDecl '%s'", self->indentation, "", var_decl->name));
    if (var_decl->type != nullptr)
        string_append_cstr(out, ssprintf(" '%s'", ast_type_string(var_decl->type)));
    print_source_location(self, var_decl, out);
    string_append_cstr(out, "\n");

    if (var_decl->init_expr != nullptr)
    {
        self->indentation += PRINT_INDENTATION_WIDTH;
        ast_visitor_visit(self, var_decl->init_expr, out);
        self->indentation -= PRINT_INDENTATION_WIDTH;
    }
}

static void print_member_decl(void* self_, ast_member_decl_t* member_decl, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sMemberDecl '%s'", self->indentation, "", member_decl->base.name));
    if (member_decl->base.type != nullptr)
        string_append_cstr(out, ssprintf(" '%s'", ast_type_string(member_decl->base.type)));
    print_source_location(self, member_decl, out);
    string_append_cstr(out, "\n");

    if (member_decl->base.init_expr != nullptr)
    {
        self->indentation += PRINT_INDENTATION_WIDTH;
        ast_visitor_visit(self, member_decl->base.init_expr, out);
        self->indentation -= PRINT_INDENTATION_WIDTH;
    }
}

static void print_class_def(void* self_, ast_class_def_t* class_def, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sClassDef '%s'", self->indentation, "", class_def->base.name));
    if (class_def->exported)
        string_append_cstr(out, " exported");
    print_source_location(self, class_def, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    for (size_t i = 0; i < vec_size(&class_def->members); ++i)
        ast_visitor_visit(self, vec_get(&class_def->members, i), out);
    for (size_t i = 0; i < vec_size(&class_def->methods); ++i)
        ast_visitor_visit(self, vec_get(&class_def->methods, i), out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_fn_def(void* self_, ast_fn_def_t* fn_def, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sFnDef '%s'", self->indentation, "", fn_def->base.name));
    if (fn_def->return_type != nullptr)
        string_append_cstr(out, ssprintf(" %s", ast_type_string(fn_def->return_type)));
    if (fn_def->exported)
        string_append_cstr(out, " exported");
    print_source_location(self, fn_def, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    for (size_t i = 0; i < vec_size(&fn_def->params); ++i)
        ast_visitor_visit(self, vec_get(&fn_def->params, i), out);
    if (fn_def->body)
        ast_visitor_visit(self, fn_def->body, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_method_def(void* self_, ast_method_def_t* method_def, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sMethodDef '%s'", self->indentation, "", method_def->base.base.name));
    if (method_def->base.return_type != nullptr)
        string_append_cstr(out, ssprintf(" %s", ast_type_string(method_def->base.return_type)));
    print_source_location(self, method_def, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    for (size_t i = 0; i < vec_size(&method_def->base.params); ++i)
        ast_visitor_visit(self, vec_get(&method_def->base.params, i), out);
    ast_visitor_visit(self, method_def->base.body, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_import_def(void* self_, ast_import_def_t* import_def, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sUseDef '%s.%s'", self->indentation, "", import_def->project_name,
        import_def->module_name));
    print_source_location(self, import_def, out);
    string_append_cstr(out, "\n");
}

static void print_access_expr(void* self_, ast_access_expr_t* access_expr, void* out_)
{
    ast_printer_t* self = self_;
    string_t* out = out_;

    string_append_cstr(out, ssprintf("%*sAccessExpr", self->indentation, ""));
    print_source_location(self, access_expr, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, access_expr->outer, out);
    ast_visitor_visit(self, access_expr->inner, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_array_lit(void* self_, ast_array_lit_t* lit, void* out_)
{
    ast_printer_t* self = self_;
    string_t* out = out_;

    string_append_cstr(out, ssprintf("%*sArrayLit", self->indentation, ""));
    print_source_location(self, lit, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    for (size_t i = 0; i < vec_size(&lit->exprs); ++i)
        ast_visitor_visit(self, vec_get(&lit->exprs, i), out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_array_slice(void* self_, ast_array_slice_t* array_slice, void* out_)
{
    ast_printer_t* self = self_;
    string_t* out = out_;

    string_append_cstr(out, ssprintf("%*sArraySlice", self->indentation, ""));
    print_source_location(self, array_slice, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, array_slice->array, out);
    if (array_slice->start != nullptr)
        ast_visitor_visit(self, array_slice->start, out);
    if (array_slice->end != nullptr)
        ast_visitor_visit(self, array_slice->end, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_array_subscript(void* self_, ast_array_subscript_t* array_subscript, void* out_)
{
    ast_printer_t* self = self_;
    string_t* out = out_;

    string_append_cstr(out, ssprintf("%*sArraySubscript", self->indentation, ""));
    print_source_location(self, array_subscript, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, array_subscript->array, out);
    ast_visitor_visit(self, array_subscript->index, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_bin_op(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sBinOp '%s'", self->indentation, "", token_type_str(bin_op->op)));
    print_source_location(self, bin_op, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, bin_op->lhs, out);
    ast_visitor_visit(self, bin_op->rhs, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_call_expr(void* self_, ast_call_expr_t* call_expr, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sCallExpr", self->indentation, ""));
    print_source_location(self, call_expr, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self_, call_expr->function, out_);
    for (size_t i = 0; i < vec_size(&call_expr->arguments); ++i)
        ast_visitor_visit(self, vec_get(&call_expr->arguments, i), out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_cast_expr(void* self_, ast_cast_expr_t* cast, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sCastExpr '%s'", self->indentation, "", ast_type_string(cast->target)));
    print_source_location(self, cast, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self_, cast->expr, out_);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_coercion_expr(void* self_, ast_coercion_expr_t* coercion, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sCoercionExpr '%s'", self->indentation, "", ast_type_string(coercion->target)));
    print_source_location(self, coercion, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self_, coercion->expr, out_);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_construct_expr(void* self_, ast_construct_expr_t* construct_expr, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sConstructExpr '%s'", self->indentation, "",
        ast_type_string(construct_expr->class_type)));
    print_source_location(self, construct_expr, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    for (size_t i = 0; i < vec_size(&construct_expr->member_inits); ++i)
        ast_visitor_visit(self, vec_get(&construct_expr->member_inits, i), out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_bool_lit(void* self_, ast_bool_lit_t* bool_lit, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sBoolLit '%s'", self->indentation, "", bool_lit->value ? "true" : "false"));
    print_source_location(self, bool_lit, out);
    string_append_cstr(out, "\n");
}

static void print_float_lit(void* self_, ast_float_lit_t* float_lit, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sFloatLit '%lf'", self->indentation, "", float_lit->value));
    print_source_location(self, float_lit, out);
    string_append_cstr(out, "\n");
}

static void print_int_lit(void* self_, ast_int_lit_t* int_lit, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    if (ast_type_is_signed(int_lit->base.type))
        string_append_cstr(out, ssprintf("%*sIntLit '%ld'", self->indentation, "", int_lit->value.as_signed));
    else
        string_append_cstr(out, ssprintf("%*sIntLit '%lu'", self->indentation, "", int_lit->value.as_unsigned));
    print_source_location(self, int_lit, out);
    string_append_cstr(out, "\n");
}

static void print_member_access(void* self_, ast_member_access_t* member_access, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sMemberAccess '%s'", self->indentation, "", member_access->member_name));
    print_source_location(self, member_access, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, member_access->instance, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_method_call(void* self_, ast_method_call_t* method_call, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sMethodCall '%s'", self->indentation, "", method_call->method_name));
    print_source_location(self, method_call, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, method_call->instance, out);
    for (size_t i = 0; i < vec_size(&method_call->arguments); ++i)
        ast_visitor_visit(self, vec_get(&method_call->arguments, i), out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_member_init(void* self_, ast_member_init_t* member_init, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sMemberInit '%s'", self->indentation, "", member_init->member_name));
    print_source_location(self, member_init, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, member_init->init_expr, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_null_lit(void* self_, ast_null_lit_t* lit, void* out_)
{
    (void)lit;
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sNullLit\n", self->indentation, ""));
}

static void print_uninit_lit(void* self_, ast_uninit_lit_t* lit, void* out_)
{
    (void)lit;
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sUninitLit\n", self->indentation, ""));
}

static void print_str_lit(void* self_, ast_str_lit_t* str_lit, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sStrLit '%s'", self->indentation, "", str_lit->value));
    print_source_location(self, str_lit, out);
    string_append_cstr(out, "\n");
}

static void print_unary_op(void* self_, ast_unary_op_t* unary_op, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sUnaryOp '%s'\n", self->indentation, "", token_type_str(unary_op->op)));
    ast_visitor_visit(self, unary_op->expr, out);
}

static void print_paren_expr(void* self_, ast_paren_expr_t* paren_expr, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sParenExpr", self->indentation, ""));
    print_source_location(self, paren_expr, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self_, paren_expr->expr, out_);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_ref_expr(void* self_, ast_ref_expr_t* ref_expr, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sRefExpr '%s'", self->indentation, "", ref_expr->name));
    print_source_location(self, ref_expr, out);
    string_append_cstr(out, "\n");
}

static void print_self_expr(void* self_, ast_self_expr_t* self_expr, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    if (self_expr->implicit)
        string_append_cstr(out, ssprintf("%*sSelfExpr (implicit)", self->indentation, ""));
    else
        string_append_cstr(out, ssprintf("%*sSelfExpr", self->indentation, ""));
    print_source_location(self, self_expr, out);
    string_append_cstr(out, "\n");
}

static void print_compound_stmt(void* self_, ast_compound_stmt_t* compound_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sCompoundStmt", self->indentation, ""));
    print_source_location(self, compound_stmt, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    for (size_t i = 0; i < vec_size(&compound_stmt->inner_stmts); ++i)
        ast_visitor_visit(self, vec_get(&compound_stmt->inner_stmts, i), out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_decl_stmt(void* self_, ast_decl_stmt_t* decl_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sDeclStmt", self->indentation, ""));
    print_source_location(self, decl_stmt, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, decl_stmt->decl, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_expr_stmt(void* self_, ast_expr_stmt_t* expr_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sExprStmt", self->indentation, ""));
    print_source_location(self, expr_stmt, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, expr_stmt->expr, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_for_stmt(void* self_, ast_for_stmt_t* for_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sForStmt", self->indentation, ""));
    print_source_location(self, for_stmt, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    if (for_stmt->init_stmt != nullptr)
        ast_visitor_visit(self, for_stmt->init_stmt, out);
    if (for_stmt->cond_expr != nullptr)
        ast_visitor_visit(self, for_stmt->cond_expr, out);
    if (for_stmt->post_stmt != nullptr)
        ast_visitor_visit(self, for_stmt->post_stmt, out);
    ast_visitor_visit(self, for_stmt->body, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_if_stmt(void* self_, ast_if_stmt_t* if_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sIfStmt", self->indentation, ""));
    if (if_stmt->else_branch != nullptr)
        string_append_cstr(out, "has_else");
    print_source_location(self, if_stmt, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, if_stmt->condition, out);
    ast_visitor_visit(self, if_stmt->then_branch, out);
    if (if_stmt->else_branch != nullptr)
        ast_visitor_visit(self, if_stmt->else_branch, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_inc_dec_stmt(void* self_, ast_inc_dec_stmt_t* inc_dec_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sIncDecStmt %s", self->indentation, "",
        inc_dec_stmt->increment ? "++" : "--"));
    print_source_location(self, inc_dec_stmt, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, inc_dec_stmt->operand, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_return_stmt(void* self_, ast_return_stmt_t* return_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sReturnStmt", self->indentation, ""));
    print_source_location(self, return_stmt, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, return_stmt->value_expr, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_while_stmt(void* self_, ast_while_stmt_t* while_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sWhileStmt", self->indentation, ""));
    print_source_location(self, while_stmt, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, while_stmt->condition, out);
    ast_visitor_visit(self, while_stmt->body, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_break_stmt(void* self_, ast_break_stmt_t* break_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sBreakStmt", self->indentation, ""));
    print_source_location(self, break_stmt, out);
    string_append_cstr(out, "\n");
}

static void print_continue_stmt(void* self_, ast_continue_stmt_t* continue_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sContinueStmt", self->indentation, ""));
    print_source_location(self, continue_stmt, out);
    string_append_cstr(out, "\n");
}

ast_printer_t* ast_printer_create()
{
    ast_printer_t* printer = malloc(sizeof(*printer));

    // NOTE: We do not need to init the visitor because we override every implementation
    *printer = (ast_printer_t){
        .base = (ast_visitor_t){
            .visit_root = print_root,
            // Declarations
            .visit_member_decl = print_member_decl,
            .visit_param_decl = print_param_decl,
            .visit_var_decl = print_var_decl,
            // Definitions
            .visit_class_def = print_class_def,
            .visit_fn_def = print_fn_def,
            .visit_method_def = print_method_def,
            .visit_import_def = print_import_def,
            // Expressions
            .visit_access_expr = print_access_expr,
            .visit_array_lit = print_array_lit,
            .visit_array_slice = print_array_slice,
            .visit_array_subscript = print_array_subscript,
            .visit_bin_op = print_bin_op,
            .visit_bool_lit = print_bool_lit,
            .visit_call_expr = print_call_expr,
            .visit_cast_expr = print_cast_expr,
            .visit_coercion_expr = print_coercion_expr,
            .visit_construct_expr = print_construct_expr,
            .visit_float_lit = print_float_lit,
            .visit_int_lit = print_int_lit,
            .visit_member_access = print_member_access,
            .visit_method_call = print_method_call,
            .visit_member_init = print_member_init,
            .visit_null_lit = print_null_lit,
            .visit_paren_expr = print_paren_expr,
            .visit_ref_expr = print_ref_expr,
            .visit_self_expr = print_self_expr,
            .visit_str_lit = print_str_lit,
            .visit_unary_op = print_unary_op,
            .visit_uninit_lit = print_uninit_lit,
            // Statements
            .visit_break_stmt = print_break_stmt,
            .visit_compound_stmt = print_compound_stmt,
            .visit_continue_stmt = print_continue_stmt,
            .visit_decl_stmt = print_decl_stmt,
            .visit_expr_stmt = print_expr_stmt,
            .visit_for_stmt = print_for_stmt,
            .visit_if_stmt = print_if_stmt,
            .visit_inc_dec_stmt = print_inc_dec_stmt,
            .visit_return_stmt = print_return_stmt,
            .visit_while_stmt = print_while_stmt,
        },
    };

    return printer;
}

void ast_printer_destroy(ast_printer_t* printer)
{
    if (printer != nullptr)
        free(printer);
}

char* ast_printer_print_ast(ast_printer_t* printer, ast_node_t* node)
{
    string_t out = STRING_INIT;
    printer->indentation = 0;
    ast_visitor_visit(printer, node, &out);
    return string_release(&out);
}

void ast_printer_set_show_source_loc(ast_printer_t* printer, bool show)
{
    printer->show_source_loc = show;
}
