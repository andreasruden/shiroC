#include "ast/decl/member_decl.h"
#include "ast/decl/param_decl.h"
#include "ast/def/class_def.h"
#include "ast/def/fn_def.h"
#include "ast/def/method_def.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/construct_expr.h"
#include "ast/expr/float_lit.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/member_access.h"
#include "ast/expr/member_init.h"
#include "ast/expr/ref_expr.h"
#include "ast/expr/self_expr.h"
#include "ast/expr/unary_op.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "ast/type.h"
#include "parser/parser.h"
#include "test_runner.h"
#include "parser_shared.h"

TEST_FIXTURE(parser_templates_fixture_t)
{
    parser_t* parser;
};

TEST_SETUP(parser_templates_fixture_t)
{
    fix->parser = parser_create();
    ASSERT_NEQ(fix->parser, nullptr);
}

TEST_TEARDOWN(parser_templates_fixture_t)
{
    parser_destroy(fix->parser);
    ast_type_cache_reset();
}

// Test parsing a simple templated class with one type parameter
TEST(parser_templates_fixture_t, parse_class_single_type_param)
{
    parser_set_source(fix->parser, "test",
        "class UniquePtr<T> {\n"
        "    var value: T;\n"
        "}");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_class_def_create_templated_va("UniquePtr",
            "T", nullptr,  // type parameters
            ast_member_decl_create("value", ast_type_variable("T"), nullptr),
            nullptr),  // members/methods
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

// Test parsing a templated class with multiple type parameters
TEST(parser_templates_fixture_t, parse_class_multiple_type_params)
{
    parser_set_source(fix->parser, "test",
        "class Pair<T, U> {\n"
        "    var first: T;\n"
        "    var second: U;\n"
        "}");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_class_def_create_templated_va("Pair",
            "T", "U", nullptr,  // type parameters
            ast_member_decl_create("first", ast_type_variable("T"), nullptr),
            ast_member_decl_create("second", ast_type_variable("U"), nullptr),
            nullptr),  // members/methods
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

// Test parsing a simple templated function with one type parameter
TEST(parser_templates_fixture_t, parse_function_single_type_param)
{
    parser_set_source(fix->parser, "test",
        "fn identity<T>(value: T) -> T {\n"
        "    return value;\n"
        "}");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_fn_def_create_templated_va("identity", ast_type_variable("T"),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(ast_ref_expr_create("value")),
                nullptr),
            "T", nullptr,  // type parameters
            ast_param_decl_create("value", ast_type_variable("T")),
            nullptr),  // parameters
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

// Test parsing a templated function with multiple type parameters
TEST(parser_templates_fixture_t, parse_function_multiple_type_params)
{
    parser_set_source(fix->parser, "test",
        "fn swap<T, U>(a: T*, b: U*) {\n"
        "    var temp = *a;\n"
        "}");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_fn_def_create_templated_va("swap", nullptr,
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(ast_var_decl_create("temp", nullptr,
                    ast_unary_op_create(TOKEN_STAR, ast_ref_expr_create("a")))),
                nullptr),
            "T", "U", nullptr,  // type parameters
            ast_param_decl_create("a", ast_type_pointer(ast_type_variable("T"))),
            ast_param_decl_create("b", ast_type_pointer(ast_type_variable("U"))),
            nullptr),  // parameters
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

// Test parsing templated class with methods
TEST(parser_templates_fixture_t, parse_templated_class_with_methods)
{
    parser_set_source(fix->parser, "test",
        "class Container<T> {\n"
        "    var data: T;\n"
        "    fn get() -> T { return self.data; }\n"
        "    fn set(value: T) { self.data = value; }\n"
        "}");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_class_def_create_templated_va("Container",
            "T", nullptr,  // type parameters
            ast_member_decl_create("data", ast_type_variable("T"), nullptr),
            ast_method_def_create_va("get", ast_type_variable("T"),
                ast_compound_stmt_create_va(
                    ast_return_stmt_create(
                        ast_member_access_create(ast_self_expr_create(false), "data")),
                    nullptr),
                nullptr),
            ast_method_def_create_va("set", nullptr,
                ast_compound_stmt_create_va(
                    ast_expr_stmt_create(
                        ast_bin_op_create(TOKEN_ASSIGN,
                            ast_member_access_create(ast_self_expr_create(false), "data"),
                            ast_ref_expr_create("value"))),
                    nullptr),
                ast_param_decl_create("value", ast_type_variable("T")),
                nullptr),
            nullptr),  // members/methods
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

// Test parsing multiple templated definitions
TEST(parser_templates_fixture_t, parse_multiple_template_definitions)
{
    parser_set_source(fix->parser, "test",
        "class UniquePtr<T> { var value: T; }\n"
        "fn identity<U>(x: U) -> U { return x; }\n"
        "class Pair<A, B> { var first: A; var second: B; }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_class_def_create_templated_va("UniquePtr",
            "T", nullptr,
            ast_member_decl_create("value", ast_type_variable("T"), nullptr),
            nullptr),
        ast_fn_def_create_templated_va("identity", ast_type_variable("U"),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(ast_ref_expr_create("x")),
                nullptr),
            "U", nullptr,
            ast_param_decl_create("x", ast_type_variable("U")),
            nullptr),
        ast_class_def_create_templated_va("Pair",
            "A", "B", nullptr,
            ast_member_decl_create("first", ast_type_variable("A"), nullptr),
            ast_member_decl_create("second", ast_type_variable("B"), nullptr),
            nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

// Test error recovery: missing closing angle bracket
TEST(parser_templates_fixture_t, parse_template_missing_closing_angle_bracket)
{
    parser_set_source(fix->parser, "test", "class UniquePtr<T { }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);

    // Should have recorded an error
    vec_t* errors = parser_errors(fix->parser);
    ASSERT_TRUE(vec_size(errors) > 0);

    ast_node_destroy(root);
}

// Test error recovery: empty type parameter list
TEST(parser_templates_fixture_t, parse_template_empty_type_params)
{
    parser_set_source(fix->parser, "test", "class UniquePtr<> { }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);

    // Should have recorded an error
    vec_t* errors = parser_errors(fix->parser);
    ASSERT_TRUE(vec_size(errors) > 0);

    ast_node_destroy(root);
}

// Test parsing construct expression with single type argument
TEST(parser_templates_fixture_t, parse_construct_expr_single_type_arg)
{
    parser_set_source(fix->parser, "test", "UniquePtr<i32> { value = 42 }");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct expected tree
    vec_t* type_args = vec_create(nullptr);
    vec_push(type_args, ast_type_builtin(TYPE_I32));
    ast_expr_t* expected = ast_construct_expr_create_va(
        ast_type_user_unresolved_with_args("UniquePtr", type_args),
        ast_member_init_create("value", ast_int_lit_val(42)),
        nullptr);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

// Test parsing construct expression with multiple type arguments
TEST(parser_templates_fixture_t, parse_construct_expr_multiple_type_args)
{
    parser_set_source(fix->parser, "test", "Pair<i32, f64> { first = 10, second = 3.14 }");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct expected tree
    vec_t* type_args = vec_create(nullptr);
    vec_push(type_args, ast_type_builtin(TYPE_I32));
    vec_push(type_args, ast_type_builtin(TYPE_F64));
    ast_expr_t* expected = ast_construct_expr_create_va(
        ast_type_user_unresolved_with_args("Pair", type_args),
        ast_member_init_create("first", ast_int_lit_val(10)),
        ast_member_init_create("second", ast_float_lit_create(3.14, "")),
        nullptr);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

// Test parsing construct expression with nested template types
TEST(parser_templates_fixture_t, parse_construct_expr_nested_template_types)
{
    parser_set_source(fix->parser, "test", "UniquePtr<Pair<i32, i64>> { }");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct expected tree - nested type arguments
    vec_t* inner_type_args = vec_create(nullptr);
    vec_push(inner_type_args, ast_type_builtin(TYPE_I32));
    vec_push(inner_type_args, ast_type_builtin(TYPE_I64));
    vec_t* outer_type_args = vec_create(nullptr);
    vec_push(outer_type_args, ast_type_user_unresolved_with_args("Pair", inner_type_args));
    ast_expr_t* expected = ast_construct_expr_create_va(
        ast_type_user_unresolved_with_args("UniquePtr", outer_type_args),
        nullptr);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

// Test parsing construct expression with type variable as type argument
TEST(parser_templates_fixture_t, parse_construct_expr_type_variable_arg)
{
    parser_set_source(fix->parser, "test", "UniquePtr<T> { value = x }");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct expected tree - T is a type variable
    vec_t* type_args = vec_create(nullptr);
    vec_push(type_args, ast_type_variable("T"));
    ast_expr_t* expected = ast_construct_expr_create_va(
        ast_type_user_unresolved_with_args("UniquePtr", type_args),
        ast_member_init_create("value", ast_ref_expr_create("x")),
        nullptr);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

// Test parsing construct expression with pointer to template type
TEST(parser_templates_fixture_t, parse_construct_expr_pointer_to_template)
{
    parser_set_source(fix->parser, "test", "Container<i32*> { data = ptr }");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct expected tree
    vec_t* type_args = vec_create(nullptr);
    vec_push(type_args, ast_type_pointer(ast_type_builtin(TYPE_I32)));
    ast_expr_t* expected = ast_construct_expr_create_va(
        ast_type_user_unresolved_with_args("Container", type_args),
        ast_member_init_create("data", ast_ref_expr_create("ptr")),
        nullptr);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

// Test parsing construct expression with array type as type argument
TEST(parser_templates_fixture_t, parse_construct_expr_array_type_arg)
{
    parser_set_source(fix->parser, "test", "Container<[i32, 10]> { }");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct expected tree
    vec_t* type_args = vec_create(nullptr);
    vec_push(type_args, ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(10)));
    ast_expr_t* expected = ast_construct_expr_create_va(
        ast_type_user_unresolved_with_args("Container", type_args),
        nullptr);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}
