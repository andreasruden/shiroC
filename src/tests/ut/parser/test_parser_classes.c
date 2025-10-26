#include "ast/decl/param_decl.h"
#include "ast/decl/member_decl.h"
#include "ast/def/class_def.h"
#include "ast/def/method_def.h"
#include "ast/expr/access_expr.h"
#include "ast/expr/array_subscript.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/construct_expr.h"
#include "ast/expr/member_init.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/member_access.h"
#include "ast/expr/method_call.h"
#include "ast/expr/ref_expr.h"
#include "ast/expr/self_expr.h"
#include "ast/expr/unary_op.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "ast/type.h"
#include "compiler_error.h"
#include "parser/parser.h"
#include "test_runner.h"
#include "parser_shared.h"

TEST_FIXTURE(parser_classes_fixture_t)
{
    parser_t* parser;
};

TEST_SETUP(parser_classes_fixture_t)
{
    fix->parser = parser_create();
    ASSERT_NEQ(fix->parser, nullptr);
}

TEST_TEARDOWN(parser_classes_fixture_t)
{
    parser_destroy(fix->parser);
    ast_type_cache_reset();
}

TEST(parser_classes_fixture_t, parse_empty_class)
{
    // Parse source code
    parser_set_source(fix->parser, "test", "class Empty { }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_class_def_create_va("Empty", nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_class_with_var_members)
{
    // Parse source code with variables including one with default value
    parser_set_source(fix->parser, "test",
        "class Point {\n"
        "    var x: i32;\n"
        "    var y: i32 = 10;\n"
        "}");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            ast_member_decl_create("y", ast_type_builtin(TYPE_I32), ast_int_lit_val(10)),
            nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_class_with_fn_member)
{
    // Parse source code
    parser_set_source(fix->parser, "test",
        "class Calculator {\n"
        "    fn add(a: i32, b: i32) -> i32 { return a + b; }\n"
        "}");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
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
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_class_mixed_members)
{
    // Parse source code with mixed variables and functions
    parser_set_source(fix->parser, "test",
        "class Counter {\n"
        "    var count: i32 = 0;\n"
        "    fn increment() { count = count + 1; }\n"
        "    var max: i32;\n"
        "}");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_class_def_create_va("Counter",
            ast_member_decl_create("count", ast_type_builtin(TYPE_I32), ast_int_lit_val(0)),
            ast_method_def_create_va("increment", nullptr,
                ast_compound_stmt_create_va(
                    ast_expr_stmt_create(
                        ast_bin_op_create(TOKEN_ASSIGN,
                            ast_ref_expr_create("count"),
                            ast_bin_op_create(TOKEN_PLUS,
                                ast_ref_expr_create("count"),
                                ast_int_lit_val(1)))),
                    nullptr),
                nullptr),
            ast_member_decl_create("max", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_multiple_classes)
{
    // Parse source code with multiple class definitions
    parser_set_source(fix->parser, "test",
        "class Foo { var x: i32; }\n"
        "class Bar { fn test() { } }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_class_def_create_va("Foo",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        ast_class_def_create_va("Bar",
            ast_method_def_create_va("test", nullptr,
                ast_compound_stmt_create_empty(),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_class_with_syntax_error_recovery)
{
    // Parse source code with syntax error in first class, valid second class
    parser_set_source(fix->parser, "test",
        "class Bad { var x: i32 }\n"
        "class Good { var y: i32; }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);

    // Verify the error
    vec_t* errors = parser_errors(fix->parser);
    ASSERT_EQ(1, vec_size(errors));
    compiler_error_t* err = vec_get(errors, 0);
    ASSERT_NEQ(nullptr, err);
    ASSERT_EQ("test", err->source_file);

    // Construct the expected tree - both classes should be parsed
    ast_root_t* expected = ast_root_create_va(
        ast_class_def_create_va("Bad",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        ast_class_def_create_va("Good",
            ast_member_decl_create("y", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_simple_class_construction)
{
    // Parse simple class construction with two fields
    parser_set_source(fix->parser, "test", "Point { x = 10, y = 20 }");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_expr_t* expected = ast_construct_expr_create_va(ast_type_user_unresolved("Point"),
        ast_member_init_create("x", ast_int_lit_val(10)),
        ast_member_init_create("y", ast_int_lit_val(20)),
        nullptr);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_empty_class_construction)
{
    // Parse empty class construction with no fields
    parser_set_source(fix->parser, "test", "Point {}");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_expr_t* expected = ast_construct_expr_create_va(ast_type_user_unresolved("Point"), nullptr);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_construction_with_binary_ops)
{
    // Parse class construction with binary operator expressions as field values
    parser_set_source(fix->parser, "test", "Point { x = 1 + 2, y = 3 * 4 }");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_expr_t* expected = ast_construct_expr_create_va(ast_type_user_unresolved("Point"),
        ast_member_init_create("x",
            ast_bin_op_create(TOKEN_PLUS, ast_int_lit_val(1), ast_int_lit_val(2))),
        ast_member_init_create("y",
            ast_bin_op_create(TOKEN_STAR, ast_int_lit_val(3), ast_int_lit_val(4))),
        nullptr);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_construction_with_function_calls)
{
    // Parse class construction with function call expressions as field values
    parser_set_source(fix->parser, "test", "Point { x = getX(), y = getY() }");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_expr_t* expected = ast_construct_expr_create_va(ast_type_user_unresolved("Point"),
        ast_member_init_create("x",
            ast_call_expr_create_va(ast_ref_expr_create("getX"), nullptr)),
        ast_member_init_create("y",
            ast_call_expr_create_va(ast_ref_expr_create("getY"), nullptr)),
        nullptr);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_nested_class_construction)
{
    // Parse nested class constructions as field values
    parser_set_source(fix->parser, "test",
        "Line { start = Point { x = 0, y = 0 }, end = Point { x = 10, y = 10 } }");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_expr_t* expected = ast_construct_expr_create_va(ast_type_user_unresolved("Line"),
        ast_member_init_create("start",
            ast_construct_expr_create_va(ast_type_user_unresolved("Point"),
                ast_member_init_create("x", ast_int_lit_val(0)),
                ast_member_init_create("y", ast_int_lit_val(0)),
                nullptr)),
        ast_member_init_create("end",
            ast_construct_expr_create_va(ast_type_user_unresolved("Point"),
                ast_member_init_create("x", ast_int_lit_val(10)),
                ast_member_init_create("y", ast_int_lit_val(10)),
                nullptr)),
        nullptr);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_construction_missing_closing_brace)
{
    // Parse class construction with missing closing brace - test error recovery
    parser_set_source(fix->parser, "test", "Point { x = 10");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);

    // Verify that an error was recorded
    vec_t* errors = parser_errors(fix->parser);
    ASSERT_TRUE(vec_size(errors) > 0);

    // Construct the expected tree - should recover and include the field we parsed
    ast_expr_t* expected = ast_construct_expr_create_va(ast_type_user_unresolved("Point"),
        ast_member_init_create("x", ast_int_lit_val(10)),
        nullptr);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_construction_invalid_field_syntax)
{
    // Parse class construction with invalid field syntax (missing =) - test error recovery
    parser_set_source(fix->parser, "test", "Point { x 10 }");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);

    // Verify that an error was recorded
    vec_t* errors = parser_errors(fix->parser);
    ASSERT_TRUE(vec_size(errors) > 0);

    // Construct the expected tree - parser recovers and treats 'x' as field name, 10 as value
    ast_expr_t* expected = ast_construct_expr_create_va(ast_type_user_unresolved("Point"),
        ast_member_init_create("x", ast_int_lit_val(10)),
        nullptr);

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_simple_member_access)
{
    // Parse basic field access
    parser_set_source(fix->parser, "test", "p.x");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_expr_t* expected = ast_access_expr_create(ast_ref_expr_create("p"), ast_ref_expr_create("x"));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_member_access_assignment)
{
    // Parse field assignment
    parser_set_source(fix->parser, "test", "p.x = 10;");
    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_stmt_t* expected = ast_expr_stmt_create(
        ast_bin_op_create(TOKEN_ASSIGN,
            ast_access_expr_create(ast_ref_expr_create("p"), ast_ref_expr_create("x")),
            ast_int_lit_val(10)));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(stmt);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_member_method_call_with_args)
{
    // Parse method call with arguments
    parser_set_source(fix->parser, "test", "p.distanceTo(&other_p);");
    ast_stmt_t* stmt = parser_parse_stmt(fix->parser);
    ASSERT_NEQ(nullptr, stmt);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_stmt_t* expected = ast_expr_stmt_create(
        ast_call_expr_create_va(ast_access_expr_create(ast_ref_expr_create("p"), ast_ref_expr_create("distanceTo")),
        ast_unary_op_create(TOKEN_AMPERSAND, ast_ref_expr_create("other_p")), nullptr));

    ASSERT_TREES_EQUAL(expected, stmt);
    ast_node_destroy(stmt);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_chained_member_access)
{
    // Parse chained member access (tests left-associativity)
    parser_set_source(fix->parser, "test", "line.start.x");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree: (line.start).x
    ast_expr_t* expected = ast_access_expr_create(
        ast_access_expr_create(ast_ref_expr_create("line"), ast_ref_expr_create("start")),
            ast_ref_expr_create("x"));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_member_access_in_expression)
{
    // Parse member access in binary expression (tests precedence)
    parser_set_source(fix->parser, "test", "p.x + p.y");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_expr_t* expected = ast_bin_op_create(TOKEN_PLUS,
        ast_access_expr_create(ast_ref_expr_create("p"), ast_ref_expr_create("x")),
        ast_access_expr_create(ast_ref_expr_create("p"), ast_ref_expr_create("y")));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_member_access_on_call_result)
{
    // Parse member access on function call result
    parser_set_source(fix->parser, "test", "getPoint().x");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_expr_t* expected = ast_access_expr_create(
        ast_call_expr_create_va(ast_ref_expr_create("getPoint"), nullptr),
            ast_ref_expr_create("x"));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_member_access_on_subscript)
{
    // Parse member access on array subscript result
    parser_set_source(fix->parser, "test", "points[0].x");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_expr_t* expected = ast_access_expr_create(
        ast_array_subscript_create(ast_ref_expr_create("points"), ast_int_lit_val(0)),
            ast_ref_expr_create("x"));

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

TEST(parser_classes_fixture_t, parse_self_member_access)
{
    // Parse self.member access
    parser_set_source(fix->parser, "test", "self.x");
    ast_expr_t* expr = parser_parse_expr(fix->parser);
    ASSERT_NEQ(nullptr, expr);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree: member_access(self_expr, "x")
    ast_expr_t* expected = ast_member_access_create(ast_self_expr_create(false), "x");

    ASSERT_TREES_EQUAL(expected, expr);
    ast_node_destroy(expr);
    ast_node_destroy(expected);
}

// Parse source code with exported class containing a method
TEST(parser_classes_fixture_t, parse_exported_class_with_method)
{
    parser_set_source(fix->parser, "test",
        "export class Calculator {\n"
        "    fn multiply(a: i32, b: i32) -> i32 { return a * b; }\n"
        "}");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    vec_t members = VEC_INIT(ast_node_destroy);
    vec_t methods = VEC_INIT(ast_node_destroy);
    vec_push(&methods, ast_method_def_create_va("multiply", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(
                ast_bin_op_create(TOKEN_STAR,
                    ast_ref_expr_create("a"),
                    ast_ref_expr_create("b"))),
            nullptr),
        ast_param_decl_create("a", ast_type_builtin(TYPE_I32)),
        ast_param_decl_create("b", ast_type_builtin(TYPE_I32)),
        nullptr));

    ast_root_t* expected = ast_root_create_va(
        ast_class_def_create("Calculator", &members, &methods, true),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}
