#include "expr_evaluator.h"
#include "ast/expr/bool_lit.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/null_lit.h"
#include "ast/visitor.h"

#include <string.h>

static void set_error(expr_evaluator_t* expr_eval, const char* description)
{
    free(expr_eval->last_error);
    expr_eval->last_error = strdup(description);
}

static void eval_bool_lit(void* self_, ast_bool_lit_t* lit, void* out_)
{
    expr_evaluator_t* expr_eval = self_;
    ast_expr_t** node = out_;
    (void)expr_eval;

    *node = ast_bool_lit_create(lit->value);
}

static void eval_float_lit(void* self_, ast_float_lit_t* lit, void* out_)
{
    expr_evaluator_t* expr_eval = self_;
    ast_expr_t** node = out_;
    (void)expr_eval;

    *node = ast_float_lit_create(lit->value, lit->suffix);
}

static void eval_int_lit(void* self_, ast_int_lit_t* lit, void* out_)
{
    expr_evaluator_t* expr_eval = self_;
    ast_expr_t** node = out_;
    (void)expr_eval;

    *node = ast_int_lit_create(lit->has_minus_sign, lit->value.magnitude, lit->suffix);
}

static void eval_null_lit(void* self_, ast_null_lit_t* lit, void* out_)
{
    expr_evaluator_t* expr_eval = self_;
    ast_expr_t** node = out_;
    (void)expr_eval;
    (void)lit;

    *node = ast_null_lit_create();
}

expr_evaluator_t* expr_evaluator_create()
{
    expr_evaluator_t* expr_eval = malloc(sizeof(*expr_eval));

    *expr_eval = (expr_evaluator_t){
        .last_error = strdup(""),
    };

    ast_visitor_init(&expr_eval->base);
    expr_eval->base.visit_bool_lit = eval_bool_lit;
    expr_eval->base.visit_float_lit = eval_float_lit;
    expr_eval->base.visit_int_lit = eval_int_lit;
    expr_eval->base.visit_null_lit = eval_null_lit;

    return expr_eval;
}

void expr_evaluator_destroy(expr_evaluator_t* expr_eval)
{
    if (expr_eval == nullptr)
        return;

    free(expr_eval->last_error);
    free(expr_eval);
}

ast_expr_t* expr_evaluator_eval(expr_evaluator_t* expr_eval, ast_expr_t* expr)
{
    ast_expr_t* result = nullptr;
    ast_visitor_visit(expr_eval, expr, &result);
    if (result == nullptr && strlen(expr_eval->last_error) == 0)
        set_error(expr_eval, "expression is not implemented in expr-eval");
    return result;
}
