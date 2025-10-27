#include "ast/decl/member_decl.h"
#include "ast/decl/param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/class_def.h"
#include "ast/def/fn_def.h"
#include "ast/def/method_def.h"
#include "ast/expr/access_expr.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/construct_expr.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/member_access.h"
#include "ast/expr/member_init.h"
#include "ast/expr/method_call.h"
#include "ast/expr/ref_expr.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "ast/type.h"
#include "sema/decl_collector.h"
#include "sema/semantic_analyzer.h"
#include "sema/semantic_context.h"
#include "sema/symbol.h"
#include "sema_shared.h"
#include "test_runner.h"

#include <string.h>

TEST_FIXTURE(ut_sema_access_fixture_t)
{
    semantic_analyzer_t* sema;
    semantic_context_t* ctx;
    decl_collector_t* collector;
};

TEST_SETUP(ut_sema_access_fixture_t)
{
    fix->ctx = semantic_context_create(nullptr, "TestModule");  // nullptr = own module, uses "Self" namespace
    ASSERT_NEQ(nullptr, fix->ctx);

    fix->collector = decl_collector_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->collector);

    fix->sema = semantic_analyzer_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->sema);
}

TEST_TEARDOWN(ut_sema_access_fixture_t)
{
    semantic_analyzer_destroy(fix->sema);
    decl_collector_destroy(fix->collector);
    semantic_context_destroy(fix->ctx);
    ast_type_cache_reset();
}

TEST(ut_sema_access_fixture_t, simple_member_access_on_variable)
{
    ast_def_t* class_def = ast_class_def_create_va("Point",
        ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
        nullptr);

    ast_decl_t* var = ast_var_decl_create("p", ast_type_user_unresolved("Point"), nullptr);

    // Create access expression: p.x
    ast_expr_t* access = ast_access_expr_create(
        ast_ref_expr_create("p"),
        ast_ref_expr_create("x"));
    ast_stmt_t* return_stmt = ast_return_stmt_create(access);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(var),
            return_stmt,
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(class_def, fn, nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    // Verify transformation: access_expr should be transformed to member_access
    ast_return_stmt_t* ret = (ast_return_stmt_t*)return_stmt;
    ASSERT_NEQ(nullptr, ret->value_expr);
    ASSERT_EQ(AST_EXPR_MEMBER_ACCESS, AST_KIND(ret->value_expr));

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, chained_member_access)
{
    ast_def_t* inner_class = ast_class_def_create_va("Inner",
        ast_member_decl_create("value", ast_type_builtin(TYPE_I32), nullptr),
        nullptr);

    ast_def_t* outer_class = ast_class_def_create_va("Outer",
        ast_member_decl_create("inner", ast_type_user_unresolved("Inner"), nullptr),
        nullptr);

    // Create function accessing nested member: obj.inner.value
    ast_decl_t* var = ast_var_decl_create("obj", ast_type_user_unresolved("Outer"), nullptr);

    ast_expr_t* access = ast_access_expr_create(
        ast_access_expr_create(
            ast_ref_expr_create("obj"),
            ast_ref_expr_create("inner")),
        ast_ref_expr_create("value"));

    ast_stmt_t* return_stmt = ast_return_stmt_create(access);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(var),
            return_stmt,
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(inner_class, outer_class, fn, nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    // Verify transformation: outer access_expr should be transformed to member_access
    ast_return_stmt_t* ret = (ast_return_stmt_t*)return_stmt;
    ASSERT_NEQ(nullptr, ret->value_expr);
    ASSERT_EQ(AST_EXPR_MEMBER_ACCESS, AST_KIND(ret->value_expr));

    // Verify nested transformation: the instance should also be a member_access
    ast_member_access_t* outer_access = (ast_member_access_t*)ret->value_expr;
    ASSERT_NEQ(nullptr, outer_access->instance);
    ASSERT_EQ(AST_EXPR_MEMBER_ACCESS, AST_KIND(outer_access->instance));
    ASSERT_EQ(outer_access->member_name, "value");

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, access_undefined_member)
{
    ast_def_t* class_def = ast_class_def_create_va("Point",
        ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
        nullptr);

    ast_decl_t* var = ast_var_decl_create("p", ast_type_user_unresolved("Point"), nullptr);

    // Try to access undefined member: p.y
    ast_expr_t* access = ast_access_expr_create(
        ast_ref_expr_create("p"),
        ast_ref_expr_create("y"));

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(var),
            ast_return_stmt_create(access),
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(class_def, fn, nullptr);

    decl_collector_run(fix->collector, AST_NODE(root));
    bool res = semantic_analyzer_run(fix->sema, AST_NODE(root));
    ASSERT_FALSE(res);
    ASSERT_LT(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, access_undefined_variable)
{
    // Try to access member on undefined variable: undefined.x
    ast_expr_t* access = ast_access_expr_create(
        ast_ref_expr_create("undefined"),
        ast_ref_expr_create("x"));

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(access),
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(fn, nullptr);

    decl_collector_run(fix->collector, AST_NODE(root));
    bool res = semantic_analyzer_run(fix->sema, AST_NODE(root));
    ASSERT_FALSE(res);
    ASSERT_LT(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, qualified_name_from_namespace)
{
    ast_def_t* test_func = ast_fn_def_create_va("testFunc", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(ast_int_lit_val(42)), nullptr
        ), nullptr);
    symbol_t* test_func_symb = symbol_create("testFunc", SYMBOL_FUNCTION, test_func, fix->ctx->module_namespace);
    test_func_symb->data.function.return_type = ast_type_builtin(TYPE_I32);
    symbol_table_insert(fix->ctx->global, test_func_symb);

    // Create access to namespace function: Self.TestModule.testFunc()
    ast_expr_t* qualified_ref = ast_access_expr_create(
        ast_access_expr_create(
            ast_ref_expr_create("Self"),
            ast_ref_expr_create("TestModule")),
        ast_ref_expr_create("testFunc"));

    ast_expr_t* call = ast_call_expr_create_va(qualified_ref, nullptr);

    ast_stmt_t* return_stmt = ast_return_stmt_create(call);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            return_stmt,
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(fn, nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    // Verify transformation: return value should be a call expression
    ast_return_stmt_t* ret = (ast_return_stmt_t*)return_stmt;
    ASSERT_NEQ(nullptr, ret->value_expr);
    ASSERT_EQ(AST_EXPR_CALL, AST_KIND(ret->value_expr));

    // Verify the call's function field is no longer an access_expr
    ast_call_expr_t* call_expr = (ast_call_expr_t*)ret->value_expr;
    ASSERT_NEQ(nullptr, call_expr->function);
    ASSERT_NEQ(AST_EXPR_ACCESS, AST_KIND(call_expr->function));

    ast_node_destroy(root);
    ast_node_destroy(test_func);
}

TEST(ut_sema_access_fixture_t, undefined_namespace)
{
    // Try to access undefined namespace: UndefinedProject.Module.Func()
    ast_expr_t* qualified_ref = ast_access_expr_create(
        ast_access_expr_create(
            ast_ref_expr_create("UndefinedProject"),
            ast_ref_expr_create("Module")),
        ast_ref_expr_create("Func"));

    ast_expr_t* call = ast_call_expr_create_va(qualified_ref, nullptr);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(call),
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(fn, nullptr);

    decl_collector_run(fix->collector, AST_NODE(root));
    bool res = semantic_analyzer_run(fix->sema, AST_NODE(root));
    ASSERT_FALSE(res);
    ASSERT_LT(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, undefined_symbol_in_namespace)
{
    // Try to access undefined function in namespace: Self.TestModule.undefinedFunc()
    ast_expr_t* qualified_ref = ast_access_expr_create(
        ast_access_expr_create(
            ast_ref_expr_create("Self"),
            ast_ref_expr_create("TestModule")),
        ast_ref_expr_create("undefinedFunc"));

    ast_expr_t* call = ast_call_expr_create_va(qualified_ref, nullptr);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(call),
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(fn, nullptr);

    decl_collector_run(fix->collector, AST_NODE(root));
    bool res = semantic_analyzer_run(fix->sema, AST_NODE(root));
    ASSERT_FALSE(res);
    ASSERT_LT(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, simple_method_call_transformation)
{
    ast_def_t* class_def = ast_class_def_create_va("Calculator",
        ast_method_def_create_va("compute", ast_type_builtin(TYPE_I32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(ast_int_lit_val(42)),
                nullptr),
            nullptr),
        nullptr);

    ast_decl_t* var = ast_var_decl_create("calc", ast_type_user_unresolved("Calculator"),
        ast_construct_expr_create_va(ast_type_user_unresolved("Calculator"), nullptr));

    // Parser would create: call_expr(access_expr(ref("calc"), ref("compute")))
    ast_expr_t* access = ast_access_expr_create(
        ast_ref_expr_create("calc"),
        ast_ref_expr_create("compute"));

    ast_expr_t* call = ast_call_expr_create_va(access, nullptr);

    ast_stmt_t* return_stmt = ast_return_stmt_create(call);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(var),
            return_stmt,
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(class_def, fn, nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    // Verify transformation: call_expr should be transformed to method_call
    ast_return_stmt_t* ret = (ast_return_stmt_t*)return_stmt;
    ASSERT_NEQ(nullptr, ret->value_expr);
    ASSERT_EQ(AST_EXPR_METHOD_CALL, AST_KIND(ret->value_expr));

    // Verify method_call details
    ast_method_call_t* method_call = (ast_method_call_t*)ret->value_expr;
    ASSERT_NEQ(nullptr, method_call->instance);
    ASSERT_EQ(0, strcmp("compute", method_call->method_name));
    ASSERT_NEQ(nullptr, method_call->method_symbol);
    ASSERT_EQ(ast_type_builtin(TYPE_I32), method_call->base.type);

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, chained_member_access_then_method_call)
{
    ast_def_t* inner_class = ast_class_def_create_va("Inner",
        ast_method_def_create_va("getValue", ast_type_builtin(TYPE_I32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(ast_int_lit_val(100)),
                nullptr),
            nullptr),
        nullptr);

    ast_def_t* outer_class = ast_class_def_create_va("Outer",
        ast_member_decl_create("inner", ast_type_user_unresolved("Inner"), nullptr),
        nullptr);

    ast_decl_t* var = ast_var_decl_create("obj", ast_type_user_unresolved("Outer"), nullptr);

    // Parser creates: call_expr(access_expr(access_expr(ref("obj"), ref("inner")), ref("getValue")))
    ast_expr_t* access_chain = ast_access_expr_create(
        ast_access_expr_create(
            ast_ref_expr_create("obj"),
            ast_ref_expr_create("inner")),
        ast_ref_expr_create("getValue"));

    ast_expr_t* call = ast_call_expr_create_va(access_chain, nullptr);
    ast_stmt_t* return_stmt = ast_return_stmt_create(call);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(var),
            return_stmt,
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(inner_class, outer_class, fn, nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    // Verify transformation: should be method_call
    ast_return_stmt_t* ret = (ast_return_stmt_t*)return_stmt;
    ASSERT_EQ(AST_EXPR_METHOD_CALL, AST_KIND(ret->value_expr));

    // Verify method_call has member_access as instance
    ast_method_call_t* method_call = (ast_method_call_t*)ret->value_expr;
    ASSERT_EQ(0, strcmp("getValue", method_call->method_name));
    ASSERT_NEQ(nullptr, method_call->instance);
    ASSERT_EQ(AST_EXPR_MEMBER_ACCESS, AST_KIND(method_call->instance));

    // Verify the member_access is correct (obj.inner)
    ast_member_access_t* member_access = (ast_member_access_t*)method_call->instance;
    ASSERT_EQ(0, strcmp("inner", member_access->member_name));
    ASSERT_EQ(AST_EXPR_REF, AST_KIND(member_access->instance));

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, method_call_with_arguments)
{
    ast_def_t* class_def = ast_class_def_create_va("Calculator",
        ast_method_def_create_va("add", ast_type_builtin(TYPE_I32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_bin_op_create(TOKEN_PLUS,
                        ast_ref_expr_create("a"),
                        ast_ref_expr_create("b"))),
                nullptr),
            ast_param_decl_create("a", ast_type_builtin(TYPE_I32)),
            ast_param_decl_create("b", ast_type_builtin(TYPE_I32)),
            nullptr),
        nullptr);

    // Create call: calc.add(10, 20)
    ast_decl_t* var = ast_var_decl_create("calc", ast_type_user_unresolved("Calculator"),
        ast_construct_expr_create_va(ast_type_user_unresolved("Calculator"), nullptr));

    ast_expr_t* access = ast_access_expr_create(
        ast_ref_expr_create("calc"),
        ast_ref_expr_create("add"));

    ast_expr_t* call = ast_call_expr_create_va(access,
        ast_int_lit_val(10),
        ast_int_lit_val(20),
        nullptr);

    ast_stmt_t* return_stmt = ast_return_stmt_create(call);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(var),
            return_stmt,
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(class_def, fn, nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    // Verify transformation and argument preservation
    ast_return_stmt_t* ret = (ast_return_stmt_t*)return_stmt;
    ASSERT_EQ(AST_EXPR_METHOD_CALL, AST_KIND(ret->value_expr));

    ast_method_call_t* method_call = (ast_method_call_t*)ret->value_expr;
    ASSERT_EQ(0, strcmp("add", method_call->method_name));
    ASSERT_EQ(2, vec_size(&method_call->arguments));

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, call_data_member_as_method)
{
    ast_def_t* class_def = ast_class_def_create_va("Point",
        ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
        nullptr);

    ast_decl_t* var = ast_var_decl_create("p", ast_type_user_unresolved("Point"),
        ast_construct_expr_create_va(ast_type_user_unresolved("Point"),
            ast_member_init_create("x", ast_int_lit_val(5)),
            nullptr));

    // Try to call data member as method: p.x()
    ast_expr_t* access = ast_access_expr_create(
        ast_ref_expr_create("p"),
        ast_ref_expr_create("x"));

    ast_expr_t* call = ast_call_expr_create_va(access, nullptr);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(var),
            ast_return_stmt_create(call),
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(class_def, fn, nullptr);

    // Should fail with error
    decl_collector_run(fix->collector, AST_NODE(root));
    bool res = semantic_analyzer_run(fix->sema, AST_NODE(root));
    ASSERT_FALSE(res);
    ASSERT_LT(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, call_undefined_method)
{
    ast_def_t* class_def = ast_class_def_create_va("Point",
        ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
        nullptr);

    ast_decl_t* var = ast_var_decl_create("p", ast_type_user_unresolved("Point"),
        ast_construct_expr_create_va(ast_type_user_unresolved("Point"),
            ast_member_init_create("x", ast_int_lit_val(5)),
            nullptr));

    // Try to call non-existent method: p.compute()
    ast_expr_t* access = ast_access_expr_create(
        ast_ref_expr_create("p"),
        ast_ref_expr_create("compute"));

    ast_expr_t* call = ast_call_expr_create_va(access, nullptr);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(var),
            ast_return_stmt_create(call),
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(class_def, fn, nullptr);

    decl_collector_run(fix->collector, AST_NODE(root));
    bool res = semantic_analyzer_run(fix->sema, AST_NODE(root));
    ASSERT_FALSE(res);
    ASSERT_LT(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, method_call_on_pointer_to_class)
{
    ast_def_t* class_def = ast_class_def_create_va("Node",
        ast_method_def_create_va("getValue", ast_type_builtin(TYPE_I32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(ast_int_lit_val(42)),
                nullptr),
            nullptr),
        nullptr);

    ast_decl_t* var = ast_var_decl_create("ptr",
        ast_type_pointer(ast_type_user_unresolved("Node")),
        nullptr);

    // Call method on pointer: ptr.getValue()
    ast_expr_t* access = ast_access_expr_create(
        ast_ref_expr_create("ptr"),
        ast_ref_expr_create("getValue"));

    ast_expr_t* call = ast_call_expr_create_va(access, nullptr);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(var),
            ast_return_stmt_create(call),
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(class_def, fn, nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, member_access_on_function_return_value_pointer)
{
    ast_def_t* class_def = ast_class_def_create_va("Point",
        ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
        nullptr);

    // Create function that returns *Point
    ast_def_t* get_point_fn = ast_fn_def_create_va("getPoint",
        ast_type_pointer(ast_type_user_unresolved("Point")),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(ast_null_lit_create()),
            nullptr),
        nullptr);

    // Create access expression: getPoint().x
    // Parser creates: access_expr(call_expr(ref("getPoint"), args), ref("x"))
    ast_expr_t* call = ast_call_expr_create_va(ast_ref_expr_create("getPoint"), nullptr);
    ast_expr_t* access = ast_access_expr_create(call, ast_ref_expr_create("x"));

    ast_stmt_t* return_stmt = ast_return_stmt_create(access);

    ast_def_t* test_fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(return_stmt, nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(class_def, get_point_fn, test_fn, nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    // Verify transformation: access_expr should be transformed to member_access
    ast_return_stmt_t* ret = (ast_return_stmt_t*)return_stmt;
    ASSERT_NEQ(nullptr, ret->value_expr);
    ASSERT_EQ(AST_EXPR_MEMBER_ACCESS, AST_KIND(ret->value_expr));

    // Verify member_access details
    ast_member_access_t* member_access = (ast_member_access_t*)ret->value_expr;
    ASSERT_EQ(0, strcmp("x", member_access->member_name));
    ASSERT_NEQ(nullptr, member_access->instance);
    ASSERT_EQ(AST_EXPR_CALL, AST_KIND(member_access->instance));
    ASSERT_EQ(ast_type_builtin(TYPE_I32), member_access->base.type);

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, namespace_chosen_over_function_in_ambiguous_access)
{
    // Create a class that will be the return type of a function
    ast_def_t* class_def = ast_class_def_create_va("Result",
        ast_member_decl_create("value", ast_type_builtin(TYPE_I32), nullptr),
        nullptr);

    // Create a function named "MyNamespace" that returns Result
    // If called as MyNamespace().value, this would be used
    ast_def_t* name_func = ast_fn_def_create_va("MyNamespace", ast_type_user_unresolved("Result"),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(
                ast_construct_expr_create_va(ast_type_user_unresolved("Result"),
                    ast_member_init_create("value", ast_int_lit_val(99)),
                    nullptr)),
            nullptr),
        nullptr);

    symbol_t* namespace_symb = semantic_context_register_namespace(fix->ctx, nullptr, "MyNamespace", nullptr);

    // Add a function to MyNamespace
    ast_def_t* namespaced_func = ast_fn_def_create_va("getValue", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(ast_int_lit_val(42)),
            nullptr),
        nullptr);
    symbol_t* namespaced_func_symb = symbol_create("getValue", SYMBOL_FUNCTION, namespaced_func,
        namespace_symb);
    namespaced_func_symb->data.function.return_type = ast_type_builtin(TYPE_I32);

    // Create access expression: MyNamespace.getValue()
    // This is ambiguous because:
    // - It could be a qualified name accessing the namespace function
    // - It could be calling MyNamespace() and accessing a member "getValue"
    // The namespace interpretation should be chosen
    ast_expr_t* access = ast_access_expr_create(
        ast_ref_expr_create("MyNamespace"),
        ast_ref_expr_create("getValue"));

    ast_expr_t* call = ast_call_expr_create_va(access, nullptr);
    ast_stmt_t* return_stmt = ast_return_stmt_create(call);

    ast_def_t* test_fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(return_stmt, nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(class_def, name_func, test_fn, nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    // Verify that the call resolved to the namespace function (returning i32)
    // not the function call returning Result
    ast_return_stmt_t* ret = (ast_return_stmt_t*)return_stmt;
    ASSERT_NEQ(nullptr, ret->value_expr);
    ASSERT_EQ(AST_EXPR_CALL, AST_KIND(ret->value_expr));

    // The return type should be i32 (from namespaced function), not Result
    ast_call_expr_t* call_expr = (ast_call_expr_t*)ret->value_expr;
    ASSERT_EQ(ast_type_builtin(TYPE_I32), call_expr->base.type);

    ast_node_destroy(root);
    ast_node_destroy(namespaced_func);
    symbol_destroy(namespaced_func_symb);
}

TEST(ut_sema_access_fixture_t, builtin_array_len_method)
{
    // Create: var arr: [i32, 5]; return arr.len();
    ast_type_t* arr_type = ast_type_array(ast_type_builtin(TYPE_I32), 5);
    ast_decl_t* var = ast_var_decl_create("arr", arr_type, nullptr);

    ast_expr_t* arr_ref = ast_ref_expr_create("arr");
    ast_expr_t* len_call = ast_method_call_create_va(arr_ref, "len", nullptr);
    ast_stmt_t* return_stmt = ast_return_stmt_create(len_call);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_USIZE),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(var),
            return_stmt,
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(fn, nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    // Verify the method call is marked as builtin and has correct return type
    ast_return_stmt_t* ret = (ast_return_stmt_t*)return_stmt;
    ASSERT_NEQ(nullptr, ret->value_expr);
    ASSERT_EQ(AST_EXPR_METHOD_CALL, AST_KIND(ret->value_expr));

    ast_method_call_t* call = (ast_method_call_t*)ret->value_expr;
    ASSERT_TRUE(call->is_builtin_method);
    ASSERT_EQ(ast_type_builtin(TYPE_USIZE), call->base.type);

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, builtin_view_len_method)
{
    // Create: var v: [i32]; return v.len();
    ast_type_t* view_type = ast_type_view(ast_type_builtin(TYPE_I32));
    ast_decl_t* var = ast_var_decl_create("v", view_type, nullptr);

    ast_expr_t* view_ref = ast_ref_expr_create("v");
    ast_expr_t* len_call = ast_method_call_create_va(view_ref, "len", nullptr);
    ast_stmt_t* return_stmt = ast_return_stmt_create(len_call);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_USIZE),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(var),
            return_stmt,
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(fn, nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    // Verify the method call is marked as builtin and has correct return type
    ast_return_stmt_t* ret = (ast_return_stmt_t*)return_stmt;
    ASSERT_NEQ(nullptr, ret->value_expr);
    ASSERT_EQ(AST_EXPR_METHOD_CALL, AST_KIND(ret->value_expr));

    ast_method_call_t* call = (ast_method_call_t*)ret->value_expr;
    ASSERT_TRUE(call->is_builtin_method);
    ASSERT_EQ(ast_type_builtin(TYPE_USIZE), call->base.type);

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, builtin_string_len_method)
{
    // Create: var s: string; return s.len();
    ast_decl_t* var = ast_var_decl_create("s", ast_type_builtin(TYPE_STRING), nullptr);

    ast_expr_t* str_ref = ast_ref_expr_create("s");
    ast_expr_t* len_call = ast_method_call_create_va(str_ref, "len", nullptr);
    ast_stmt_t* return_stmt = ast_return_stmt_create(len_call);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_builtin(TYPE_USIZE),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(var),
            return_stmt,
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(fn, nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    // Verify the method call is marked as builtin and has correct return type
    ast_return_stmt_t* ret = (ast_return_stmt_t*)return_stmt;
    ASSERT_NEQ(nullptr, ret->value_expr);
    ASSERT_EQ(AST_EXPR_METHOD_CALL, AST_KIND(ret->value_expr));

    ast_method_call_t* call = (ast_method_call_t*)ret->value_expr;
    ASSERT_TRUE(call->is_builtin_method);
    ASSERT_EQ(ast_type_builtin(TYPE_USIZE), call->base.type);

    ast_node_destroy(root);
}

TEST(ut_sema_access_fixture_t, builtin_string_raw_method)
{
    // Create: var s: string; return s.raw();
    ast_decl_t* var = ast_var_decl_create("s", ast_type_builtin(TYPE_STRING), nullptr);

    ast_expr_t* str_ref = ast_ref_expr_create("s");
    ast_expr_t* raw_call = ast_method_call_create_va(str_ref, "raw", nullptr);
    ast_stmt_t* return_stmt = ast_return_stmt_create(raw_call);

    ast_def_t* fn = ast_fn_def_create_va("test", ast_type_pointer(ast_type_builtin(TYPE_U8)),
        ast_compound_stmt_create_va(
            ast_decl_stmt_create(var),
            return_stmt,
            nullptr),
        nullptr);

    ast_root_t* root = ast_root_create_va(fn, nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    // Verify the method call is marked as builtin and has correct return type
    ast_return_stmt_t* ret = (ast_return_stmt_t*)return_stmt;
    ASSERT_NEQ(nullptr, ret->value_expr);
    ASSERT_EQ(AST_EXPR_METHOD_CALL, AST_KIND(ret->value_expr));

    ast_method_call_t* call = (ast_method_call_t*)ret->value_expr;
    ASSERT_TRUE(call->is_builtin_method);
    ASSERT_EQ(ast_type_pointer(ast_type_builtin(TYPE_U8)), call->base.type);

    ast_node_destroy(root);
}
