#include "ast/decl/param_decl.h"
#include "ast/def/fn_def.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/ref_expr.h"
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

TEST_FIXTURE(parser_functions_fixture_t)
{
    parser_t* parser;
};

TEST_SETUP(parser_functions_fixture_t)
{
    fix->parser = parser_create();
    ASSERT_NEQ(fix->parser, nullptr);
}

TEST_TEARDOWN(parser_functions_fixture_t)
{
    parser_destroy(fix->parser);
}

TEST(parser_functions_fixture_t, parse_basic_main_function)
{
    // Parse source code
    parser_set_source(fix->parser, "test", "fn main() -> i32 { return 0; }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_fn_def_create_va("main", ast_type_builtin(TYPE_I32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_int_lit_val(0)),
                nullptr), nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(parser_functions_fixture_t, parse_fn_parameters_and_calls_with_args)
{
    // Parse source code
    parser_set_source(fix->parser, "test", "fn foo2(arg1: i32, arg2: i32) -> i32 { return arg2; } "
        "fn foo(arg: i32) { foo2(arg, arg); foo2(arg, 5); }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_fn_def_create_va("foo2", ast_type_builtin(TYPE_I32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_ref_expr_create("arg2")),
                nullptr),
            ast_param_decl_create("arg1", ast_type_builtin(TYPE_I32)),
            ast_param_decl_create("arg2", ast_type_builtin(TYPE_I32)),
            nullptr),
        ast_fn_def_create_va("foo", nullptr,
            ast_compound_stmt_create_va(
                ast_expr_stmt_create(
                    ast_call_expr_create_va(ast_ref_expr_create("foo2"),
                        ast_ref_expr_create("arg"),
                        ast_ref_expr_create("arg"),
                        nullptr)),
                ast_expr_stmt_create(
                    ast_call_expr_create_va(ast_ref_expr_create("foo2"),
                        ast_ref_expr_create("arg"),
                        ast_int_lit_val(5),
                        nullptr)),
                nullptr),
            ast_param_decl_create("arg", ast_type_builtin(TYPE_I32)),
            nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(parser_functions_fixture_t, full_parse_with_simple_syntax_error)
{
    parser_set_source(fix->parser, "test", "fn foo() -> f32\n{ return 0 }\nfn foo2() -> i32\n{ return 10; }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);

    // Verify the error
    vec_t* errors = parser_errors(fix->parser);
    ASSERT_EQ(1, vec_size(errors));
    compiler_error_t* err = vec_get(errors, 0);
    ASSERT_NEQ(nullptr, err);
    ASSERT_EQ("test", err->source_file);
    ASSERT_EQ(2, err->line);
    ASSERT_EQ(11, err->column);
    ASSERT_EQ("expected ';'", err->description);
    ASSERT_EQ(nullptr, err->offender);

    // Construct the expected tree
    ast_root_t* expected = ast_root_create_va(
        ast_fn_def_create_va("foo", ast_type_builtin(TYPE_F32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_int_lit_val(0)),
                nullptr),
            nullptr),
        ast_fn_def_create_va("foo2", ast_type_builtin(TYPE_I32),
            ast_compound_stmt_create_va(
                ast_return_stmt_create(
                    ast_int_lit_val(10)),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}

TEST(parser_functions_fixture_t, parse_complex_type_annotation_in_parameter)
{
    // Parse source code with complex param type: view[i32*]* (pointer to a view of i32 pointers)
    parser_set_source(fix->parser, "test", "fn foo(range_ptr: view[i32*]*) { }");
    ast_root_t* root = parser_parse(fix->parser);
    ASSERT_NEQ(nullptr, root);
    ASSERT_EQ(0, vec_size(parser_errors(fix->parser)));

    ast_type_t* view_ptr_type = ast_type_pointer(
        ast_type_view(ast_type_pointer(ast_type_builtin(TYPE_I32))));

    ast_root_t* expected = ast_root_create_va(
        ast_fn_def_create_va("foo", nullptr, ast_compound_stmt_create_empty(),
            ast_param_decl_create("range_ptr", view_ptr_type), nullptr),
        nullptr);

    ASSERT_TREES_EQUAL(expected, root);
    ast_node_destroy(root);
    ast_node_destroy(expected);
}
