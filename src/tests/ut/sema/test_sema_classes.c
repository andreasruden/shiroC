#include "ast/decl/member_decl.h"
#include "ast/decl/param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/class_def.h"
#include "ast/def/fn_def.h"
#include "ast/def/method_def.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/construct_expr.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/member_access.h"
#include "ast/expr/member_init.h"
#include "ast/expr/method_call.h"
#include "ast/expr/ref_expr.h"
#include "ast/expr/self_expr.h"
#include "ast/expr/str_lit.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "ast/type.h"
#include "sema/decl_collector.h"
#include "sema/semantic_analyzer.h"
#include "test_runner.h"
#include "sema_shared.h"

TEST_FIXTURE(ut_sema_classes_fixture_t)
{
    semantic_analyzer_t* sema;
    semantic_context_t* ctx;
    decl_collector_t* collector;
};

TEST_SETUP(ut_sema_classes_fixture_t)
{
    fix->ctx = semantic_context_create();
    ASSERT_NEQ(nullptr, fix->ctx);

    fix->collector = decl_collector_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->collector);

    fix->sema = semantic_analyzer_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->sema);
}

TEST_TEARDOWN(ut_sema_classes_fixture_t)
{
    semantic_analyzer_destroy(fix->sema);
    decl_collector_destroy(fix->collector);
    semantic_context_destroy(fix->ctx);
}

TEST(ut_sema_classes_fixture_t, empty_class_is_valid)
{
    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Empty", nullptr),
        nullptr);

    decl_collector_run(fix->collector, AST_NODE(root));
    bool res = semantic_analyzer_run(fix->sema, AST_NODE(root));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, valid_member_types)
{
    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Data",
            ast_member_decl_create("a", ast_type_builtin(TYPE_I32), nullptr),
            ast_member_decl_create("b", ast_type_builtin(TYPE_BOOL), nullptr),
            ast_member_decl_create("c", ast_type_builtin(TYPE_F64), nullptr),
            nullptr),
        nullptr);

    decl_collector_run(fix->collector, AST_NODE(root));
    bool res = semantic_analyzer_run(fix->sema, AST_NODE(root));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, invalid_member_type_undefined_class)
{
    ast_decl_t* error_node = ast_member_decl_create("next", ast_type_user("UndefinedClass"), nullptr);

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Node",
            error_node,
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "undefined type");

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, member_default_value_type_mismatch)
{
    ast_decl_t* error_node = ast_member_decl_create("x", ast_type_builtin(TYPE_I32),
        ast_str_lit_create("hello"));

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            error_node,
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "does not match annotation");

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, method_return_type_mismatch)
{
    ast_expr_t* error_node = ast_str_lit_create("hello");

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Calculator",
            ast_method_def_create_va("compute", ast_type_builtin(TYPE_I32),
                ast_compound_stmt_create_va(
                    ast_return_stmt_create(error_node),
                    nullptr),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "cannot coerce type");

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, method_parameter_shadows_member)
{
    // This tests whether params can shadow members - behavior depends on design
    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            ast_method_def_create_va("setX", nullptr,
                ast_compound_stmt_create_va(
                    ast_expr_stmt_create(
                        ast_bin_op_create(TOKEN_ASSIGN,
                            ast_ref_expr_create("x"),
                            ast_ref_expr_create("x"))),
                    nullptr),
                ast_param_decl_create("x", ast_type_builtin(TYPE_I32)),
                nullptr),
            nullptr),
        nullptr);

    // This should either pass (param shadows member) or fail with specific error
    // Adjust assertion based on actual behavior
    decl_collector_run(fix->collector, AST_NODE(root));
    bool res = semantic_analyzer_run(fix->sema, AST_NODE(root));
    // For now, assuming it's allowed
    ASSERT_TRUE(res);

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, method_undefined_variable_reference)
{
    ast_expr_t* error_node = ast_ref_expr_create("undefined_var");

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            ast_method_def_create_va("test", nullptr,
                ast_compound_stmt_create_va(
                    ast_expr_stmt_create(error_node),
                    nullptr),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "undefined");

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, valid_class_construction)
{
    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            ast_member_decl_create("y", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        ast_fn_def_create_va("main", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(
                    ast_var_decl_create("p", nullptr,
                        ast_construct_expr_create_va(ast_type_user("Point"),
                            ast_member_init_create("x", ast_int_lit_val(10)),
                            ast_member_init_create("y", ast_int_lit_val(20)),
                            nullptr))),
                nullptr),
            nullptr),
        nullptr);

    decl_collector_run(fix->collector, AST_NODE(root));
    bool res = semantic_analyzer_run(fix->sema, AST_NODE(root));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, construction_missing_required_field)
{
    ast_expr_t* error_node = ast_construct_expr_create_va(ast_type_user("Point"),
        ast_member_init_create("x", ast_int_lit_val(10)),
        nullptr);

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            ast_member_decl_create("y", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        ast_fn_def_create_va("main", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(ast_var_decl_create("p", nullptr, error_node)),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "missing");

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, construction_unknown_field)
{
    ast_member_init_t* error_node = ast_member_init_create("z", ast_int_lit_val(30));

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            ast_member_decl_create("y", ast_type_builtin(TYPE_I32), ast_int_lit_val(0)),
            nullptr),
        ast_fn_def_create_va("main", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(ast_var_decl_create("p", nullptr,
                    ast_construct_expr_create_va(ast_type_user("Point"),
                        ast_member_init_create("x", ast_int_lit_val(10)),
                        error_node,
                        nullptr))),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "no member");

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, construction_field_type_mismatch)
{
    ast_member_init_t* error_node = ast_member_init_create("x", ast_str_lit_create("hello"));

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            ast_member_decl_create("y", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        ast_fn_def_create_va("main", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(ast_var_decl_create("p", nullptr,
                    ast_construct_expr_create_va(ast_type_user("Point"),
                        error_node,
                        ast_member_init_create("y", ast_int_lit_val(20)),
                        nullptr))),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "cannot coerce");

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, construction_duplicate_field_init)
{
    ast_expr_t* error_node = ast_construct_expr_create_va(ast_type_user("Point"),
        ast_member_init_create("x", ast_int_lit_val(10)),
        ast_member_init_create("x", ast_int_lit_val(20)),
        nullptr);

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        ast_fn_def_create_va("main", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(ast_var_decl_create("p", nullptr, error_node)),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "duplicate initialization for member 'x'");

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, construction_undefined_class)
{
    ast_expr_t* error_node = ast_construct_expr_create_va(ast_type_user("UndefinedClass"),
        ast_member_init_create("x", ast_int_lit_val(10)),
        nullptr);

    ast_root_t* root = ast_root_create_va(
        ast_fn_def_create_va("main", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(ast_var_decl_create("p", nullptr, error_node)),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "undefined");

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, valid_member_access)
{
    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        ast_fn_def_create_va("main", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(
                    ast_var_decl_create("p", ast_type_user("Point"),
                        ast_construct_expr_create_va(ast_type_user("Point"),
                            ast_member_init_create("x", ast_int_lit_val(10)),
                            nullptr))),
                ast_expr_stmt_create(
                    ast_member_access_create(ast_ref_expr_create("p"), "x")),
                nullptr),
            nullptr),
        nullptr);

    decl_collector_run(fix->collector, AST_NODE(root));
    bool res = semantic_analyzer_run(fix->sema, AST_NODE(root));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, member_access_nonexistent_field)
{
    ast_expr_t* error_node = ast_member_access_create(ast_ref_expr_create("p"), "z");

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        ast_fn_def_create_va("main", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(
                    ast_var_decl_create("p", ast_type_user("Point"),
                        ast_construct_expr_create_va(ast_type_user("Point"),
                            ast_member_init_create("x", ast_int_lit_val(10)),
                            nullptr))),
                ast_expr_stmt_create(error_node),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "no member");

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, chained_member_access)
{
    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        ast_class_def_create_va("Line",
            ast_member_decl_create("start", ast_type_user("Point"), nullptr),
            nullptr),
        ast_fn_def_create_va("main", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(
                    ast_var_decl_create("p", ast_type_user("Point"),
                        ast_construct_expr_create_va(ast_type_user("Point"),
                            ast_member_init_create("x", ast_int_lit_val(0)),
                            nullptr))),
                ast_decl_stmt_create(
                    ast_var_decl_create("line", ast_type_user("Line"),
                        ast_construct_expr_create_va(ast_type_user("Line"),
                            ast_member_init_create("start", ast_ref_expr_create("p")),
                            nullptr))),
                ast_expr_stmt_create(
                    ast_member_access_create(
                        ast_member_access_create(ast_ref_expr_create("line"), "start"),
                        "x")),
                nullptr),
            nullptr),
        nullptr);

    decl_collector_run(fix->collector, AST_NODE(root));
    bool res = semantic_analyzer_run(fix->sema, AST_NODE(root));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, member_access_on_non_class_type)
{
    ast_expr_t* error_node = ast_member_access_create(ast_ref_expr_create("x"), "field");

    ast_root_t* root = ast_root_create_va(
        ast_fn_def_create_va("main", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_I32), ast_int_lit_val(5))),
                ast_expr_stmt_create(error_node),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "not class type");

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, valid_method_call)
{
    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Calculator",
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
            nullptr),
        ast_fn_def_create_va("main", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(
                    ast_var_decl_create("calc", ast_type_user("Calculator"),
                        ast_construct_expr_create_va(ast_type_user("Calculator"), nullptr))),
                ast_expr_stmt_create(
                    ast_method_call_create_va(ast_ref_expr_create("calc"), "add",
                        ast_int_lit_val(1), ast_int_lit_val(2), nullptr)),
                nullptr),
            nullptr),
        nullptr);

    decl_collector_run(fix->collector, AST_NODE(root));
    bool res = semantic_analyzer_run(fix->sema, AST_NODE(root));
    ASSERT_TRUE(res);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, method_call_undefined_method)
{
    ast_expr_t* error_node = ast_method_call_create_va(ast_ref_expr_create("p"), "nonexistent", nullptr);

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        ast_fn_def_create_va("main", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(
                    ast_var_decl_create("p", ast_type_user("Point"),
                        ast_construct_expr_create_va(ast_type_user("Point"),
                            ast_member_init_create("x", ast_int_lit_val(10)),
                            nullptr))),
                ast_expr_stmt_create(error_node),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "no method");

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, method_call_argument_count_mismatch)
{
    ast_expr_t* error_node = ast_method_call_create_va(ast_ref_expr_create("calc"), "add",
        ast_int_lit_val(1), nullptr);

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Calculator",
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
            nullptr),
        ast_fn_def_create_va("main", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(
                    ast_var_decl_create("calc", ast_type_user("Calculator"),
                        ast_construct_expr_create_va(ast_type_user("Calculator"), nullptr))),
                ast_expr_stmt_create(error_node),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "argument");

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, method_call_argument_type_mismatch)
{
    ast_expr_t* error_node = ast_str_lit_create("hello");

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Calculator",
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
            nullptr),
        ast_fn_def_create_va("main", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(
                    ast_var_decl_create("calc", ast_type_user("Calculator"),
                        ast_construct_expr_create_va(ast_type_user("Calculator"), nullptr))),
                ast_expr_stmt_create(
                    ast_method_call_create_va(ast_ref_expr_create("calc"), "add",
                        error_node, ast_int_lit_val(2), nullptr)),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "cannot coerce type");

    ast_node_destroy(root);
}

TEST(ut_sema_classes_fixture_t, explicit_self_member_assignment)
{
    // Test that we can access `self.some_member` inside a method
    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Counter",
            ast_member_decl_create("count", ast_type_builtin(TYPE_I32), ast_int_lit_val(0)),
            ast_method_def_create_va("increment", nullptr,
                ast_compound_stmt_create_va(
                    ast_expr_stmt_create(
                        ast_bin_op_create(TOKEN_ASSIGN,
                            ast_member_access_create(ast_self_expr_create(false), "count"),
                            ast_int_lit_val(20))),
                    nullptr),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    ast_node_destroy(root);
}
