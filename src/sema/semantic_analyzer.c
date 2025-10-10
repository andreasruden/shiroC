#include "semantic_analyzer.h"

#include "ast/node.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/type.h"
#include "ast/visitor.h"
#include "common/debug/panic.h"
#include "common/util/ssprintf.h"
#include "parser/lexer.h"
#include "sema/init_tracker.h"
#include "sema/semantic_context.h"
#include "sema/symbol.h"
#include "sema/symbol_table.h"

struct semantic_analyzer
{
    ast_visitor_t base;
    semantic_context_t* ctx;  // decl_collector does not own ctx
    ast_fn_def_t* current_function;
    init_tracker_t* init_tracker;
    bool in_lvalue_context;
};

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

    if (var->init_expr != nullptr)
        ast_visitor_visit(sema, var->init_expr, nullptr);

    symbol_t* symbol = add_variable_to_scope(sema, var, var->name,
        var->init_expr == nullptr ? var->type : var->init_expr->type);
    if (symbol != nullptr)
        init_tracker_set_initialized(sema->init_tracker, symbol, var->init_expr != nullptr);
}

static void analyze_fn_def(void* self_, ast_fn_def_t* fn, void* out_)
{
    semantic_analyzer_t* sema = self_;

    sema->current_function = fn;
    semantic_context_push_scope(sema->ctx, SCOPE_FUNCTION);
    init_tracker_t* previous_tracker = sema->init_tracker;
    sema->init_tracker = init_tracker_create();

    if (fn->return_type == nullptr)
        fn->return_type = ast_type_from_builtin(TYPE_VOID);

    for (size_t i = 0; i < vec_size(&fn->params); ++i)
        ast_visitor_visit(sema, vec_get(&fn->params, i), out_);

    ast_visitor_visit(sema, fn->body, out_);

    panic_if(AST_KIND(fn->body) != AST_STMT_COMPOUND);
    ast_compound_stmt_t* block = (ast_compound_stmt_t*)fn->body;
    if (!ast_type_equal(fn->return_type, ast_type_from_builtin(TYPE_VOID)) &&
        AST_KIND(vec_last(&block->inner_stmts)) != AST_STMT_RETURN)
    {
        semantic_context_add_error(sema->ctx, fn, ssprintf("'%s' missing return statement", fn->base.name));
    }

    init_tracker_destroy(sema->init_tracker);
    sema->init_tracker = previous_tracker;
    semantic_context_pop_scope(sema->ctx);
    sema->current_function = nullptr;
}

static void analyze_bin_op_assignment(semantic_analyzer_t* sema, ast_bin_op_t* bin_op)
{
    symbol_t* lhs_symbol = nullptr;
    sema->in_lvalue_context = bin_op->op == TOKEN_ASSIGN;
    ast_visitor_visit(sema, bin_op->lhs, &lhs_symbol);
    sema->in_lvalue_context = false;
    if (lhs_symbol == nullptr || (lhs_symbol->kind != SYMBOL_VARIABLE && lhs_symbol->kind != SYMBOL_PARAMETER))
    {
        semantic_context_add_error(sema->ctx, bin_op->lhs, "cannot be assigned to");
        return;
    }

    init_tracker_set_initialized(sema->init_tracker, lhs_symbol, true);

    ast_visitor_visit(sema, bin_op->rhs, nullptr);
    if (bin_op->rhs->type == ast_type_invalid())
        return;  // avoid cascading errors

    if (!ast_type_equal(lhs_symbol->type, bin_op->rhs->type))
    {
        semantic_context_add_error(sema->ctx, bin_op->lhs,
            ssprintf("'%s' type is '%s' but assigned expression type is '%s'", lhs_symbol->name,
                ast_type_string(lhs_symbol->type), ast_type_string(bin_op->rhs->type)));
        return;
    }

    bin_op->base.type = lhs_symbol->type;
}

static bool is_type_valid_for_operator(ast_type_t* type, token_type_t operator, ast_type_t** result_type)
{
    if (type->kind != AST_TYPE_BUILTIN)
    {
        *result_type = ast_type_invalid();
        return false;
    }

    if (token_type_is_arithmetic_op(operator))
    {
        if (ast_type_is_arithmetic(type))
        {
            *result_type = ast_type_from_builtin(type->data.builtin.type);
            return true;
        }
        else
        {
            *result_type = ast_type_invalid();
            return false;
        }
    }

    if ((operator == TOKEN_EQ || operator == TOKEN_NEQ) && ast_type_equal(type, ast_type_from_builtin(TYPE_BOOL)))
    {
        *result_type = ast_type_from_builtin(TYPE_BOOL);
        return true;
    }

    if (token_type_is_relation_op(operator))
    {
        if (ast_type_is_arithmetic(type))
        {
            *result_type = ast_type_from_builtin(TYPE_BOOL);
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

    ast_visitor_visit(sema, bin_op->lhs, nullptr);
    ast_visitor_visit(sema, bin_op->rhs, nullptr);
    if (bin_op->lhs->type == ast_type_invalid() || bin_op->rhs->type == ast_type_invalid())
        return;  // avoid cascading errors

    if (!ast_type_equal(bin_op->lhs->type, bin_op->rhs->type))
    {
        semantic_context_add_error(sema->ctx, bin_op, ssprintf("LHS type '%s' != RHS type '%s'",
            ast_type_string(bin_op->lhs->type), ast_type_string(bin_op->rhs->type)));
        return;
    }

    ast_type_t* result_type = nullptr;
    if (!is_type_valid_for_operator(bin_op->lhs->type, bin_op->op, &result_type))
    {
        semantic_context_add_error(sema->ctx, bin_op, ssprintf("cannot apply '%s' to LHS type and '%s' RHS type '%s'",
            token_type_str(bin_op->op), ast_type_string(bin_op->lhs->type), ast_type_string(bin_op->rhs->type)));
        return;
    }

    bin_op->base.type = result_type;
}

static void analyze_call_expr(void* self_, ast_call_expr_t* call, void* out_)
{
    semantic_analyzer_t* sema = self_;

    symbol_t* symbol = nullptr;
    ast_visitor_visit(sema, call->function, &symbol);

    if (symbol != nullptr && symbol->kind != SYMBOL_FUNCTION)
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

        if (!ast_type_equal(param_decl->type, arg_expr->type))
        {
            semantic_context_add_error(sema->ctx, arg_expr,
                ssprintf("arg type '%s' does not match parameter '%s' type '%s'", ast_type_string(arg_expr->type),
                    param_decl->name, ast_type_string(param_decl->type)));
            return;
        }
    }
}

static void analyze_int_lit(void* self_, ast_int_lit_t* lit, void* out_)
{
    (void)self_;
    (void)out_;

    if (lit->value > INT32_MAX)
        lit->base.type = ast_type_from_builtin(TYPE_I64);
    else
        lit->base.type = ast_type_from_builtin(TYPE_I32);
}

static void analyze_paren_expr(void* self_, ast_paren_expr_t* paren, void* out_)
{
    semantic_analyzer_t* sema = self_;

    ast_visitor_visit(sema, paren->expr, out_);
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

    if (!sema->in_lvalue_context)
    {
        if (!require_variable_initialized(sema, symbol, ref_expr))
            return;
    }

    ref_expr->base.type = symbol->type;
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

    if (!ast_type_equal(if_stmt->condition->type, ast_type_from_builtin(TYPE_BOOL)))
    {
        semantic_context_add_error(sema->ctx, if_stmt->condition,
            ssprintf("invalid expression type '%s' in if-condition: must be bool",
                ast_type_string(if_stmt->condition->type)));
    }

    init_tracker_t* then_tracker = sema->init_tracker;
    init_tracker_t* else_tracker = init_tracker_clone(sema->init_tracker);

    sema->init_tracker = then_tracker;
    ast_visitor_visit(sema, if_stmt->then_branch, out_);

    if (if_stmt->else_branch != nullptr)
    {
        sema->init_tracker = else_tracker;
        ast_visitor_visit(sema, if_stmt->else_branch, out_);
    }

    sema->init_tracker = init_tracker_merge(&then_tracker, &else_tracker);
}

static void analyze_return_stmt(void* self_, ast_return_stmt_t* ret_stmt, void* out_)
{
    semantic_analyzer_t* sema = self_;

    ast_visitor_visit(sema, ret_stmt->value_expr, out_);
    if (!ast_type_equal(sema->current_function->return_type, ret_stmt->value_expr->type))
    {
        semantic_context_add_error(sema->ctx, ret_stmt->value_expr,
            ssprintf("returned type '%s' does not match function's return type '%s'",
                ast_type_string(ret_stmt->value_expr->type), ast_type_string(sema->current_function->return_type)));
    }
}

static void analyze_while_stmt(void* self_, ast_while_stmt_t* while_stmt, void* out_)
{
    semantic_analyzer_t* sema = self_;

    ast_visitor_visit(sema, while_stmt->condition, out_);
    if (while_stmt->condition->type == ast_type_invalid())
        return;  // avoid cascading errors

    if (!ast_type_equal(while_stmt->condition->type, ast_type_from_builtin(TYPE_BOOL)))
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
    init_tracker_destroy(body_tracker);
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
            .visit_bin_op = analyze_bin_op,
            .visit_call_expr = analyze_call_expr,
            .visit_int_lit = analyze_int_lit,
            .visit_paren_expr = analyze_paren_expr,
            .visit_ref_expr = analyze_ref_expr,
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
