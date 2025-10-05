#include "printer.h"

#include "ast/def/fn_def.h"
#include "common/containers/string.h"
#include "common/util/ssprintf.h"
#include "visitor.h"

#include <stdlib.h>

struct ast_printer
{
    ast_visitor_t base;
    int indentation;
};

static constexpr int PRINT_INDENTATION_WIDTH = 2;

static void print_root(void* self_, ast_root_t* root, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, "Root\n");
    self->indentation += PRINT_INDENTATION_WIDTH;
    for (size_t i = 0; i < ptr_vec_size(&root->tl_defs); ++i)
        ast_visitor_visit(self_, ptr_vec_get(&root->tl_defs, i), out_);
}

static void print_param_decl(void* self_, ast_param_decl_t* param_decl, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sParamDecl '%s' '%s'\n", self->indentation, "", param_decl->type,
        param_decl->name));
}

static void print_var_decl(void* self_, ast_var_decl_t* var_decl, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sVarDecl '%s'", self->indentation, "", var_decl->name));
    if (var_decl->type == nullptr)
        string_append_cstr(out, "\n");
    else
        string_append_cstr(out, ssprintf(" '%s'", var_decl->type));
    string_append_cstr(out, "\n");
    if (var_decl->init_expr != nullptr)
        ast_visitor_visit(self, var_decl->init_expr, out);
}

static void print_fn_def(void* self_, ast_fn_def_t* fn_def, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sFnDef '%s'\n", self->indentation, "", fn_def->base.name));
    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, fn_def->body, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_bin_op(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sBinOp '%s'\n", self->indentation, "", token_type_str(bin_op->op)));
    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, bin_op->lhs, out);
    ast_visitor_visit(self, bin_op->rhs, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_call_expr(void* self_, ast_call_expr_t* call_expr, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sCallExpr\n", self->indentation, ""));
    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self_, call_expr->function, out_);
    for (size_t i = 0; i < ptr_vec_size(&call_expr->arguments); ++i)
        ast_visitor_visit(self, ptr_vec_get(&call_expr->arguments, i), out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_int_lit(void* self_, ast_int_lit_t* int_lit, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sIntLit '%d'\n", self->indentation, "", int_lit->value));
}

static void print_paren_expr(void* self_, ast_paren_expr_t* paren_expr, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sParenExpr\n", self->indentation, ""));
    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self_, paren_expr->expr, out_);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_ref_expr(void* self_, ast_ref_expr_t* ref_expr, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sRefExpr '%s'\n", self->indentation, "", ref_expr->name));
}

static void print_compound_stmt(void* self_, ast_compound_stmt_t* compound_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sCompoundStmt\n", self->indentation, ""));
    self->indentation += PRINT_INDENTATION_WIDTH;
    for (size_t i = 0; i < ptr_vec_size(&compound_stmt->inner_stmts); ++i)
        ast_visitor_visit(self, ptr_vec_get(&compound_stmt->inner_stmts, i), out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_decl_stmt(void* self_, ast_decl_stmt_t* decl_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sDeclStmt\n", self->indentation, ""));
    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, decl_stmt->decl, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_expr_stmt(void* self_, ast_expr_stmt_t* expr_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sExprStmt\n", self->indentation, ""));
    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, expr_stmt->expr, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

static void print_return_stmt(void* self_, ast_return_stmt_t* return_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sReturnStmt\n", self->indentation, ""));
    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, return_stmt->value_expr, out);
    self->indentation -= PRINT_INDENTATION_WIDTH;
}

ast_printer_t* ast_printer_create()
{
    ast_printer_t* printer = malloc(sizeof(*printer));

    // NOTE: We do not need to init the visitor because we override every implementation
    *printer = (ast_printer_t){
        .base = (ast_visitor_t){
            .visit_root = print_root,
            // Declarations
            .visit_param_decl = print_param_decl,
            .visit_var_decl = print_var_decl,
            // Definitions
            .visit_fn_def = print_fn_def,
            // Expressions
            .visit_bin_op = print_bin_op,
            .visit_call_expr = print_call_expr,
            .visit_int_lit = print_int_lit,
            .visit_paren_expr = print_paren_expr,
            .visit_ref_expr = print_ref_expr,
            // Statements
            .visit_compound_stmt = print_compound_stmt,
            .visit_decl_stmt = print_decl_stmt,
            .visit_expr_stmt = print_expr_stmt,
            .visit_return_stmt = print_return_stmt,
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
