#ifndef SEMA_EXPR_EVALUATOR__H
#define SEMA_EXPR_EVALUATOR__H

#include "ast/visitor.h"

/*
 * A basic interpreter that evalutes some constant expressions.
 * Currently fairly incomplete.
 */

typedef struct expr_evaluator
{
    ast_visitor_t base;
    char* last_error;
} expr_evaluator_t;

expr_evaluator_t* expr_evaluator_create();

void expr_evaluator_destroy(expr_evaluator_t* expr_eval);

// Returns a constructed AST expr that is the result of the expression, or nullptr
// if the expression cannot be evaluated. Updates last_error.
ast_expr_t* expr_evaluator_eval(expr_evaluator_t* expr_eval, ast_expr_t* expr);

#endif
