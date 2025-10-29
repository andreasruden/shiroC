#include "cloner.h"

#include "ast/decl/decl.h"
#include "ast/def/class_def.h"
#include "ast/def/fn_def.h"
#include "ast/expr/array_lit.h"
#include "ast/expr/array_slice.h"
#include "ast/expr/array_subscript.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/bool_lit.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/coercion_expr.h"
#include "ast/expr/construct_expr.h"
#include "ast/expr/float_lit.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/member_access.h"
#include "ast/expr/member_init.h"
#include "ast/expr/method_call.h"
#include "ast/expr/null_lit.h"
#include "ast/expr/paren_expr.h"
#include "ast/expr/ref_expr.h"
#include "ast/expr/self_expr.h"
#include "ast/expr/str_lit.h"
#include "ast/expr/unary_op.h"
#include "ast/expr/uninit_lit.h"
#include "ast/decl/member_decl.h"
#include "ast/decl/param_decl.h"
#include "ast/decl/type_param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/method_def.h"
#include "ast/stmt/break_stmt.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/continue_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/for_stmt.h"
#include "ast/stmt/if_stmt.h"
#include "ast/stmt/inc_dec_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "ast/stmt/stmt.h"
#include "ast/stmt/while_stmt.h"
#include "ast/visitor.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include <string.h>

typedef struct cloner_visitor
{
    ast_visitor_t base;
    ast_expr_t* result;
} cloner_visitor_t;

static void clone_int_lit(void* self_, ast_int_lit_t* lit, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    cloner->result = ast_int_lit_create(lit->has_minus_sign, lit->value.magnitude, lit->suffix);
}

static void clone_float_lit(void* self_, ast_float_lit_t* lit, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    cloner->result = ast_float_lit_create(lit->value, lit->suffix);
}

static void clone_bool_lit(void* self_, ast_bool_lit_t* lit, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    cloner->result = ast_bool_lit_create(lit->value);
}

static void clone_str_lit(void* self_, ast_str_lit_t* lit, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    cloner->result = ast_str_lit_create(lit->value);
}

static void clone_null_lit(void* self_, ast_null_lit_t* lit, void* out_)
{
    (void)out_;
    (void)lit;
    cloner_visitor_t* cloner = self_;
    cloner->result = ast_null_lit_create();
}

static void clone_uninit_lit(void* self_, ast_uninit_lit_t* lit, void* out_)
{
    (void)out_;
    (void)lit;
    cloner_visitor_t* cloner = self_;
    cloner->result = ast_uninit_lit_create();
}

static void clone_ref_expr(void* self_, ast_ref_expr_t* ref, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    cloner->result = ast_ref_expr_create(ref->name);
}

static void clone_self_expr(void* self_, ast_self_expr_t* self_expr, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    cloner->result = ast_self_expr_create(self_expr->implicit);
}

static void clone_bin_op(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    ast_expr_t* lhs = ast_expr_clone(bin_op->lhs);
    ast_expr_t* rhs = ast_expr_clone(bin_op->rhs);
    cloner->result = ast_bin_op_create(bin_op->op, lhs, rhs);
}

static void clone_unary_op(void* self_, ast_unary_op_t* unary_op, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    ast_expr_t* expr = ast_expr_clone(unary_op->expr);
    cloner->result = ast_unary_op_create(unary_op->op, expr);
}

static void clone_paren_expr(void* self_, ast_paren_expr_t* paren, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    ast_expr_t* inner = ast_expr_clone(paren->expr);
    cloner->result = ast_paren_expr_create(inner);
}

static void clone_access_expr(void* self_, ast_access_expr_t* access_expr, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    ast_expr_t* outer = ast_expr_clone(access_expr->outer);
    ast_expr_t* inner = ast_expr_clone(access_expr->inner);
    cloner->result = ast_access_expr_create(outer, inner);
}

static void clone_member_access(void* self_, ast_member_access_t* access, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    ast_expr_t* instance = ast_expr_clone(access->instance);
    cloner->result = ast_member_access_create(instance, access->member_name);
}

static void clone_array_subscript(void* self_, ast_array_subscript_t* subscript, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    ast_expr_t* array = ast_expr_clone(subscript->array);
    ast_expr_t* index = ast_expr_clone(subscript->index);
    cloner->result = ast_array_subscript_create(array, index);
}

static void clone_array_slice(void* self_, ast_array_slice_t* slice, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    ast_expr_t* array = ast_expr_clone(slice->array);
    ast_expr_t* start = slice->start ? ast_expr_clone(slice->start) : nullptr;
    ast_expr_t* end = slice->end ? ast_expr_clone(slice->end) : nullptr;
    cloner->result = ast_array_slice_create(array, start, end);
}

static void clone_array_lit(void* self_, ast_array_lit_t* lit, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    vec_t exprs = VEC_INIT(ast_node_destroy);
    for (size_t i = 0; i < vec_size(&lit->exprs); ++i)
    {
        ast_expr_t* elem = vec_get(&lit->exprs, i);
        vec_push(&exprs, ast_expr_clone(elem));
    }
    cloner->result = ast_array_lit_create(&exprs);
}

static void clone_call_expr(void* self_, ast_call_expr_t* call, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    ast_expr_t* function = ast_expr_clone(call->function);
    vec_t arguments = VEC_INIT(ast_node_destroy);
    for (size_t i = 0; i < vec_size(&call->arguments); ++i)
    {
        ast_expr_t* arg = vec_get(&call->arguments, i);
        vec_push(&arguments, ast_expr_clone(arg));
    }
    cloner->result = ast_call_expr_create(function, &arguments);
}

static void clone_method_call(void* self_, ast_method_call_t* call, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    ast_expr_t* instance = ast_expr_clone(call->instance);
    vec_t arguments = VEC_INIT(ast_node_destroy);
    for (size_t i = 0; i < vec_size(&call->arguments); ++i)
    {
        ast_expr_t* arg = vec_get(&call->arguments, i);
        vec_push(&arguments, ast_expr_clone(arg));
    }
    cloner->result = ast_method_call_create(instance, call->method_name, &arguments);
}

static void clone_construct_expr(void* self_, ast_construct_expr_t* construct, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    vec_t member_inits = VEC_INIT(ast_node_destroy);
    for (size_t i = 0; i < vec_size(&construct->member_inits); ++i)
    {
        ast_member_init_t* init = vec_get(&construct->member_inits, i);
        ast_expr_t* init_expr = ast_expr_clone(init->init_expr);
        vec_push(&member_inits, ast_member_init_create(init->member_name, init_expr));
    }
    cloner->result = ast_construct_expr_create(construct->class_type, &member_inits);
}

static void clone_coercion_expr(void* self_, ast_coercion_expr_t* coercion, void* out_)
{
    (void)out_;
    cloner_visitor_t* cloner = self_;
    ast_expr_t* expr = ast_expr_clone(coercion->expr);
    cloner->result = ast_coercion_expr_create(expr, coercion->target);
}

ast_expr_t* ast_expr_clone(ast_expr_t* expr)
{
    if (expr == nullptr)
        return nullptr;

    cloner_visitor_t cloner = {0};
    ast_visitor_init(&cloner.base);

    // Override visit functions
    cloner.base.visit_int_lit = clone_int_lit;
    cloner.base.visit_float_lit = clone_float_lit;
    cloner.base.visit_bool_lit = clone_bool_lit;
    cloner.base.visit_str_lit = clone_str_lit;
    cloner.base.visit_null_lit = clone_null_lit;
    cloner.base.visit_uninit_lit = clone_uninit_lit;
    cloner.base.visit_ref_expr = clone_ref_expr;
    cloner.base.visit_self_expr = clone_self_expr;
    cloner.base.visit_bin_op = clone_bin_op;
    cloner.base.visit_unary_op = clone_unary_op;
    cloner.base.visit_paren_expr = clone_paren_expr;
    cloner.base.visit_access_expr = clone_access_expr;
    cloner.base.visit_member_access = clone_member_access;
    cloner.base.visit_array_subscript = clone_array_subscript;
    cloner.base.visit_array_slice = clone_array_slice;
    cloner.base.visit_array_lit = clone_array_lit;
    cloner.base.visit_call_expr = clone_call_expr;
    cloner.base.visit_method_call = clone_method_call;
    cloner.base.visit_construct_expr = clone_construct_expr;
    cloner.base.visit_coercion_expr = clone_coercion_expr;

    ast_visitor_visit(&cloner.base, AST_NODE(expr), nullptr);

    panic_if(cloner.result == nullptr);
    cloner.result->type = expr->type;
    return cloner.result;
}

ast_stmt_t* ast_stmt_clone(ast_stmt_t* stmt)
{
    if (stmt == nullptr)
        return nullptr;

    switch (AST_KIND(stmt))
    {
        case AST_STMT_COMPOUND:
        {
            ast_compound_stmt_t* compound = (ast_compound_stmt_t*)stmt;
            vec_t cloned_stmts = VEC_INIT(ast_node_destroy);
            for (size_t i = 0; i < vec_size(&compound->inner_stmts); ++i)
            {
                ast_stmt_t* inner = vec_get(&compound->inner_stmts, i);
                vec_push(&cloned_stmts, ast_stmt_clone(inner));
            }
            return ast_compound_stmt_create(&cloned_stmts);
        }
        case AST_STMT_EXPR:
        {
            ast_expr_stmt_t* expr_stmt = (ast_expr_stmt_t*)stmt;
            return ast_expr_stmt_create(ast_expr_clone(expr_stmt->expr));
        }
        case AST_STMT_DECL:
        {
            ast_decl_stmt_t* decl_stmt = (ast_decl_stmt_t*)stmt;
            return ast_decl_stmt_create(ast_decl_clone(decl_stmt->decl));
        }
        case AST_STMT_RETURN:
        {
            ast_return_stmt_t* ret = (ast_return_stmt_t*)stmt;
            ast_expr_t* cloned_expr = ret->value_expr != nullptr ? ast_expr_clone(ret->value_expr) : nullptr;
            return ast_return_stmt_create(cloned_expr);
        }
        case AST_STMT_IF:
        {
            ast_if_stmt_t* if_stmt = (ast_if_stmt_t*)stmt;
            ast_expr_t* cloned_cond = ast_expr_clone(if_stmt->condition);
            ast_stmt_t* cloned_then = ast_stmt_clone(if_stmt->then_branch);
            ast_stmt_t* cloned_else = if_stmt->else_branch != nullptr ? ast_stmt_clone(if_stmt->else_branch) : nullptr;
            return ast_if_stmt_create(cloned_cond, cloned_then, cloned_else);
        }
        case AST_STMT_WHILE:
        {
            ast_while_stmt_t* while_stmt = (ast_while_stmt_t*)stmt;
            return ast_while_stmt_create(ast_expr_clone(while_stmt->condition), ast_stmt_clone(while_stmt->body));
        }
        case AST_STMT_FOR:
        {
            ast_for_stmt_t* for_stmt = (ast_for_stmt_t*)stmt;
            ast_stmt_t* cloned_init = for_stmt->init_stmt != nullptr ? ast_stmt_clone(for_stmt->init_stmt) : nullptr;
            ast_expr_t* cloned_cond = for_stmt->cond_expr != nullptr ? ast_expr_clone(for_stmt->cond_expr) : nullptr;
            ast_stmt_t* cloned_post = for_stmt->post_stmt != nullptr ? ast_stmt_clone(for_stmt->post_stmt) : nullptr;
            return ast_for_stmt_create(cloned_init, cloned_cond, cloned_post, ast_stmt_clone(for_stmt->body));
        }
        case AST_STMT_BREAK:
            return ast_break_stmt_create();
        case AST_STMT_CONTINUE:
            return ast_continue_stmt_create();
        case AST_STMT_INC_DEC:
        {
            ast_inc_dec_stmt_t* inc_dec = (ast_inc_dec_stmt_t*)stmt;
            return ast_inc_dec_stmt_create(ast_expr_clone(inc_dec->operand), inc_dec->increment);
        }
        default:
            panic("Unsupported statement kind for cloning: %d", AST_KIND(stmt));
    }
}

ast_decl_t* ast_decl_clone(ast_decl_t* decl)
{
    if (decl == nullptr)
        return nullptr;

    switch (AST_KIND(decl))
    {
        case AST_DECL_VAR:
        {
            ast_var_decl_t* var = (ast_var_decl_t*)decl;
            ast_expr_t* cloned_init = var->init_expr != nullptr ? ast_expr_clone(var->init_expr) : nullptr;
            return ast_var_decl_create(var->name, var->type, cloned_init);
        }
        case AST_DECL_PARAM:
        {
            ast_param_decl_t* param = (ast_param_decl_t*)decl;
            return ast_param_decl_create(param->name, param->type);
        }
        case AST_DECL_MEMBER:
        {
            ast_member_decl_t* member = (ast_member_decl_t*)decl;
            ast_expr_t* cloned_init = member->base.init_expr != nullptr ? ast_expr_clone(member->base.init_expr) : nullptr;
            return ast_member_decl_create(member->base.name, member->base.type, cloned_init);
        }
        case AST_DECL_TYPE_PARAM:
        {
            ast_type_param_decl_t* type_param = (ast_type_param_decl_t*)decl;
            return ast_type_param_decl_create(type_param->name);
        }
        default:
            panic("Unsupported declaration kind for cloning: %d", AST_KIND(decl));
    }
}

ast_fn_def_t* ast_fn_def_clone(ast_fn_def_t* fn)
{
    if (fn == nullptr)
        return nullptr;

    vec_t cloned_type_params = VEC_INIT(ast_node_destroy);
    for (size_t i = 0; i < vec_size(&fn->type_params); ++i)
    {
        ast_type_param_decl_t* type_param = vec_get(&fn->type_params, i);
        vec_push(&cloned_type_params, ast_decl_clone((ast_decl_t*)type_param));
    }

    vec_t cloned_params = VEC_INIT(ast_node_destroy);
    for (size_t i = 0; i < vec_size(&fn->params); ++i)
    {
        ast_param_decl_t* param = vec_get(&fn->params, i);
        vec_push(&cloned_params, ast_decl_clone((ast_decl_t*)param));
    }

    ast_stmt_t* cloned_body = fn->body != nullptr ? ast_stmt_clone(fn->body) : nullptr;

    ast_fn_def_t* cloned = (ast_fn_def_t*)ast_fn_def_create(fn->base.name, &cloned_params, fn->return_type, cloned_body,
        fn->exported);

    cloned->type_params = cloned_type_params;
    cloned->overload_index = fn->overload_index;
    if (fn->extern_abi != nullptr)
        cloned->extern_abi = strdup(fn->extern_abi);

    return cloned;
}

ast_class_def_t* ast_class_def_clone(ast_class_def_t* class_def)
{
    if (class_def == nullptr)
        return nullptr;

    vec_t cloned_type_params = VEC_INIT(ast_node_destroy);
    for (size_t i = 0; i < vec_size(&class_def->type_params); ++i)
    {
        ast_type_param_decl_t* type_param = vec_get(&class_def->type_params, i);
        vec_push(&cloned_type_params, ast_decl_clone((ast_decl_t*)type_param));
    }

    vec_t cloned_members = VEC_INIT(ast_node_destroy);
    for (size_t i = 0; i < vec_size(&class_def->members); ++i)
    {
        ast_member_decl_t* member = vec_get(&class_def->members, i);
        vec_push(&cloned_members, ast_decl_clone((ast_decl_t*)member));
    }

    vec_t cloned_methods = VEC_INIT(ast_node_destroy);
    for (size_t i = 0; i < vec_size(&class_def->methods); ++i)
    {
        ast_method_def_t* method = vec_get(&class_def->methods, i);

        vec_t cloned_method_params = VEC_INIT(ast_node_destroy);
        for (size_t j = 0; j < vec_size(&method->base.params); ++j)
        {
            ast_param_decl_t* param = vec_get(&method->base.params, j);
            vec_push(&cloned_method_params, ast_decl_clone((ast_decl_t*)param));
        }

        ast_stmt_t* cloned_method_body = method->base.body != nullptr ? ast_stmt_clone(method->base.body) : nullptr;

        ast_method_def_t* cloned_method = (ast_method_def_t*)ast_method_def_create(method->base.base.name,
            &cloned_method_params, method->base.return_type, cloned_method_body);

        vec_push(&cloned_methods, cloned_method);
    }

    ast_class_def_t* cloned = (ast_class_def_t*)ast_class_def_create(class_def->base.name, &cloned_members,
        &cloned_methods, class_def->exported);

    cloned->type_params = cloned_type_params;

    return cloned;
}
