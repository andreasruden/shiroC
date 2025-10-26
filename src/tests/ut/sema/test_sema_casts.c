#include "ast/decl/member_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/class_def.h"
#include "ast/def/fn_def.h"
#include "ast/expr/bool_lit.h"
#include "ast/expr/cast_expr.h"
#include "ast/expr/construct_expr.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/member_init.h"
#include "ast/expr/ref_expr.h"
#include "ast/expr/unary_op.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/type.h"
#include "sema/decl_collector.h"
#include "sema/semantic_analyzer.h"
#include "test_runner.h"
#include "sema_shared.h"

TEST_FIXTURE(ut_sema_cast_fixture_t)
{
    semantic_analyzer_t* sema;
    semantic_context_t* ctx;
    decl_collector_t* collector;
};

TEST_SETUP(ut_sema_cast_fixture_t)
{
    fix->ctx = semantic_context_create("test", "sema_cast");
    ASSERT_NEQ(nullptr, fix->ctx);

    fix->collector = decl_collector_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->collector);

    fix->sema = semantic_analyzer_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->sema);
}

TEST_TEARDOWN(ut_sema_cast_fixture_t)
{
    semantic_analyzer_destroy(fix->sema);
    decl_collector_destroy(fix->collector);
    semantic_context_destroy(fix->ctx);
    ast_type_cache_reset();
}

// Basic numeric conversion (int to int)
TEST(ut_sema_cast_fixture_t, accept_numeric_int_to_int_cast)
{
    // var x: i64 = 42 as i64;
    ast_expr_t* cast_expr = ast_cast_expr_create(ast_int_lit_val(42), ast_type_builtin(TYPE_I64));
    ast_stmt_t* stmt = ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_I64), cast_expr));

    ASSERT_SEMA_SUCCESS(AST_NODE(stmt));

    ast_node_destroy(stmt);
}

// Pointer to pointer cast
TEST(ut_sema_cast_fixture_t, accept_pointer_to_pointer_cast)
{
    // var i = 5;
    // var i32_ptr = &i;
    // var u8_ptr: u8* = i32_ptr as u8*;
    ast_expr_t* cast_expr = ast_cast_expr_create(ast_ref_expr_create("i32_ptr"),
        ast_type_pointer(ast_type_builtin(TYPE_U8)));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", nullptr, ast_int_lit_val(5))),
        ast_decl_stmt_create(ast_var_decl_create("i32_ptr", nullptr,
            ast_unary_op_create(TOKEN_AMPERSAND, ast_ref_expr_create("i")))),
        ast_decl_stmt_create(ast_var_decl_create("u8_ptr",
            ast_type_pointer(ast_type_builtin(TYPE_U8)), cast_expr)),
        nullptr);

    ASSERT_SEMA_SUCCESS(AST_NODE(block));

    ast_node_destroy(block);
}

// Pointer to usize cast
TEST(ut_sema_cast_fixture_t, accept_pointer_to_usize_cast)
{
    // var i = 5;
    // var ptr = &i;
    // var addr: usize = ptr as usize;
    ast_expr_t* cast_expr = ast_cast_expr_create(ast_ref_expr_create("ptr"),
        ast_type_builtin(TYPE_USIZE));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", nullptr, ast_int_lit_val(5))),
        ast_decl_stmt_create(ast_var_decl_create("ptr", nullptr,
            ast_unary_op_create(TOKEN_AMPERSAND, ast_ref_expr_create("i")))),
        ast_decl_stmt_create(ast_var_decl_create("addr", ast_type_builtin(TYPE_USIZE), cast_expr)),
        nullptr);

    ASSERT_SEMA_SUCCESS(AST_NODE(block));

    ast_node_destroy(block);
}

// usize to pointer cast
TEST(ut_sema_cast_fixture_t, accept_usize_to_pointer_cast)
{
    // var addr = 0x1000usize;
    // var ptr: i32* = addr as i32*;
    ast_expr_t* cast_expr = ast_cast_expr_create(ast_ref_expr_create("addr"),
        ast_type_pointer(ast_type_builtin(TYPE_I32)));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("addr", nullptr,
            ast_int_lit_create(false, 0x1000, "usize"))),
        ast_decl_stmt_create(ast_var_decl_create("ptr",
            ast_type_pointer(ast_type_builtin(TYPE_I32)), cast_expr)),
        nullptr);

    ASSERT_SEMA_SUCCESS(AST_NODE(block));

    ast_node_destroy(block);
}

// Bool to numeric cast
TEST(ut_sema_cast_fixture_t, accept_bool_to_numeric_cast)
{
    // var x: i32 = true as i32;
    ast_expr_t* cast_expr = ast_cast_expr_create(ast_bool_lit_create(true), ast_type_builtin(TYPE_I32));
    ast_stmt_t* stmt = ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_I32), cast_expr));

    ASSERT_SEMA_SUCCESS(AST_NODE(stmt));

    ast_node_destroy(stmt);
}

// INVALID - Array to numeric cast
TEST(ut_sema_cast_fixture_t, reject_array_to_numeric_cast)
{
    // var arr = [1, 2, 3];
    // var x: i32 = arr as i32;  // Error
    ast_expr_t* error_node = ast_cast_expr_create(ast_ref_expr_create("arr"), ast_type_builtin(TYPE_I32));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("arr", nullptr,
            ast_array_lit_create_va(ast_int_lit_val(1), ast_int_lit_val(2), ast_int_lit_val(3), nullptr))),
        ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_I32), error_node)),
        nullptr);

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "cannot cast");

    ast_node_destroy(block);
}

// INVALID - Pointer to i32 cast (only usize allowed)
TEST(ut_sema_cast_fixture_t, reject_pointer_to_i32_cast)
{
    // var i = 5;
    // var ptr = &i;
    // var x: i32 = ptr as i32;  // Error: must use usize, not i32
    ast_expr_t* error_node = ast_cast_expr_create(ast_ref_expr_create("ptr"), ast_type_builtin(TYPE_I32));

    ast_stmt_t* block = ast_compound_stmt_create_va(
        ast_decl_stmt_create(ast_var_decl_create("i", nullptr, ast_int_lit_val(5))),
        ast_decl_stmt_create(ast_var_decl_create("ptr", nullptr,
            ast_unary_op_create(TOKEN_AMPERSAND, ast_ref_expr_create("i")))),
        ast_decl_stmt_create(ast_var_decl_create("x", ast_type_builtin(TYPE_I32), error_node)),
        nullptr);

    ASSERT_SEMA_ERROR(AST_NODE(block), error_node, "cannot cast");

    ast_node_destroy(block);
}

// INVALID - Class to numeric cast
TEST(ut_sema_cast_fixture_t, reject_class_to_numeric_cast)
{
    // class Point { x: i32 }
    // fn test() {
    //     var obj = Point { x: 5 };
    //     var n: i32 = obj as i32;  // Error
    // }
    ast_expr_t* error_node = ast_cast_expr_create(ast_ref_expr_create("obj"), ast_type_builtin(TYPE_I32));

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        ast_fn_def_create_va("test", ast_type_builtin(TYPE_VOID),
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(ast_var_decl_create("obj", nullptr,
                    ast_construct_expr_create_va(ast_type_user_unresolved("Point"),
                        ast_member_init_create("x", ast_int_lit_val(5)),
                        nullptr))),
                ast_decl_stmt_create(ast_var_decl_create("n", ast_type_builtin(TYPE_I32), error_node)),
                nullptr),
            nullptr),
        nullptr);

    ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(AST_NODE(root), error_node, "cannot cast");

    ast_node_destroy(root);
}
