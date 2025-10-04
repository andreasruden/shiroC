#include "printer.h"

#include "ast/def/fn_def.h"
#include "common/containers/string.h"
#include "common/util/ssprintf.h"
#include "visitor.h"

#include <stdlib.h>

// TODO: Should have out as a string_t instead of writing directly to stdout

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
    ast_visitor_visit(self, root->tl_def, out);
}

static void print_fn_def(void* self_, ast_fn_def_t* fn_def, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sFnDef (name=%s)\n", self->indentation, "", fn_def->base.name));
    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, fn_def->body, out);
}

static void print_int_lit(void* self_, ast_int_lit_t* int_lit, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sIntLit (value=%d)\n", self->indentation, "", int_lit->value));
}

static void print_compound_stmt(void* self_, ast_compound_stmt_t* compound_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sCompoundStmt\n", self->indentation, ""));
    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, compound_stmt->inner_stmts, out);
}

static void print_return_stmt(void* self_, ast_return_stmt_t* return_stmt, void* out_)
{
    string_t* out = out_;
    ast_printer_t* self = self_;

    string_append_cstr(out, ssprintf("%*sReturnStmt\n", self->indentation, ""));
    self->indentation += PRINT_INDENTATION_WIDTH;
    ast_visitor_visit(self, return_stmt->value_expr, out);
}

ast_printer_t* ast_printer_create()
{
    ast_printer_t* printer = malloc(sizeof(*printer));

    // NOTE: We do not need to init the visitor because we override every implementation
    *printer = (ast_printer_t){
        .base = (ast_visitor_t){
            .visit_root = print_root,
            // Definitions
            .visit_fn_def = print_fn_def,
            // Expressions
            .visit_int_lit = print_int_lit,
            // Statements
            .visit_compound_stmt = print_compound_stmt,
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
