#include "semantic_analyzer.h"

#include "ast/decl/member_decl.h"
#include "ast/expr/coercion_expr.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/member_access.h"
#include "ast/expr/member_init.h"
#include "ast/expr/method_call.h"
#include "ast/expr/self_expr.h"
#include "ast/expr/unary_op.h"
#include "ast/node.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/transformer.h"
#include "ast/type.h"
#include "ast/util/cloner.h"
#include "common/containers/hash_table.h"
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

// Returns true if type chain is well defined (i.e. no user-type names that don't exist)
static bool verify_type_defined(semantic_analyzer_t* sema, ast_type_t* type, void* node)
{
    switch (type->kind)
    {
        case AST_TYPE_USER:
        {
            symbol_t* symbol = symbol_table_lookup(sema->ctx->global, type->data.user.name);
            if (symbol == nullptr)
            {
                semantic_context_add_error(sema->ctx, node, ssprintf("undefined type '%s'", type->data.user.name));
                return false;
            }
            if (symbol->kind != SYMBOL_CLASS)
            {
                semantic_context_add_error(sema->ctx, node, ssprintf("'%s' is not a class", type->data.user.name));
                return false;
            }
            return true;
        }
        case AST_TYPE_POINTER:
            return verify_type_defined(sema, type->data.pointer.pointee, node);
        case AST_TYPE_ARRAY:
            return verify_type_defined(sema, type->data.array.element_type, node);
        case AST_TYPE_HEAP_ARRAY:
            return verify_type_defined(sema, type->data.heap_array.element_type, node);
        case AST_TYPE_VIEW:
            return verify_type_defined(sema, type->data.view.element_type, node);
        case AST_TYPE_BUILTIN:
            return true;
        case AST_TYPE_INVALID:
            return false;  // don't emit a new error, should have already been reported
    }

    panic("Unhandled kind %d", type->kind);
}

static symbol_t* add_variable_to_scope(semantic_analyzer_t* sema, void* node, const char* name, ast_type_t* type,
    bool member)
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

    symbol_t* symb = symbol_create(name, member ? SYMBOL_MEMBER : SYMBOL_VARIABLE, node);
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

static void* analyze_root(void* self_, ast_root_t* root, void* out_)
{
    semantic_analyzer_t* sema = self_;

    for (size_t i = 0; i < vec_size(&root->tl_defs); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(sema, &root->tl_defs, i, out_);

    return root;
}

static void* analyze_member_decl(void* self_, ast_member_decl_t* member, void* out_)
{
    semantic_analyzer_t* sema = self_;
    panic_if(sema->current_class == nullptr);

    if (member->base.type == ast_type_invalid())
        return member;  // don't propagate errors

    if (member->base.type == ast_type_user(sema->current_class->base.name))
    {
        semantic_context_add_error(sema->ctx, member,
            ssprintf("infinitely recursive type: needs to be pointer to self (%s*)", sema->current_class->base.name));
        member->base.type = ast_type_invalid();
        return member;
    }

    if (!verify_type_defined(sema, member->base.type, member))
        return member;

    if (member->base.init_expr != nullptr)
    {
        member->base.init_expr = ast_transformer_transform(sema, member->base.init_expr, out_);
        if (member->base.init_expr->type == ast_type_invalid())
            return member;

        // We don't allow any coercion for member defaults
        if (member->base.init_expr->type != member->base.type &&
            !(member->base.init_expr->type == ast_type_builtin(TYPE_NULL) &&
                member->base.type->kind == AST_TYPE_POINTER))
        {
            semantic_context_add_error(sema->ctx, member, ssprintf("type '%s' does not match annotation",
                ast_type_string(member->base.init_expr->type)));
            return member;
        }
    }

    // Add to class scope's symbol table
    add_variable_to_scope(sema, member, member->base.name, member->base.type, true);

    // NOTE: Already added to class symbol by decl_collector
    return member;
}

static void* analyze_param_decl(void* self_, ast_param_decl_t* param, void* out_)
{
    (void)out_;
    semantic_analyzer_t* sema = self_;

    if (param->type == ast_type_invalid())
        return param;  // don't propagate errors

    if (!verify_type_defined(sema, param->type, param))
        return param;

    if (!ast_type_is_instantiable(param->type))
    {
        semantic_context_add_error(sema->ctx, param, ssprintf("cannot instantiate type '%s'",
            ast_type_string(param->type)));
        return param;
    }

    symbol_t* symbol = add_variable_to_scope(sema, param, param->name, param->type, false);
    if (symbol != nullptr)
    {
        symbol->kind = SYMBOL_PARAMETER;
        symbol->type = param->type;
    }
    return param;
}

static void* analyze_var_decl(void* self_, ast_var_decl_t* var, void* out_)
{
    (void)out_;
    semantic_analyzer_t* sema = self_;

    if (var->type != nullptr)
    {
        var->type = type_expr_solver_solve(sema->ctx, var->type, var);
        if (var->type == ast_type_invalid())
            return var;  // don't propagate errors

        if (!verify_type_defined(sema, var->type, var))
            return var;
    }

    if (var->init_expr != nullptr)
        var->init_expr = ast_transformer_transform(sema, var->init_expr, nullptr);

    ast_type_t* inferred_type = var->init_expr == nullptr ? nullptr : var->init_expr->type;
    ast_type_t* annotated_type = var->type;
    panic_if(inferred_type == nullptr && annotated_type == nullptr);  // parser disallows this

    // If the inference is null_t, verify we have a valid annotation
    if (inferred_type == ast_type_builtin(TYPE_NULL))
    {
        if (annotated_type == nullptr)
        {
            semantic_context_add_error(sema->ctx, var, "cannot infer type from 'null'");
            return var;
        }
        if (annotated_type->kind != AST_TYPE_POINTER)
        {
            semantic_context_add_error(sema->ctx, var, ssprintf("cannot assign 'null' to non-pointer type '%s'",
                ast_type_string(var->type)));
            return var;
        }
    }

    // Empty arrays cannot let us infer a type on its own
    if (annotated_type == nullptr && inferred_type != nullptr && inferred_type->kind == AST_TYPE_ARRAY &&
        (!inferred_type->data.array.size_known || inferred_type->data.array.size == 0))
    {
        semantic_context_add_error(sema->ctx, var, "cannot infer type of empty array");
        return var;
    }

    // Uninit cannot be inferred from
    if (annotated_type == nullptr && inferred_type == ast_type_builtin(TYPE_UNINIT))
    {
        semantic_context_add_error(sema->ctx, var, "missing type annotation");
        return var;
    }

    // Do we have both an annotation and an inference?
    if (annotated_type != nullptr && inferred_type != nullptr)
    {
        ast_coercion_kind_t coercion = check_coercion_with_expr(sema, var, var->init_expr, annotated_type);

        if (coercion == COERCION_INVALID)
        {
            return var;
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
        return var;
    }

    var->type = actual_type;
    symbol_t* symbol = add_variable_to_scope(sema, var, var->name, actual_type, false);
    if (symbol != nullptr)
        init_tracker_set_initialized(sema->init_tracker, symbol, var->init_expr != nullptr);
    return var;
}

static void* analyze_class_def(void* self_, ast_class_def_t* class_def, void* out_)
{
    semantic_analyzer_t* sema = self_;

    semantic_context_push_scope(sema->ctx, SCOPE_CLASS);
    sema->current_class = class_def;

    // Add "self" to scope
    ast_decl_t* self_decl = ast_member_decl_create("self", ast_type_user(class_def->base.name), nullptr);
    symbol_t* self_symb = symbol_create("self", SYMBOL_MEMBER, self_decl);
    symbol_table_insert(sema->ctx->current, self_symb);

    for (size_t i = 0; i < vec_size(&class_def->members); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(sema, &class_def->members, i, out_);

    for (size_t i = 0; i < vec_size(&class_def->methods); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(sema, &class_def->methods, i, out_);

    ast_node_destroy(self_decl);
    semantic_context_pop_scope(sema->ctx);
    sema->current_class = nullptr;
    return class_def;
}
static void* analyze_fn_def(void* self_, ast_fn_def_t* fn, void* out_)
{
    // TODO: This and method is very similar, should try to reuse their impl

    semantic_analyzer_t* sema = self_;
    panic_if(fn->return_type == nullptr);  // solved by decl_collector

    semantic_context_push_scope(sema->ctx, SCOPE_FUNCTION);
    sema->current_function = fn;
    sema->current_function_scope = sema->ctx->current;

    for (size_t i = 0; i < vec_size(&fn->params); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(sema, &fn->params, i, out_);

    fn->body = ast_transformer_transform(sema, fn->body, out_);

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
    return fn;
}
static void* analyze_method_def(void* self_, ast_method_def_t* method, void* out_)
{
    // TODO: This and fn is very similar, should try to reuse their impl

    semantic_analyzer_t* sema = self_;
    panic_if(sema->current_class == nullptr);
    panic_if(method->base.return_type == nullptr);  // solved by decl_collector

    symbol_t* method_symbol = symbol_create(method->base.base.name, SYMBOL_METHOD, method);
    symbol_table_insert(sema->ctx->current, method_symbol);

    semantic_context_push_scope(sema->ctx, SCOPE_METHOD);
    sema->current_method = method;
    sema->current_function_scope = sema->ctx->current;

    for (size_t i = 0; i < vec_size(&method->base.params); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(sema, &method->base.params, i, out_);

    method->base.body = ast_transformer_transform(sema, method->base.body, out_);

    panic_if(AST_KIND(method->base.body) != AST_STMT_COMPOUND);
    ast_compound_stmt_t* block = (ast_compound_stmt_t*)method->base.body;
    if (method->base.return_type != ast_type_builtin(TYPE_VOID) &&
        (vec_size(&block->inner_stmts) == 0 || AST_KIND(vec_last(&block->inner_stmts)) != AST_STMT_RETURN))
    {
        semantic_context_add_error(sema->ctx, method, ssprintf("'%s' missing return statement",
            method->base.base.name));
    }

    init_tracker_destroy(sema->init_tracker);
    sema->init_tracker = init_tracker_create();
    semantic_context_pop_scope(sema->ctx);
    sema->current_method = nullptr;
    sema->current_function_scope = nullptr;
    return method;
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

static void* analyze_array_lit(void* self_, ast_array_lit_t* lit, void* out_)
{
    semantic_analyzer_t* sema = self_;

    ast_type_t* element_type = ast_type_invalid();
    size_t size = vec_size(&lit->exprs);

    for (size_t i = 0; i < size; ++i)
    {
        ast_expr_t* expr = vec_get(&lit->exprs, i);
        expr = ast_transformer_transform(sema, expr, out_);
        vec_replace(&lit->exprs, i, expr);
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
            return lit;
        }
    }

    lit->base.is_lvalue = false;
    lit->base.type = ast_type_array(element_type, size);
    return lit;
}

static void* analyze_array_slice(void* self_, ast_array_slice_t* slice, void* out_)
{
    semantic_analyzer_t* sema = self_;

    slice->array = ast_transformer_transform(sema, slice->array, out_);
    if (slice->array->type == ast_type_invalid())
        return slice;  // don't propagate errors

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
            return slice;
    }

    bool start_safe = slice->start == nullptr;
    bool end_safe = slice->end == nullptr;

    // Visit start & verify bounds if possible
    if (slice->start != nullptr)
    {
        slice->start = ast_transformer_transform(sema, slice->start, out_);
        if (slice->start->type == ast_type_invalid())
        {
            slice->base.type = ast_type_invalid();
            return slice;  // don't propagate errors
        }

        if (slice->array->type->kind == AST_TYPE_ARRAY && !analyze_fixed_size_array_index(sema, slice,
            slice->array->type, slice->start, false, &start_safe))
        {
            slice->base.type = ast_type_invalid();
            return slice;
        }
    }

    // Visit end & verify bounds if possible
    if (slice->end != nullptr)
    {
        slice->end = ast_transformer_transform(sema, slice->end, out_);
        if (slice->end->type == ast_type_invalid())
        {
            slice->base.type = ast_type_invalid();
            return slice;  // don't propagate errors
        }

        if (slice->array->type->kind == AST_TYPE_ARRAY && !analyze_fixed_size_array_index(sema, slice,
            slice->array->type, slice->end, true, &end_safe))
        {
            slice->base.type = ast_type_invalid();
            return slice;
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
            return slice;
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
            return slice;
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
            return slice;
        }

        if (coercion != COERCION_EQUAL)
            slice->end = ast_coercion_expr_create(slice->end, ast_type_builtin(TYPE_USIZE));
    }

    slice->bounds_safe = start_safe && end_safe;
    slice->base.is_lvalue = false;
    slice->base.type = ast_type_view(element_type);
    return slice;
}

static void* analyze_array_subscript(void* self_, ast_array_subscript_t* subscript, void* out_)
{
    semantic_analyzer_t* sema = self_;

    subscript->array = ast_transformer_transform(sema, subscript->array, out_);
    if (subscript->array->type == ast_type_invalid())
        return subscript;  // don't propagate errors

    subscript->index = ast_transformer_transform(sema, subscript->index, out_);

    ast_type_t* expr_type;
    switch (subscript->array->type->kind)
    {
        case AST_TYPE_ARRAY:
            if (!analyze_fixed_size_array_index(sema, subscript, subscript->array->type, subscript->index, false,
                &subscript->bounds_safe))
            {
                expr_type = ast_type_invalid();
                return subscript;
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
            return subscript;
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
        return subscript;
    }

    if (coercion != COERCION_EQUAL)
        subscript->index = ast_coercion_expr_create(subscript->index, ast_type_builtin(TYPE_USIZE));

    subscript->base.is_lvalue = true;
    subscript->base.type = expr_type;
    return subscript;
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

static void* analyze_bin_op_assignment(semantic_analyzer_t* sema, ast_bin_op_t* bin_op)
{
    symbol_t* lhs_symbol = nullptr;
    sema->is_lvalue_context = true;
    bin_op->lhs = ast_transformer_transform(sema, bin_op->lhs, &lhs_symbol);
    sema->is_lvalue_context = false;

    if (bin_op->lhs->type == ast_type_invalid())
    {
        bin_op->base.type = ast_type_invalid();
        return bin_op;  // avoid cascading errors
    }

    if (!bin_op->lhs->is_lvalue)
    {
        semantic_context_add_error(sema->ctx, bin_op->lhs, "expr is not l-value");
        bin_op->base.type = ast_type_invalid();
        return bin_op;
    }

    if (lhs_symbol != nullptr && lhs_symbol->kind == SYMBOL_FUNCTION)
    {
        semantic_context_add_error(sema->ctx, bin_op->lhs, "cannot assign to function");
        bin_op->base.type = ast_type_invalid();
        return bin_op;
    }

    // For compound assignments (+=, -=, etc.), check that LHS is initialized first
    if (bin_op->op != TOKEN_ASSIGN && lhs_symbol != nullptr)
    {
        if (!require_variable_initialized(sema, lhs_symbol, bin_op->lhs))
        {
            bin_op->base.type = ast_type_invalid();
            return bin_op;
        }
    }

    if (lhs_symbol != nullptr)
        init_tracker_set_initialized(sema->init_tracker, lhs_symbol, true);

    bin_op->rhs = ast_transformer_transform(sema, bin_op->rhs, nullptr);
    if (bin_op->rhs->type == ast_type_invalid())
        return bin_op;  // avoid cascading errors

    ast_coercion_kind_t coercion = check_coercion_with_expr(sema, bin_op, bin_op->rhs, bin_op->lhs->type);
    if (coercion == COERCION_INVALID)
        return bin_op;
    else if (coercion != COERCION_EQUAL && coercion != COERCION_ALWAYS)
    {
        semantic_context_add_error(sema->ctx, bin_op->lhs,
            ssprintf("left-hand side type '%s' does not match right-hand side type '%s'",
                ast_type_string(bin_op->lhs->type), ast_type_string(bin_op->rhs->type)));
        bin_op->base.type = ast_type_invalid();
        return bin_op;
    }

    if (coercion == COERCION_ALWAYS)
        bin_op->rhs = ast_coercion_expr_create(bin_op->rhs, bin_op->lhs->type);

    bin_op->base.is_lvalue = false;
    bin_op->base.type = bin_op->lhs->type;
    return bin_op;
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

static void* analyze_bin_op(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    (void)out_;
    semantic_analyzer_t* sema = self_;

    if (token_type_is_assignment_op(bin_op->op))
        return analyze_bin_op_assignment(sema, bin_op);

    symbol_t* lhs_symbol = nullptr;
    bin_op->lhs = ast_transformer_transform(sema, bin_op->lhs, &lhs_symbol);
    bin_op->rhs = ast_transformer_transform(sema, bin_op->rhs, nullptr);
    if (bin_op->lhs->type == ast_type_invalid() || bin_op->rhs->type == ast_type_invalid())
    {
        bin_op->base.type = ast_type_invalid();
        return bin_op;  // avoid cascading errors
    }

    if (!is_type_equal_for_bin_op(bin_op->lhs->type, bin_op->rhs->type))
    {
        semantic_context_add_error(sema->ctx, bin_op, ssprintf("type mismatch '%s' and '%s'",
            ast_type_string(bin_op->lhs->type), ast_type_string(bin_op->rhs->type)));
        bin_op->base.type = ast_type_invalid();
        return bin_op;
    }

    ast_type_t* result_type = nullptr;
    if (!is_type_valid_for_operator(bin_op->lhs->type, bin_op->op, &result_type))
    {
        semantic_context_add_error(sema->ctx, bin_op, ssprintf("cannot apply '%s' to '%s' and '%s'",
            token_type_str(bin_op->op), ast_type_string(bin_op->lhs->type), ast_type_string(bin_op->rhs->type)));
        bin_op->base.type = ast_type_invalid();
        return bin_op;
    }

    bin_op->base.is_lvalue = false;
    bin_op->base.type = result_type;
    return bin_op;
}

static void* analyze_bool_lit(void* self_, ast_bool_lit_t* lit, void* out_)
{
    (void)self_;
    (void)out_;

    lit->base.is_lvalue = false;
    lit->base.type = ast_type_builtin(TYPE_BOOL);
    return lit;
}

static bool analyze_call_and_method_shared(semantic_analyzer_t* sema, void* node, const char* fn_name,
    vec_t* parameters, vec_t* arguments)
{
    int num_params = (int)vec_size(parameters);
    int num_args = (int)vec_size(arguments);
    if (num_params != num_args)
    {
        semantic_context_add_error(sema->ctx, node,
            ssprintf("function '%s' takes %d arguments but %d given", fn_name, num_params, num_args));
        return false;
    }

    for (int i = 0; i < num_args; ++i)
    {
        ast_param_decl_t* param_decl = vec_get(parameters, (size_t)i);
        ast_expr_t* arg_expr = vec_get(arguments, (size_t)i);

        arg_expr = ast_transformer_transform(sema, arg_expr, nullptr);
        vec_replace(arguments, (size_t)i, arg_expr);

        ast_coercion_kind_t coercion = check_coercion_with_expr(sema, arg_expr, arg_expr, param_decl->type);
        if (coercion == COERCION_INVALID)
            return false;
        else if (coercion != COERCION_EQUAL && coercion != COERCION_ALWAYS && coercion != COERCION_WIDEN)
        {
            semantic_context_add_error(sema->ctx, arg_expr,
                ssprintf("arg type '%s' does not match parameter '%s' type '%s'", ast_type_string(arg_expr->type),
                    param_decl->name, ast_type_string(param_decl->type)));
            return false;
        }

        if (coercion != COERCION_EQUAL)
        {
            // Wrap arg in coercion expr (don't free the return of replace)
            vec_replace(arguments, (size_t)i, ast_coercion_expr_create(arg_expr, param_decl->type));
        }
    }

    return true;
}

static void* analyze_call_expr(void* self_, ast_call_expr_t* call, void* out_)
{
    (void)out_;
    semantic_analyzer_t* sema = self_;

    symbol_t* symbol = nullptr;
    call->function = ast_transformer_transform(sema, call->function, &symbol);
    if (symbol == nullptr)
    {
        call->base.type = ast_type_invalid();
        return call;
    }

    // Call expr can transform to method call via implicit self expr
    if (symbol->kind == SYMBOL_METHOD)
    {
        ast_expr_t* replacement = ast_method_call_create(ast_self_expr_create(true), symbol->name,
            &call->arguments);
        ast_node_destroy(call);
        return ast_transformer_transform(sema, replacement, out_);  // visit method call
    }

    if (symbol->kind != SYMBOL_FUNCTION)
    {
        semantic_context_add_error(sema->ctx, call->function, ssprintf("symbol '%s' is not callable", symbol->name));
        call->base.type = ast_type_invalid();
        return call;
    }

    if (!analyze_call_and_method_shared(sema, call, symbol->name, &symbol->data.function.parameters, &call->arguments))
    {
        call->base.type = ast_type_invalid();
        return call;
    }

    call->base.is_lvalue = false;
    call->base.type = symbol->type;
    return call;
}

static void* analyze_construct_expr(void* self_, ast_construct_expr_t* construct, void* out_)
{
    (void)out_;
    semantic_analyzer_t* sema = self_;
    hash_table_t initialized_members = HASH_TABLE_INIT(nullptr);

    // Verify type is correct
    if (construct->class_type->kind != AST_TYPE_USER)
    {
        semantic_context_add_error(sema->ctx, construct, ssprintf("cannot construct type '%s'",
            ast_type_string(construct->class_type)));
        construct->base.type = ast_type_invalid();
        goto cleanup;
    }

    // Make sure referenced class name exists
    symbol_t* class_symbol = symbol_table_lookup(sema->ctx->global, construct->class_type->data.user.name);
    if (class_symbol == nullptr || class_symbol->kind != SYMBOL_CLASS)
    {
        semantic_context_add_error(sema->ctx, construct,
            class_symbol == nullptr ? ssprintf("undefined type '%s'", construct->class_type->data.user.name) :
                ssprintf("'%s' is not a class", class_symbol->name));
        construct->base.type = ast_type_invalid();
        goto cleanup;
    }

    // Visit every member initialization
    for (size_t i = 0; i < vec_size(&construct->member_inits); ++i)
    {
        // Bit ugly, but we pass in class symbol's name and get member's name in return (or nullptr on failure)
        const char* name = class_symbol->name;
        AST_TRANSFORMER_TRANSFORM_VEC(sema, &construct->member_inits, i, &name);
        if (name == nullptr)  // member init outputs nullptr if invalid
        {
            construct->base.type = ast_type_invalid();
            goto cleanup;
        }

        if (hash_table_contains(&initialized_members, name))
        {
            semantic_context_add_error(sema->ctx, construct, ssprintf("duplicate initialization for member '%s'",
                name));
            construct->base.type = ast_type_invalid();
            goto cleanup;
        }

        hash_table_insert(&initialized_members, name, nullptr);
    }

    // Handle default initializations, also make sure no non-default initializable member was left out
    hash_table_iter_t itr;
    for (hash_table_iter_init(&itr, &class_symbol->data.class.members); hash_table_iter_has_next(&itr);
        hash_table_iter_next(&itr))
    {
        hash_table_entry_t* entry = hash_table_iter_current(&itr);
        ast_member_decl_t* member_decl = entry->value;

        if (hash_table_contains(&initialized_members, entry->key))
            continue;

        if (member_decl->base.init_expr == nullptr)
        {
            semantic_context_add_error(sema->ctx, construct, ssprintf("missing initialization for '%s'",
                member_decl->base.name));
            construct->base.type = ast_type_invalid();
            goto cleanup;
        }

        ast_expr_t* cloned_expr = ast_expr_clone(member_decl->base.init_expr);
        panic_if(cloned_expr == nullptr);
        ast_member_init_t* injected_init = ast_member_init_create(member_decl->base.name, cloned_expr);
        vec_push(&construct->member_inits, injected_init);
    }

    // TODO: Do we want to ensure some consitent initialization ordering

    construct->base.type = construct->class_type;
    construct->base.is_lvalue = false;

cleanup:
    hash_table_deinit(&initialized_members);
    return construct;
}

static void* analyze_float_lit(void* self_, ast_float_lit_t* lit, void* out_)
{
    (void)out_;
    semantic_analyzer_t* sema = self_;

    ast_type_t* type = ast_type_builtin(TYPE_F64);
    if (strcmp(lit->suffix, "f32") == 0) type = ast_type_builtin(TYPE_F32);
    else if (strcmp(lit->suffix, "f64") == 0) type = ast_type_builtin(TYPE_F64);
    else if (strlen(lit->suffix) > 0)
    {
        semantic_context_add_error(sema->ctx, lit, ssprintf("invalid suffix '%s' for integer literal", lit->suffix));
        lit->base.type = ast_type_invalid();
        return lit;
    }

    if (type == ast_type_builtin(TYPE_F32))
    {
        float f32_value = (float)lit->value;

        // Check for overflow when converting to f32
        if (isinf(f32_value) && !isinf(lit->value))
        {
            semantic_context_add_error(sema->ctx, lit, "floating-point literal too large for f32");
            lit->base.type = ast_type_invalid();
            return lit;
        }

        // Check for underflow (non-zero became zero)
        if (f32_value == 0.0f && lit->value != 0.0)
        {
            semantic_context_add_error(sema->ctx, lit, "floating-point literal underflows to zero in f32");
            lit->base.type = ast_type_invalid();
            return lit;
        }
    }

    lit->base.is_lvalue = false;
    lit->base.type = type;
    return lit;
}

static void* analyze_int_lit(void* self_, ast_int_lit_t* lit, void* out_)
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
        lit->base.type = ast_type_invalid();
        return lit;
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
            lit->base.type = ast_type_invalid();
            return lit;
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
        lit->base.type = ast_type_invalid();
        return lit;
    }

    lit->base.is_lvalue = false;
    lit->base.type = type;
    return lit;
}

// verify that in "instance".<expr> expression, the instance is a valid class instance
// returns a symbol to the class the instance refers to
static symbol_t* verify_class_instance(semantic_analyzer_t* sema, ast_expr_t** instance_inout, void* out_)
{
    bool was_lvalue_ctx = sema->is_lvalue_context;
    sema->is_lvalue_context = true;
    *instance_inout = ast_transformer_transform(sema, *instance_inout, out_);
    sema->is_lvalue_context = was_lvalue_ctx;

    ast_expr_t* instance = *instance_inout;

    if (instance->type == ast_type_invalid())
        return nullptr;

    if (!instance->is_lvalue)
    {
        semantic_context_add_error(sema->ctx, instance, "not l-value");
        return nullptr;
    }

    // Expression before '.' must be a user-type or pointer to user type
    if (instance->type->kind != AST_TYPE_USER &&
        !(instance->type->kind == AST_TYPE_POINTER && instance->type->data.pointer.pointee->kind == AST_TYPE_USER))
    {
        semantic_context_add_error(sema->ctx, instance, "not class type or pointer to class type");
        return nullptr;
    }

    // Lookup symbol for class
    const char* class_name = instance->type->kind == AST_TYPE_USER ? instance->type->data.user.name :
        instance->type->data.pointer.pointee->data.user.name;
    symbol_t* class_symb = symbol_table_lookup(sema->ctx->global, class_name);
    if (class_symb == nullptr || class_symb->kind != SYMBOL_CLASS)
    {
        semantic_context_add_error(sema->ctx, instance, class_symb == nullptr ?
            ssprintf("undefined type '%s'", class_name) : ssprintf("'%s' is not a class", class_name));
        return nullptr;
    }

    return class_symb;
}

static void* analyze_member_access(void* self_, ast_member_access_t* access, void* out_)
{
    semantic_analyzer_t* sema = self_;

    symbol_t* class_symb = verify_class_instance(sema, &access->instance, out_);
    if (class_symb == nullptr)
    {
        access->base.type = ast_type_invalid();
        return access;
    }

    // Make sure class contains the specified name as a member
    ast_member_decl_t* member_decl = hash_table_find(&class_symb->data.class.members, access->member_name);
    if (member_decl == nullptr)
    {
        semantic_context_add_error(sema->ctx, access, ssprintf("type '%s' has no member '%s'",
            access->instance->type->data.user.name, access->member_name));
        access->base.type = ast_type_invalid();
        return access;
    }

    access->base.type = member_decl->base.type;
    access->base.is_lvalue = true;
    return access;
}

static void* analyze_member_init(void* self_, ast_member_init_t* init, void* out_)
{
    semantic_analyzer_t* sema = self_;
    const char** name_in_out = out_;

    panic_if(name_in_out == nullptr || *name_in_out == nullptr);

    symbol_t* class_symb = symbol_table_lookup(sema->ctx->global, *name_in_out);
    panic_if(class_symb == nullptr || class_symb->kind != SYMBOL_CLASS);

    // Make sure member is defined in class
    ast_member_decl_t* member_decl = hash_table_find(&class_symb->data.class.members, init->member_name);
    if (member_decl == nullptr)
    {
        *name_in_out = nullptr;
        semantic_context_add_error(sema->ctx, init, ssprintf("class has no member '%s'", init->member_name));
        return init;
    }

    init->init_expr = ast_transformer_transform(sema, init->init_expr, nullptr);
    if (init->init_expr->type == ast_type_invalid())
    {
        *name_in_out = nullptr;
        return init;
    }

    // Type of member and Expr must be compatible
    ast_coercion_kind_t coercion = check_coercion_with_expr(sema, init, init->init_expr, member_decl->base.type);
    if (coercion == COERCION_ALWAYS || coercion == COERCION_INIT)
        init->init_expr = ast_coercion_expr_create(init->init_expr, member_decl->base.type);
    else if (coercion != COERCION_EQUAL)
    {
        *name_in_out = nullptr;
        semantic_context_add_error(sema->ctx, init, ssprintf(
            "cannot coerce to '%s'", ast_type_string(member_decl->base.type)));
        return init;
    }

    *name_in_out = member_decl->base.name;
    return init;
}

static void* analyze_method_call(void* self_, ast_method_call_t* call, void* out_)
{
    semantic_analyzer_t* sema = self_;

    symbol_t* class_symb = verify_class_instance(sema, &call->instance, out_);
    if (class_symb == nullptr)
    {
        call->base.type = ast_type_invalid();
        return call;
    }

    // Make sure class contains the specified name as a method
    ast_method_def_t* method_def = hash_table_find(&class_symb->data.class.methods, call->method_name);
    if (method_def == nullptr)
    {
        semantic_context_add_error(sema->ctx, call, ssprintf("type '%s' has no method '%s'",
            call->instance->type->data.user.name, call->method_name));
        call->base.type = ast_type_invalid();
        return call;
    }

    if (!analyze_call_and_method_shared(sema, call, call->method_name, &method_def->base.params, &call->arguments))
    {
        call->base.type = ast_type_invalid();
        return call;
    }

    call->base.is_lvalue = false;
    call->base.type = method_def->base.return_type;
    return call;
}
static void* analyze_null_lit(void* self_, ast_null_lit_t* lit, void* out_)
{
    (void)self_;
    (void)out_;

    lit->base.is_lvalue = false;
    lit->base.type = ast_type_builtin(TYPE_NULL);
    return lit;
}

static void* analyze_paren_expr(void* self_, ast_paren_expr_t* paren, void* out_)
{
    semantic_analyzer_t* sema = self_;

    paren->expr = ast_transformer_transform(sema, paren->expr, out_);

    paren->base.is_lvalue = paren->expr->is_lvalue;
    paren->base.type = paren->expr->type;
    return paren;
}

static void* analyze_ref_expr(void* self_, ast_ref_expr_t* ref_expr, void* out_)
{
    semantic_analyzer_t* sema = self_;
    symbol_t** symbol_out = out_;

    symbol_t* symbol = symbol_table_lookup(sema->ctx->current, ref_expr->name);
    if (symbol == nullptr)
    {
        semantic_context_add_error(sema->ctx, ref_expr, ssprintf("unknown symbol name '%s'", ref_expr->name));
        ref_expr->base.type = ast_type_invalid();
        return ref_expr;
    }

    // Ref expr can transform to member access via implicit self expr
    if (symbol->kind == SYMBOL_MEMBER)
    {
        ast_expr_t* member_access = ast_member_access_create(ast_self_expr_create(true), symbol->name);
        ast_node_destroy(ref_expr);
        return ast_transformer_transform(sema, member_access, out_);  // visit member_access
    }

    if (symbol_out != nullptr)
        *symbol_out = symbol;

    if (!sema->is_lvalue_context)
    {
        if (!require_variable_initialized(sema, symbol, ref_expr))
        {
            ref_expr->base.type = ast_type_invalid();
            return ref_expr;
        }
    }

    ref_expr->base.is_lvalue = true;
    ref_expr->base.type = symbol->type;
    return ref_expr;
}

static void* analyze_self_expr(void* self_, ast_self_expr_t* self_expr, void* out_)
{
    semantic_analyzer_t* sema = self_;
    symbol_t** symbol_out = out_;

    if (sema->current_class == nullptr)
    {
        semantic_context_add_error(sema->ctx, self_expr, "'self' not valid in context");
        self_expr->base.type = ast_type_invalid();
        return self_expr;
    }

    symbol_t* symbol = symbol_table_lookup(sema->ctx->current, "self");
    panic_if(symbol == nullptr);
    if (symbol_out != nullptr)
        *symbol_out = symbol;

    self_expr->base.is_lvalue = true;
    self_expr->base.type = ast_type_pointer(ast_type_user(sema->current_class->base.name));
    return self_expr;
}

static void* analyze_str_lit(void* self_, ast_str_lit_t* lit, void* out_)
{
    (void)self_;
    (void)out_;

    lit->base.is_lvalue = false;
    lit->base.type = ast_type_builtin(TYPE_VOID);  // FIXME: We don't have a string type yet
    return lit;
}

static void* analyze_unary_op(void* self_, ast_unary_op_t* unary_op, void* out_)
{
    (void)out_;
    semantic_analyzer_t* sema = self_;

    symbol_t* symbol = nullptr;  // can be nullptr after visit
    unary_op->expr = ast_transformer_transform(sema, unary_op->expr, &symbol);

    switch (unary_op->op)
    {
        case TOKEN_AMPERSAND:
            if (!unary_op->expr->is_lvalue)
            {
                semantic_context_add_error(sema->ctx, unary_op, ssprintf("cannot take address of r-value '%s'",
                    symbol == nullptr ? "invalid expression" : symbol->name));
                unary_op->base.type = ast_type_invalid();
                return unary_op;
            }
            unary_op->base.is_lvalue = false;
            unary_op->base.type = ast_type_pointer(unary_op->expr->type);
            break;
        case TOKEN_STAR:
            if (unary_op->expr->type->kind != AST_TYPE_POINTER)
            {
                semantic_context_add_error(sema->ctx, unary_op, ssprintf("cannot dereference type '%s'",
                    ast_type_string(unary_op->expr->type)));
                unary_op->base.type = ast_type_invalid();
                return unary_op;
            }
            unary_op->base.is_lvalue = true;
            unary_op->base.type = unary_op->expr->type->data.pointer.pointee;
            break;
        default:
            panic("Unhandled op %d", unary_op->op);
    }

    return unary_op;
}

static void* analyze_uninit_lit(void* self_, ast_uninit_lit_t* lit, void* out_)
{
    (void)self_;
    (void)out_;

    lit->base.type = ast_type_builtin(TYPE_UNINIT);
    return lit;
}

static void* analyze_compound_statement(void* self_, ast_compound_stmt_t* block, void* out_)
{
    semantic_analyzer_t* sema = self_;

    semantic_context_push_scope(sema->ctx, SCOPE_BLOCK);

    for (size_t i = 0; i < vec_size(&block->inner_stmts); ++i)
        AST_TRANSFORMER_TRANSFORM_VEC(sema, &block->inner_stmts, i, out_);

    semantic_context_pop_scope(sema->ctx);
    return block;
}

static void* analyze_decl_stmt(void* self_, ast_decl_stmt_t* stmt, void* out_)
{
    semantic_analyzer_t* sema = self_;

    stmt->decl = ast_transformer_transform(sema, stmt->decl, out_);
    return stmt;
}

static void* analyze_expr_stmt(void* self_, ast_expr_stmt_t* stmt, void* out_)
{
    semantic_analyzer_t* sema = self_;

    stmt->expr = ast_transformer_transform(sema, stmt->expr, out_);
    return stmt;
}

static void* analyze_if_stmt(void* self_, ast_if_stmt_t* if_stmt, void* out_)
{
    semantic_analyzer_t* sema = self_;

    if_stmt->condition = ast_transformer_transform(sema, if_stmt->condition, out_);
    if (if_stmt->condition->type == ast_type_invalid())
        return if_stmt;  // avoid cascading errors

    if (if_stmt->condition->type != ast_type_builtin(TYPE_BOOL))
    {
        semantic_context_add_error(sema->ctx, if_stmt->condition,
            ssprintf("invalid expression type '%s' in if-condition: must be bool",
                ast_type_string(if_stmt->condition->type)));
    }

    init_tracker_t* then_tracker = sema->init_tracker;
    init_tracker_t* else_tracker = init_tracker_clone(sema->init_tracker);

    sema->init_tracker = then_tracker;
    if_stmt->then_branch = ast_transformer_transform(sema, if_stmt->then_branch, out_);
    then_tracker = sema->init_tracker;

    if (if_stmt->else_branch != nullptr)
    {
        sema->init_tracker = else_tracker;
        if_stmt->else_branch = ast_transformer_transform(sema, if_stmt->else_branch, out_);
        else_tracker = sema->init_tracker;
    }

    sema->init_tracker = init_tracker_merge(&then_tracker, &else_tracker);
    return if_stmt;
}

static void* analyze_return_stmt(void* self_, ast_return_stmt_t* ret_stmt, void* out_)
{
    semantic_analyzer_t* sema = self_;

    ret_stmt->value_expr = ast_transformer_transform(sema, ret_stmt->value_expr, out_);
    if (ret_stmt->value_expr->type == ast_type_invalid())
        return ret_stmt;  // avoid propagating error

    // Get return type from either current function or current method
    ast_type_t* return_type = sema->current_function ? sema->current_function->return_type :
        sema->current_method->base.return_type;

    ast_coercion_kind_t coercion = check_coercion_with_expr(sema, ret_stmt->value_expr, ret_stmt->value_expr,
        return_type);
    if (coercion == COERCION_INVALID)
        return ret_stmt;

    if (coercion == COERCION_ALWAYS || coercion == COERCION_WIDEN)
        ret_stmt->value_expr = ast_coercion_expr_create(ret_stmt->value_expr, return_type);

    return ret_stmt;
}

static void* analyze_while_stmt(void* self_, ast_while_stmt_t* while_stmt, void* out_)
{
    semantic_analyzer_t* sema = self_;

    while_stmt->condition = ast_transformer_transform(sema, while_stmt->condition, out_);
    if (while_stmt->condition->type == ast_type_invalid())
        return while_stmt;  // avoid cascading errors

    if (while_stmt->condition->type != ast_type_builtin(TYPE_BOOL))
    {
        semantic_context_add_error(sema->ctx, while_stmt->condition,
            ssprintf("invalid expression type '%s' in while-condition: must be bool",
                ast_type_string(while_stmt->condition->type)));
    }

    init_tracker_t* entry_state = sema->init_tracker;
    init_tracker_t* body_tracker = init_tracker_clone(sema->init_tracker);
    sema->init_tracker = body_tracker;

    while_stmt->body = ast_transformer_transform(sema, while_stmt->body, out_);

    // Discard init_tracker state changes inside while
    init_tracker_destroy(sema->init_tracker);
    sema->init_tracker = entry_state;
    return while_stmt;
}

semantic_analyzer_t* semantic_analyzer_create(semantic_context_t* ctx)
{
    semantic_analyzer_t* sema = malloc(sizeof(*sema));

    // NOTE: We do not need to init the visitor because we override every implementation
    *sema = (semantic_analyzer_t){
        .ctx = ctx,
        .init_tracker = init_tracker_create(),
        .base = (ast_transformer_t){
            .transform_root = analyze_root,
            // Declarations
            .transform_member_decl = analyze_member_decl,
            .transform_param_decl = analyze_param_decl,
            .transform_var_decl = analyze_var_decl,
            // Definitions
            .transform_class_def = analyze_class_def,
            .transform_fn_def = analyze_fn_def,
            .transform_method_def = analyze_method_def,
            // Expressions
            .transform_array_lit = analyze_array_lit,
            .transform_array_slice = analyze_array_slice,
            .transform_array_subscript = analyze_array_subscript,
            .transform_bin_op = analyze_bin_op,
            .transform_bool_lit = analyze_bool_lit,
            .transform_call_expr = analyze_call_expr,
            .transform_construct_expr = analyze_construct_expr,
            .transform_float_lit = analyze_float_lit,
            .transform_int_lit = analyze_int_lit,
            .transform_member_access = analyze_member_access,
            .transform_member_init = analyze_member_init,
            .transform_method_call = analyze_method_call,
            .transform_null_lit = analyze_null_lit,
            .transform_paren_expr = analyze_paren_expr,
            .transform_ref_expr = analyze_ref_expr,
            .transform_self_expr = analyze_self_expr,
            .transform_str_lit = analyze_str_lit,
            .transform_unary_op = analyze_unary_op,
            .transform_uninit_lit = analyze_uninit_lit,
            // Statements
            .transform_compound_stmt = analyze_compound_statement,
            .transform_decl_stmt = analyze_decl_stmt,
            .transform_expr_stmt = analyze_expr_stmt,
            .transform_if_stmt = analyze_if_stmt,
            .transform_return_stmt = analyze_return_stmt,
            .transform_while_stmt = analyze_while_stmt,
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
    ast_transformer_transform(sema, root, nullptr);
    return errors == vec_size(&sema->ctx->error_nodes);  // no new errors
}
