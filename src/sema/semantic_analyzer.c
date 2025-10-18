#include "semantic_analyzer.h"

#include "ast/expr/coercion_expr.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/unary_op.h"
#include "ast/node.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/type.h"
#include "ast/visitor.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/util/ssprintf.h"
#include "parser/lexer.h"
#include "sema/init_tracker.h"
#include "sema/semantic_context.h"
#include "sema/symbol.h"
#include "sema/symbol_table.h"
#include "sema/type_expr_solver.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

static symbol_t* add_variable_to_scope(semantic_analyzer_t* sema, void* node, const char* name, ast_type_t* type)
{
    // Error: same scope redeclaration
    symbol_t* collision = symbol_table_lookup_local(sema->ctx->current, name);
    if (collision != nullptr)
    {
        semantic_context_add_error(sema->ctx, node, ssprintf("'%s' already declared at <%s:%d>", name,
            collision->ast->source_begin.filename, collision->ast->source_begin.line));
        return nullptr;
    }

    // Error: parameter shadowing
    if (sema->current_function != nullptr)
    {
        collision = symbol_table_lookup_local(sema->current_function_scope, name);
        if (collision != nullptr)
        {
            semantic_context_add_error(sema->ctx, node, ssprintf("'%s' redeclares function parameter at <%s:%d>", name,
                collision->ast->source_begin.filename, collision->ast->source_begin.line));
            return nullptr;
        }
    }

    // Warning: outer scope shadowing
    collision = symbol_table_lookup(sema->ctx->current, name);
    if (collision != nullptr)
    {
        semantic_context_add_warning(sema->ctx, node, ssprintf("'%s' shadows previous declaration at <%s:%d>", name,
            collision->ast->source_begin.filename, collision->ast->source_begin.line));
    }

    symbol_t* symb = symbol_create(name, SYMBOL_VARIABLE, node);
    symb->type = type;
    symbol_table_insert(sema->ctx->current, symb);
    return symb;
}

static bool require_variable_initialized(semantic_analyzer_t* sema, symbol_t* symbol, void* node)
{
    if (symbol->kind != SYMBOL_VARIABLE)
        return true;

    if (init_tracker_is_initialized(sema->init_tracker, symbol))
        return true;

    semantic_context_add_error(sema->ctx, node, ssprintf("'%s' is not initialized", symbol->name));
    return false;
}

// Check if from_expr can be coerced to to_type, considering expression properties like lvalue.
// Returns the coercion kind and if the coercion kind is INVALID, adds an error to the semantic context.
static ast_coercion_kind_t check_coercion_with_expr(semantic_analyzer_t* sema, void* node, ast_expr_t* from_expr,
    ast_type_t* to_type)
{
    ast_coercion_kind_t coercion = ast_type_can_coerce(from_expr->type, to_type);
    const char* error = nullptr;

    // Array-to-view coercion requires the source to be an lvalue
    // (views cannot reference temporary array literals)
    if (coercion == COERCION_ALWAYS && to_type->kind == AST_TYPE_VIEW &&
        from_expr->type->kind == AST_TYPE_ARRAY && !from_expr->is_lvalue)
    {
        error = "cannot create view into array literal";
        coercion = COERCION_INVALID;
    }

    // Disable assigning from arrays that are not array literals
    // TODO: Figure out when/if we want to allow this; and how we want to differentiate copy/moving
    if (from_expr->type->kind == AST_TYPE_ARRAY && AST_KIND(from_expr) != AST_EXPR_ARRAY_LIT &&
        to_type->kind != AST_TYPE_VIEW)
    {
        error = "cannot assign array";
        coercion = COERCION_INVALID;
    }

    if (coercion == COERCION_INVALID)
    {
        semantic_context_add_error(sema->ctx, node, error ? error : ssprintf("cannot coerce type '%s' into type '%s",
            ast_type_string(from_expr->type), ast_type_string(to_type)));
    }

    return coercion;
}

static void analyze_root(void* self_, ast_root_t* root, void* out_)
{
    semantic_analyzer_t* sema = self_;

    for (size_t i = 0; i < vec_size(&root->tl_defs); ++i)
        ast_visitor_visit(sema, vec_get(&root->tl_defs, i), out_);
}

static void analyze_param_decl(void* self_, ast_param_decl_t* param, void* out_)
{
    (void)out_;
    semantic_analyzer_t* sema = self_;

    param->type = type_expr_solver_solve(sema->ctx, param->type, param);
    if (param->type == ast_type_invalid())
        return;  // don't propagate errors

    if (!ast_type_is_instantiable(param->type))
    {
        semantic_context_add_error(sema->ctx, param, ssprintf("cannot instantiate type '%s'",
            ast_type_string(param->type)));
        return;
    }

    symbol_t* symbol = add_variable_to_scope(sema, param, param->name, param->type);
    if (symbol != nullptr)
    {
        symbol->kind = SYMBOL_PARAMETER;
        symbol->type = param->type;
    }
}

static void analyze_var_decl(void* self_, ast_var_decl_t* var, void* out_)
{
    (void)out_;
    semantic_analyzer_t* sema = self_;

    if (var->type != nullptr)
    {
        var->type = type_expr_solver_solve(sema->ctx, var->type, var);
        if (var->type == ast_type_invalid())
            return;  // don't propagate errors
    }

    if (var->init_expr != nullptr)
        ast_visitor_visit(sema, var->init_expr, nullptr);

    ast_type_t* inferred_type = var->init_expr == nullptr ? nullptr : var->init_expr->type;
    ast_type_t* annotated_type = var->type;
    panic_if(inferred_type == nullptr && annotated_type == nullptr);  // parser disallows this

    // If the inference is null_t, verify we have a valid annotation
    if (inferred_type == ast_type_builtin(TYPE_NULL))
    {
        if (annotated_type == nullptr)
        {
            semantic_context_add_error(sema->ctx, var, "cannot infer type from 'null'");
            return;
        }
        if (annotated_type->kind != AST_TYPE_POINTER)
        {
            semantic_context_add_error(sema->ctx, var, ssprintf("cannot assign 'null' to non-pointer type '%s'",
                ast_type_string(var->type)));
            return;
        }
    }

    // Empty arrays cannot let us infer a type on its own
    if (annotated_type == nullptr && inferred_type != nullptr && inferred_type->kind == AST_TYPE_ARRAY &&
        (!inferred_type->data.array.size_known || inferred_type->data.array.size == 0))
    {
        semantic_context_add_error(sema->ctx, var, "cannot infer type of empty array");
        return;
    }

    // Uninit cannot be inferred from
    if (annotated_type == nullptr && inferred_type == ast_type_builtin(TYPE_UNINIT))
    {
        semantic_context_add_error(sema->ctx, var, "missing type annotation");
        return;
    }

    // Do we have both an annotation and an inference?
    if (annotated_type != nullptr && inferred_type != nullptr)
    {
        ast_coercion_kind_t coercion = check_coercion_with_expr(sema, var, var->init_expr, annotated_type);

        if (coercion == COERCION_INVALID)
        {
            return;
        }
        else if (coercion == COERCION_ALWAYS)
        {
            var->init_expr = ast_coercion_expr_create(var->init_expr, annotated_type);
            inferred_type = annotated_type;
        }
        else if (coercion == COERCION_EQUAL && inferred_type != ast_type_builtin(TYPE_NULL))
        {
            semantic_context_add_warning(sema->ctx, var, "type annotation is superfluous");
        }
    }

    ast_type_t* actual_type = annotated_type ? annotated_type : inferred_type;
    if (!ast_type_is_instantiable(actual_type))
    {
        semantic_context_add_error(sema->ctx, var, ssprintf("cannot instantiate type '%s'",
            ast_type_string(actual_type)));
        return;
    }

    var->type = actual_type;
    symbol_t* symbol = add_variable_to_scope(sema, var, var->name, actual_type);
    if (symbol != nullptr)
        init_tracker_set_initialized(sema->init_tracker, symbol, var->init_expr != nullptr);
}

static void analyze_fn_def(void* self_, ast_fn_def_t* fn, void* out_)
{
    semantic_analyzer_t* sema = self_;

    if (fn->return_type == nullptr)
        fn->return_type = ast_type_builtin(TYPE_VOID);
    else
    {
        fn->return_type = type_expr_solver_solve(sema->ctx, fn->return_type, fn);
        if (fn->return_type == ast_type_invalid())
            return; // don't propagate errors
    }

    semantic_context_push_scope(sema->ctx, SCOPE_FUNCTION);
    sema->current_function = fn;
    sema->current_function_scope = sema->ctx->current;

    for (size_t i = 0; i < vec_size(&fn->params); ++i)
        ast_visitor_visit(sema, vec_get(&fn->params, i), out_);

    ast_visitor_visit(sema, fn->body, out_);

    panic_if(AST_KIND(fn->body) != AST_STMT_COMPOUND);
    ast_compound_stmt_t* block = (ast_compound_stmt_t*)fn->body;
    if (fn->return_type != ast_type_builtin(TYPE_VOID) &&
        (vec_size(&block->inner_stmts) == 0 || AST_KIND(vec_last(&block->inner_stmts)) != AST_STMT_RETURN))
    {
        semantic_context_add_error(sema->ctx, fn, ssprintf("'%s' missing return statement", fn->base.name));
    }

    init_tracker_destroy(sema->init_tracker);
    sema->init_tracker = init_tracker_create();
    semantic_context_pop_scope(sema->ctx);
    sema->current_function = nullptr;
    sema->current_function_scope = nullptr;
}

static bool analyze_fixed_size_array_index(semantic_analyzer_t* sema, void* node, ast_type_t* array_type,
    ast_expr_t* index, bool is_end, bool* bounds_safe)
{
    // TODO: More sophisticated logic
    if (AST_KIND(index) != AST_EXPR_INT_LIT)
    {
        *bounds_safe = false;
        return true;
    }

    // Wrong index type, but let's just fix it when we rework this entirely
    long long index_val = ((ast_int_lit_t*)index)->value.as_signed - (is_end ? 1 : 0);
    if (index_val < 0 || index_val >= (long long)array_type->data.array.size)
    {
        semantic_context_add_error(sema->ctx, node, ssprintf("index '%lld' is out of bounds for '%s'",
            (long long)index_val, ast_type_string(array_type)));
        return false;
    }

    *bounds_safe = true;

    return true;
}

static void analyze_array_lit(void* self_, ast_array_lit_t* lit, void* out_)
{
    semantic_analyzer_t* sema = self_;

    ast_type_t* element_type = ast_type_invalid();
    size_t size = vec_size(&lit->exprs);

    for (size_t i = 0; i < size; ++i)
    {
        ast_expr_t* expr = vec_get(&lit->exprs, i);
        ast_visitor_visit(sema, expr, out_);
        if (element_type == ast_type_invalid())
        {
            element_type = expr->type;
        }
        else if (element_type != expr->type)
        {
            semantic_context_add_error(sema->ctx, lit, ssprintf(
                "mixed types in array literal (first elem type is type '%s', elem at index '%lld' is type '%s')",
                    ast_type_string(element_type), (long long)size, ast_type_string(expr->type)));
            lit->base.type = ast_type_invalid();
            return;
        }
    }

    lit->base.is_lvalue = false;
    lit->base.type = ast_type_array(element_type, size);
}

static void analyze_array_slice(void* self_, ast_array_slice_t* slice, void* out_)
{
    semantic_analyzer_t* sema = self_;

    ast_visitor_visit(sema, slice->array, out_);
    if (slice->array->type == ast_type_invalid())
        return;  // don't propagate errors

    // Verify type is slicable & extract element-type
    ast_type_t* element_type = ast_type_invalid();
    switch (slice->array->type->kind)
    {
        case AST_TYPE_ARRAY:
            element_type = slice->array->type->data.array.element_type;
            break;
        case AST_TYPE_HEAP_ARRAY:
            element_type = slice->array->type->data.heap_array.element_type;
            break;
        case AST_TYPE_VIEW:
            element_type = slice->array->type->data.view.element_type;
            break;
        case AST_TYPE_POINTER:
            element_type = slice->array->type->data.pointer.pointee;
            break;
        default:
            semantic_context_add_error(sema->ctx, slice, ssprintf("cannot slice type '%s'",
                ast_type_string(slice->array->type)));
            slice->base.type = ast_type_invalid();
            return;
    }

    bool start_safe = slice->start == nullptr;
    bool end_safe = slice->end == nullptr;

    // Visit start & verify bounds if possible
    if (slice->start != nullptr)
    {
        ast_visitor_visit(sema, slice->start, out_);
        if (slice->start->type == ast_type_invalid())
            return;  // don't propagate errors

        if (slice->array->type->kind == AST_TYPE_ARRAY && !analyze_fixed_size_array_index(sema, slice,
            slice->array->type, slice->start, false, &start_safe))
        {
            slice->base.type = ast_type_invalid();
            return;
        }
    }

    // Visit end & verify bounds if possible
    if (slice->end != nullptr)
    {
        ast_visitor_visit(sema, slice->end, out_);
        if (slice->end->type == ast_type_invalid())
            return;  // don't propagate errors

        if (slice->array->type->kind == AST_TYPE_ARRAY && !analyze_fixed_size_array_index(sema, slice,
            slice->array->type, slice->end, true, &end_safe))
        {
            slice->base.type = ast_type_invalid();
            return;
        }
    }

    // If we already know that start > end, error out
    if (slice->start != nullptr && slice->end != nullptr)
    {
        // FIXME: dervied value should be added to the expr node when more complex constants can be handled
        if (AST_KIND(slice->start) == AST_EXPR_INT_LIT && AST_KIND(slice->end) == AST_EXPR_INT_LIT &&
            ((ast_int_lit_t*)slice->start)->value.as_signed > ((ast_int_lit_t*)slice->end)->value.as_signed)
        {
            semantic_context_add_error(sema->ctx, slice, "invalid slice bounds: start > end");
            slice->base.type = ast_type_invalid();
            return;
        }
    }

    // Type coercion for start
    if (slice->start != nullptr)
    {
        ast_coercion_kind_t coercion = ast_type_can_coerce(slice->start->type, ast_type_builtin(TYPE_USIZE));
        if (coercion != COERCION_EQUAL && coercion != COERCION_ALWAYS && coercion != COERCION_WIDEN &&
            coercion != COERCION_SIGNEDNESS)
        {
            semantic_context_add_error(sema->ctx, slice, ssprintf("type '%s' is not usable for bounds",
                ast_type_string(slice->start->type)));
            slice->base.type = ast_type_invalid();
            return;
        }

        if (coercion != COERCION_EQUAL)
            slice->start = ast_coercion_expr_create(slice->start, ast_type_builtin(TYPE_USIZE));
    }

    // Type coercion for end
    if (slice->end != nullptr)
    {
        ast_coercion_kind_t coercion = ast_type_can_coerce(slice->end->type, ast_type_builtin(TYPE_USIZE));
        if (coercion != COERCION_EQUAL && coercion != COERCION_ALWAYS && coercion != COERCION_WIDEN &&
            coercion != COERCION_SIGNEDNESS)
        {
            semantic_context_add_error(sema->ctx, slice, ssprintf("type '%s' is not usable for bounds",
                ast_type_string(slice->end->type)));
            slice->base.type = ast_type_invalid();
            return;
        }

        if (coercion != COERCION_EQUAL)
            slice->end = ast_coercion_expr_create(slice->end, ast_type_builtin(TYPE_USIZE));
    }

    slice->bounds_safe = start_safe && end_safe;
    slice->base.is_lvalue = false;
    slice->base.type = ast_type_view(element_type);
}

static void analyze_array_subscript(void* self_, ast_array_subscript_t* subscript, void* out_)
{
    semantic_analyzer_t* sema = self_;

    ast_visitor_visit(sema, subscript->array, out_);
    if (subscript->array->type == ast_type_invalid())
        return;  // don't propagate errors

    ast_visitor_visit(sema, subscript->index, out_);

    ast_type_t* expr_type;
    switch (subscript->array->type->kind)
    {
        case AST_TYPE_ARRAY:
            if (!analyze_fixed_size_array_index(sema, subscript, subscript->array->type, subscript->index, false,
                &subscript->bounds_safe))
            {
                expr_type = ast_type_invalid();
            }
            else
                expr_type = subscript->array->type->data.array.element_type;
            break;
        case AST_TYPE_HEAP_ARRAY:
            expr_type = subscript->array->type->data.heap_array.element_type;
            break;
        case AST_TYPE_VIEW:
            expr_type = subscript->array->type->data.view.element_type;
            break;
        case AST_TYPE_POINTER:
            expr_type = subscript->array->type->data.pointer.pointee;
            break;
        default:
            semantic_context_add_error(sema->ctx, subscript, ssprintf("cannot subscript type '%s'",
                ast_type_string(subscript->array->type)));
            expr_type = ast_type_invalid();
            break;
    }

    ast_coercion_kind_t coercion = ast_type_can_coerce(subscript->index->type, ast_type_builtin(TYPE_USIZE));

    // We are very permissible with implicit conversion for array index because array index are bounds checked
    // at runtime, and the guard `(usize)any_integer < size` prevents any invalid access
    if (coercion != COERCION_EQUAL && coercion != COERCION_ALWAYS && coercion != COERCION_WIDEN &&
        coercion != COERCION_SIGNEDNESS)
    {
        semantic_context_add_error(sema->ctx, subscript, ssprintf("type '%s' is not usable as an index",
            ast_type_string(subscript->index->type)));
        subscript->base.type = ast_type_invalid();
        return;
    }

    if (coercion != COERCION_EQUAL)
        subscript->index = ast_coercion_expr_create(subscript->index, ast_type_builtin(TYPE_USIZE));

    subscript->base.is_lvalue = true;
    subscript->base.type = expr_type;
}

// Returns true if via some coercion we can think of LHS and RHS as the same type
// NOTE: This does not imply the types are valid for an operator, check that with is_type_valid_for_operator().
static bool is_type_equal_for_bin_op(ast_type_t* lhs_type, ast_type_t* rhs_type)
{
    if (lhs_type == rhs_type)
        return true;

    // Type null can interact with any pointer type or itself
    ast_type_t* null_type = ast_type_builtin(TYPE_NULL);
    if (lhs_type == null_type && (rhs_type == null_type || rhs_type->kind == AST_TYPE_POINTER))
        return true;
    if (rhs_type == null_type && (lhs_type == null_type || lhs_type->kind == AST_TYPE_POINTER))
        return true;

    return false;
}

static void analyze_bin_op_assignment(semantic_analyzer_t* sema, ast_bin_op_t* bin_op)
{
    symbol_t* lhs_symbol = nullptr;
    sema->is_lvalue_context = true;
    ast_visitor_visit(sema, bin_op->lhs, &lhs_symbol);
    sema->is_lvalue_context = false;

    if (bin_op->lhs->type == ast_type_invalid())
        return;  // avoid cascading errors

    if (!bin_op->lhs->is_lvalue)
    {
        semantic_context_add_error(sema->ctx, bin_op->lhs, "expr is not l-value");
        return;
    }

    if (lhs_symbol != nullptr && lhs_symbol->kind == SYMBOL_FUNCTION)
    {
        semantic_context_add_error(sema->ctx, bin_op->lhs, "cannot assign to function");
        return;
    }

    // For compound assignments (+=, -=, etc.), check that LHS is initialized first
    if (bin_op->op != TOKEN_ASSIGN && lhs_symbol != nullptr)
    {
        if (!require_variable_initialized(sema, lhs_symbol, bin_op->lhs))
            return;
    }

    if (lhs_symbol != nullptr)
        init_tracker_set_initialized(sema->init_tracker, lhs_symbol, true);

    ast_visitor_visit(sema, bin_op->rhs, nullptr);
    if (bin_op->rhs->type == ast_type_invalid())
        return;  // avoid cascading errors

    ast_coercion_kind_t coercion = check_coercion_with_expr(sema, bin_op, bin_op->rhs, bin_op->lhs->type);
    if (coercion == COERCION_INVALID)
        return;
    else if (coercion != COERCION_EQUAL && coercion != COERCION_ALWAYS)
    {
        semantic_context_add_error(sema->ctx, bin_op->lhs,
            ssprintf("left-hand side type '%s' does not match right-hand side type '%s'",
                ast_type_string(bin_op->lhs->type), ast_type_string(bin_op->rhs->type)));
        return;
    }

    if (coercion == COERCION_ALWAYS)
        bin_op->rhs = ast_coercion_expr_create(bin_op->rhs, bin_op->lhs->type);

    bin_op->base.is_lvalue = false;
    bin_op->base.type = bin_op->lhs->type;
}

static bool is_type_valid_for_operator(ast_type_t* type, token_type_t operator, ast_type_t** result_type)
{
    if (type->kind != AST_TYPE_BUILTIN && type->kind != AST_TYPE_POINTER)
    {
        *result_type = ast_type_invalid();
        return false;
    }

    if (token_type_is_arithmetic_op(operator))
    {
        if (ast_type_is_arithmetic(type))
        {
            *result_type = ast_type_builtin(type->data.builtin.type);
            return true;
        }
        else
        {
            *result_type = ast_type_invalid();
            return false;
        }
    }

    if ((operator == TOKEN_EQ || operator == TOKEN_NEQ) && ast_type_has_equality(type))
    {
        *result_type = ast_type_builtin(TYPE_BOOL);
        return true;
    }

    if (token_type_is_relation_op(operator))
    {
        if (ast_type_is_arithmetic(type))
        {
            *result_type = ast_type_builtin(TYPE_BOOL);
            return true;
        }
        else
        {
            *result_type = ast_type_invalid();
            return false;
        }
    }

    panic("Unhandled operator %d", operator);
}

static void analyze_bin_op(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    (void)out_;
    semantic_analyzer_t* sema = self_;

    if (token_type_is_assignment_op(bin_op->op))
    {
        analyze_bin_op_assignment(sema, bin_op);
        return;
    }

    symbol_t* lhs_symbol = nullptr;
    ast_visitor_visit(sema, bin_op->lhs, &lhs_symbol);
    ast_visitor_visit(sema, bin_op->rhs, nullptr);
    if (bin_op->lhs->type == ast_type_invalid() || bin_op->rhs->type == ast_type_invalid())
        return;  // avoid cascading errors

    if (lhs_symbol != nullptr && lhs_symbol->kind == SYMBOL_FUNCTION && token_type_is_assignment_op(bin_op->op))
    {
        semantic_context_add_error(sema->ctx, bin_op->lhs, "cannot assign to function");
        return;
    }

    if (!is_type_equal_for_bin_op(bin_op->lhs->type, bin_op->rhs->type))
    {
        semantic_context_add_error(sema->ctx, bin_op, ssprintf("type mismatch '%s' and '%s'",
            ast_type_string(bin_op->lhs->type), ast_type_string(bin_op->rhs->type)));
        return;
    }

    ast_type_t* result_type = nullptr;
    if (!is_type_valid_for_operator(bin_op->lhs->type, bin_op->op, &result_type))
    {
        semantic_context_add_error(sema->ctx, bin_op, ssprintf("cannot apply '%s' to '%s' and '%s'",
            token_type_str(bin_op->op), ast_type_string(bin_op->lhs->type), ast_type_string(bin_op->rhs->type)));
        return;
    }

    bin_op->base.is_lvalue = false;
    bin_op->base.type = result_type;
}

static void analyze_bool_lit(void* self_, ast_bool_lit_t* lit, void* out_)
{
    (void)self_;
    (void)out_;

    lit->base.is_lvalue = false;
    lit->base.type = ast_type_builtin(TYPE_BOOL);
}

static void analyze_call_expr(void* self_, ast_call_expr_t* call, void* out_)
{
    semantic_analyzer_t* sema = self_;

    symbol_t* symbol = nullptr;
    ast_visitor_visit(sema, call->function, &symbol);

    if (symbol == nullptr)
        return;

    if (symbol->kind != SYMBOL_FUNCTION)
    {
        semantic_context_add_error(sema->ctx, call->function,
            ssprintf("symbol '%s' is not callable", symbol->name));
        return;
    }

    int num_params = (int)vec_size(&symbol->data.function.parameters);
    int num_args = (int)vec_size(&call->arguments);
    if (num_params != num_args)
    {
        semantic_context_add_error(sema->ctx, call,
            ssprintf("function '%s' takes %d arguments but %d given", symbol->name, num_params, num_args));
        return;
    }

    for (int i = 0; i < num_args; ++i)
    {
        ast_param_decl_t* param_decl = vec_get(&symbol->data.function.parameters, (size_t)i);
        ast_expr_t* arg_expr = vec_get(&call->arguments, (size_t)i);

        ast_visitor_visit(sema, arg_expr, out_);

        ast_coercion_kind_t coercion = check_coercion_with_expr(sema, arg_expr, arg_expr, param_decl->type);
        if (coercion == COERCION_INVALID)
            return;
        else if (coercion != COERCION_EQUAL && coercion != COERCION_ALWAYS && coercion != COERCION_WIDEN)
        {
            semantic_context_add_error(sema->ctx, arg_expr,
                ssprintf("arg type '%s' does not match parameter '%s' type '%s'", ast_type_string(arg_expr->type),
                    param_decl->name, ast_type_string(param_decl->type)));
            return;
        }

        if (coercion != COERCION_EQUAL)
        {
            // Wrap arg in coercion expr (don't free the return of replace)
            vec_replace(&call->arguments, (size_t)i, ast_coercion_expr_create(arg_expr, param_decl->type));
        }
    }

    call->base.is_lvalue = false;
    call->base.type = symbol->type;
}

static void analyze_float_lit(void* self_, ast_float_lit_t* lit, void* out_)
{
    (void)out_;
    semantic_analyzer_t* sema = self_;

    ast_type_t* type = ast_type_builtin(TYPE_F64);
    if (strcmp(lit->suffix, "f32") == 0) type = ast_type_builtin(TYPE_F32);
    else if (strcmp(lit->suffix, "f64") == 0) type = ast_type_builtin(TYPE_F64);
    else if (strlen(lit->suffix) > 0)
    {
        semantic_context_add_error(sema->ctx, lit, ssprintf("invalid suffix '%s' for integer literal", lit->suffix));
        return;
    }

    if (type == ast_type_builtin(TYPE_F32))
    {
        float f32_value = (float)lit->value;

        // Check for overflow when converting to f32
        if (isinf(f32_value) && !isinf(lit->value))
        {
            semantic_context_add_error(sema->ctx, lit, "floating-point literal too large for f32");
            return;
        }

        // Check for underflow (non-zero became zero)
        if (f32_value == 0.0f && lit->value != 0.0)
        {
            semantic_context_add_error(sema->ctx, lit, "floating-point literal underflows to zero in f32");
            return;
        }
    }

    lit->base.is_lvalue = false;
    lit->base.type = type;
}

static void analyze_int_lit(void* self_, ast_int_lit_t* lit, void* out_)
{
    (void)out_;
    semantic_analyzer_t* sema = self_;

    ast_type_t* type = ast_type_builtin(TYPE_I32);
    if (strcmp(lit->suffix, "i8") == 0) type = ast_type_builtin(TYPE_I8);
    else if (strcmp(lit->suffix, "i16") == 0) type = ast_type_builtin(TYPE_I16);
    else if (strcmp(lit->suffix, "i32") == 0) type = ast_type_builtin(TYPE_I32);
    else if (strcmp(lit->suffix, "i64") == 0) type = ast_type_builtin(TYPE_I64);
    else if (strcmp(lit->suffix, "u8") == 0) type = ast_type_builtin(TYPE_U8);
    else if (strcmp(lit->suffix, "u16") == 0) type = ast_type_builtin(TYPE_U16);
    else if (strcmp(lit->suffix, "u32") == 0) type = ast_type_builtin(TYPE_U32);
    else if (strcmp(lit->suffix, "u64") == 0) type = ast_type_builtin(TYPE_U64);
    else if (strlen(lit->suffix) > 0)
    {
        semantic_context_add_error(sema->ctx, lit, ssprintf("invalid suffix '%s' for integer literal", lit->suffix));
        return;
    }

    bool is_signed = ast_type_is_signed(type);

    bool fits;
    if (is_signed)
    {
        uint64_t max_magnitude;
        if (type == ast_type_builtin(TYPE_I8)) max_magnitude = (uint64_t)INT8_MAX;
        else if (type == ast_type_builtin(TYPE_I16)) max_magnitude = (uint64_t)INT16_MAX;
        else if (type == ast_type_builtin(TYPE_I32)) max_magnitude = (uint64_t)INT32_MAX;
        else /* i64 */ max_magnitude = (uint64_t)INT64_MAX;

        if (lit->has_minus_sign)
            max_magnitude += 1;

        fits = lit->value.magnitude <= max_magnitude;
        if (fits)
        {
            if (lit->has_minus_sign)
                lit->value.as_signed = -(int64_t)lit->value.magnitude;
            else
                lit->value.as_signed = (int64_t)lit->value.magnitude;
        }
    }
    else  // unsigned
    {
        if (lit->has_minus_sign)
        {
            semantic_context_add_error(sema->ctx, lit, "literal cannot be negative");
            return;
        }

        uint64_t max_val;
        if (type == ast_type_builtin(TYPE_U8)) max_val = UINT8_MAX;
        else if (type == ast_type_builtin(TYPE_U16)) max_val = UINT16_MAX;
        else if (type == ast_type_builtin(TYPE_U32)) max_val = UINT32_MAX;
        else /* u64 */ max_val = UINT64_MAX;

        fits = lit->value.magnitude <= max_val;
        if (fits)
            lit->value.as_unsigned = lit->value.magnitude;
    }

    if (!fits)
    {
        semantic_context_add_error(sema->ctx, lit, ssprintf("integer literal does not fit in type '%s'",
            ast_type_string(type)));
        return;
    }

    lit->base.is_lvalue = false;
    lit->base.type = type;
}

static void analyze_null_lit(void* self_, ast_null_lit_t* lit, void* out_)
{
    (void)self_;
    (void)out_;

    lit->base.is_lvalue = false;
    lit->base.type = ast_type_builtin(TYPE_NULL);
}

static void analyze_paren_expr(void* self_, ast_paren_expr_t* paren, void* out_)
{
    semantic_analyzer_t* sema = self_;

    ast_visitor_visit(sema, paren->expr, out_);

    paren->base.is_lvalue = paren->expr->is_lvalue;
    paren->base.type = paren->expr->type;
}

static void analyze_ref_expr(void* self_, ast_ref_expr_t* ref_expr, void* out_)
{
    semantic_analyzer_t* sema = self_;
    symbol_t** symbol_out = out_;

    symbol_t* symbol = symbol_table_lookup(sema->ctx->current, ref_expr->name);
    if (symbol == nullptr)
    {
        semantic_context_add_error(sema->ctx, ref_expr, ssprintf("unknown symbol name '%s'", ref_expr->name));
        return;
    }

    if (symbol_out != nullptr)
        *symbol_out = symbol;

    if (!sema->is_lvalue_context)
    {
        if (!require_variable_initialized(sema, symbol, ref_expr))
            return;
    }

    ref_expr->base.is_lvalue = true;
    ref_expr->base.type = symbol->type;
}

static void analyze_str_lit(void* self_, ast_str_lit_t* lit, void* out_)
{
    (void)self_;
    (void)out_;

    lit->base.is_lvalue = false;
    lit->base.type = ast_type_builtin(TYPE_VOID);  // FIXME: We don't have a string type yet
}

static void analyze_unary_op(void* self_, ast_unary_op_t* unary_op, void* out_)
{
    (void)out_;
    semantic_analyzer_t* sema = self_;

    symbol_t* symbol = nullptr;  // can be nullptr after visit
    ast_visitor_visit(sema, unary_op->expr, &symbol);

    switch (unary_op->op)
    {
        case TOKEN_AMPERSAND:
            if (!unary_op->expr->is_lvalue)
            {
                semantic_context_add_error(sema->ctx, unary_op, ssprintf("cannot take address of r-value '%s'",
                    symbol == nullptr ? "invalid expression" : symbol->name));
                return;
            }
            unary_op->base.is_lvalue = false;
            unary_op->base.type = ast_type_pointer(unary_op->expr->type);
            break;
        case TOKEN_STAR:
            if (unary_op->expr->type->kind != AST_TYPE_POINTER)
            {
                semantic_context_add_error(sema->ctx, unary_op, ssprintf("cannot dereference type '%s'",
                    ast_type_string(unary_op->expr->type)));
                return;
            }
            unary_op->base.is_lvalue = true;
            unary_op->base.type = unary_op->expr->type->data.pointer.pointee;
            break;
        default:
            panic("Unhandled op %d", unary_op->op);
    }
}

static void analyze_uninit_lit(void* self_, ast_uninit_lit_t* lit, void* out_)
{
    (void)self_;
    (void)out_;

    lit->base.type = ast_type_builtin(TYPE_UNINIT);
}

static void analyze_compound_statement(void* self_, ast_compound_stmt_t* block, void* out_)
{
    semantic_analyzer_t* sema = self_;

    semantic_context_push_scope(sema->ctx, SCOPE_BLOCK);

    for (size_t i = 0; i < vec_size(&block->inner_stmts); ++i)
        ast_visitor_visit(sema, vec_get(&block->inner_stmts, i), out_);

    semantic_context_pop_scope(sema->ctx);
}

static void analyze_decl_stmt(void* self_, ast_decl_stmt_t* stmt, void* out_)
{
    semantic_analyzer_t* sema = self_;

    ast_visitor_visit(sema, stmt->decl, out_);
}

static void analyze_expr_stmt(void* self_, ast_expr_stmt_t* stmt, void* out_)
{
    semantic_analyzer_t* sema = self_;

    ast_visitor_visit(sema, stmt->expr, out_);
}

static void analyze_if_stmt(void* self_, ast_if_stmt_t* if_stmt, void* out_)
{
    semantic_analyzer_t* sema = self_;

    ast_visitor_visit(sema, if_stmt->condition, out_);
    if (if_stmt->condition->type == ast_type_invalid())
        return;  // avoid cascading errors

    if (if_stmt->condition->type != ast_type_builtin(TYPE_BOOL))
    {
        semantic_context_add_error(sema->ctx, if_stmt->condition,
            ssprintf("invalid expression type '%s' in if-condition: must be bool",
                ast_type_string(if_stmt->condition->type)));
    }

    init_tracker_t* then_tracker = sema->init_tracker;
    init_tracker_t* else_tracker = init_tracker_clone(sema->init_tracker);

    sema->init_tracker = then_tracker;
    ast_visitor_visit(sema, if_stmt->then_branch, out_);
    then_tracker = sema->init_tracker;

    if (if_stmt->else_branch != nullptr)
    {
        sema->init_tracker = else_tracker;
        ast_visitor_visit(sema, if_stmt->else_branch, out_);
        else_tracker = sema->init_tracker;
    }

    sema->init_tracker = init_tracker_merge(&then_tracker, &else_tracker);
}

static void analyze_return_stmt(void* self_, ast_return_stmt_t* ret_stmt, void* out_)
{
    semantic_analyzer_t* sema = self_;

    ast_visitor_visit(sema, ret_stmt->value_expr, out_);
    if (ret_stmt->value_expr->type == ast_type_invalid())
        return;  // avoid propagating error

    ast_coercion_kind_t coercion = check_coercion_with_expr(sema, ret_stmt->value_expr, ret_stmt->value_expr,
        sema->current_function->return_type);
    if (coercion == COERCION_INVALID)
        return;

    if (coercion == COERCION_ALWAYS || coercion == COERCION_WIDEN)
        ret_stmt->value_expr = ast_coercion_expr_create(ret_stmt->value_expr, sema->current_function->return_type);
}

static void analyze_while_stmt(void* self_, ast_while_stmt_t* while_stmt, void* out_)
{
    semantic_analyzer_t* sema = self_;

    ast_visitor_visit(sema, while_stmt->condition, out_);
    if (while_stmt->condition->type == ast_type_invalid())
        return;  // avoid cascading errors

    if (while_stmt->condition->type != ast_type_builtin(TYPE_BOOL))
    {
        semantic_context_add_error(sema->ctx, while_stmt->condition,
            ssprintf("invalid expression type '%s' in while-condition: must be bool",
                ast_type_string(while_stmt->condition->type)));
    }

    init_tracker_t* entry_state = sema->init_tracker;
    init_tracker_t* body_tracker = init_tracker_clone(sema->init_tracker);
    sema->init_tracker = body_tracker;

    ast_visitor_visit(sema, while_stmt->body, out_);

    // Discard init_tracker state changes inside while
    init_tracker_destroy(sema->init_tracker);
    sema->init_tracker = entry_state;
}

semantic_analyzer_t* semantic_analyzer_create(semantic_context_t* ctx)
{
    semantic_analyzer_t* sema = malloc(sizeof(*sema));

    // NOTE: We do not need to init the visitor because we override every implementation
    *sema = (semantic_analyzer_t){
        .ctx = ctx,
        .init_tracker = init_tracker_create(),
        .base = (ast_visitor_t){
            .visit_root = analyze_root,
            // Declarations
            .visit_param_decl = analyze_param_decl,
            .visit_var_decl = analyze_var_decl,
            // Definitions
            .visit_fn_def = analyze_fn_def,
            // Expressions
            .visit_array_lit = analyze_array_lit,
            .visit_array_slice = analyze_array_slice,
            .visit_array_subscript = analyze_array_subscript,
            .visit_bin_op = analyze_bin_op,
            .visit_bool_lit = analyze_bool_lit,
            .visit_call_expr = analyze_call_expr,
            .visit_float_lit = analyze_float_lit,
            .visit_int_lit = analyze_int_lit,
            .visit_null_lit = analyze_null_lit,
            .visit_paren_expr = analyze_paren_expr,
            .visit_ref_expr = analyze_ref_expr,
            .visit_str_lit = analyze_str_lit,
            .visit_unary_op = analyze_unary_op,
            .visit_uninit_lit = analyze_uninit_lit,
            // Statements
            .visit_compound_stmt = analyze_compound_statement,
            .visit_decl_stmt = analyze_decl_stmt,
            .visit_expr_stmt = analyze_expr_stmt,
            .visit_if_stmt = analyze_if_stmt,
            .visit_return_stmt = analyze_return_stmt,
            .visit_while_stmt = analyze_while_stmt,
        },
    };

    return sema;
}

void semantic_analyzer_destroy(semantic_analyzer_t* sema)
{
    if (sema == nullptr)
        return;

    init_tracker_destroy(sema->init_tracker);
    free(sema);
}

bool semantic_analyzer_run(semantic_analyzer_t* sema, ast_node_t* root)
{
    size_t errors = vec_size(&sema->ctx->error_nodes);
    ast_visitor_visit(sema, root, nullptr);
    return errors == vec_size(&sema->ctx->error_nodes);  // no new errors
}
