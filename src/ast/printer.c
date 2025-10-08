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
        param_decl->type));
    print_source_location(self, param_decl, out);
    string_append_cstr(out, "\n");
}

static void print_var_decl(void* self_, ast_var_decl_t* var_decl, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sVarDecl '%s'", self->indentation, "", var_decl->name));
    if (var_decl->type != nullptr)
        string_append_cstr(out, ssprintf(" '%s'", var_decl->type));
    print_source_location(self, var_decl, out);
    string_append_cstr(out, "\n");

    if (var_decl->init_expr != nullptr)
    {
        self->indentation += PRINT_INDENTATION_WIDTH;
        ast_visitor_visit(self, var_decl->init_expr, out);
        self->indentation -= PRINT_INDENTATION_WIDTH;
    }
}

static void print_fn_def(void* self_, ast_fn_def_t* fn_def, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sFnDef '%s'", self->indentation, "", fn_def->base.name));
    if (fn_def->return_type != nullptr)
        string_append_cstr(out, ssprintf("%s", fn_def->return_type));
    print_source_location(self, fn_def, out);
    string_append_cstr(out, "\n");

    self->indentation += PRINT_INDENTATION_WIDTH;
    for (size_t i = 0; i < vec_size(&fn_def->params); ++i)
        ast_visitor_visit(self, vec_get(&fn_def->params, i), out);
    ast_visitor_visit(self, fn_def->body, out);
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

static void print_int_lit(void* self_, ast_int_lit_t* int_lit, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sIntLit '%d'", self->indentation, "", int_lit->value));
    print_source_location(self, int_lit, out);
    string_append_cstr(out, "\n");
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
            .visit_if_stmt = print_if_stmt,
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
