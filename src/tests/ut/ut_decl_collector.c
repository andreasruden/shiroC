#include "ast/def/fn_def.h"
#include "ast/decl/param_decl.h"
#include "ast/expr/int_lit.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/type.h"
#include "sema/decl_collector.h"
#include "sema/semantic_context.h"
#include "sema/symbol.h"
#include "test_runner.h"

TEST_FIXTURE(decl_collector_fixture_t)
{
    semantic_context_t* ctx;
    decl_collector_t* collector;
};

TEST_SETUP(decl_collector_fixture_t)
{
    fix->ctx = semantic_context_create();
    fix->collector = decl_collector_create(fix->ctx);
}

TEST_TEARDOWN(decl_collector_fixture_t)
{
    decl_collector_destroy(fix->collector);
    semantic_context_destroy(fix->ctx);
}

TEST(decl_collector_fixture_t, collect_function_with_params)
{
    // Build AST: fn add(x: int, y: int) -> int
    ast_decl_t* param_x = ast_param_decl_create("x", ast_type_builtin(TYPE_I32));
    ast_decl_t* param_y = ast_param_decl_create("y", ast_type_builtin(TYPE_F32));
    ast_def_t* fn = ast_fn_def_create_va("add", ast_type_builtin(TYPE_I32), nullptr, param_x, param_y, nullptr);
    ast_root_t* root = ast_root_create_va(fn, nullptr);

    bool result = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_TRUE(result);

    // Verify symbol was added to global scope
    symbol_t* sym = symbol_table_lookup(fix->ctx->global, "add");
    ASSERT_NEQ(nullptr, sym);
    ASSERT_EQ(SYMBOL_FUNCTION, sym->kind);
    ASSERT_EQ("add", sym->name);

    // Verify function signature
    ASSERT_EQ(ast_type_builtin(TYPE_I32), sym->type);

    // Verify parameters
    ASSERT_EQ(2, vec_size(&sym->data.function.parameters));

    ast_param_decl_t* p1 = vec_get(&sym->data.function.parameters, 0);
    ASSERT_EQ("x", p1->name);
    ASSERT_EQ(ast_type_builtin(TYPE_I32), p1->type);

    ast_param_decl_t* p2 = vec_get(&sym->data.function.parameters, 1);
    ASSERT_EQ("y", p2->name);
    ASSERT_EQ(ast_type_builtin(TYPE_F32), p2->type);

    ast_node_destroy(root);
}

TEST(decl_collector_fixture_t, collect_redeclaration_error)
{
    ast_def_t* fn1 = ast_fn_def_create_va("mul", ast_type_builtin(TYPE_I32), nullptr, nullptr);
    source_location_t begin_loc, end_loc;
    set_source_location(&begin_loc, "test.c", 42, 1);
    set_source_location(&end_loc, "test.c", 60, 2);
    ast_node_set_source(fn1, &begin_loc, &end_loc);
    ast_def_t* fn2 = ast_fn_def_create_va("mul", ast_type_builtin(TYPE_I32), nullptr, nullptr);
    ast_root_t* root = ast_root_create_va(fn1, fn2, nullptr);

    bool result = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_FALSE(result);

    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* node = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(fn2, node);
    ASSERT_NEQ(nullptr, node->errors);
    compiler_error_t* error = vec_get(node->errors, 0);
    ASSERT_EQ("redeclaration of 'mul', previously from <test.c:42>", error->description);

    ast_node_destroy(root);
}

TEST(decl_collector_fixture_t, collect_implicit_void_function)
{
    // Build AST: fn foo()
    ast_def_t* fn = ast_fn_def_create_va("foo", nullptr, nullptr, nullptr);
    ast_root_t* root = ast_root_create_va(fn, nullptr);

    bool result = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_TRUE(result);

    // Verify symbol was added to global scope
    symbol_t* sym = symbol_table_lookup(fix->ctx->global, "foo");
    ASSERT_NEQ(nullptr, sym);
    ASSERT_EQ(SYMBOL_FUNCTION, sym->kind);
    ASSERT_EQ("foo", sym->name);

    // Verify function signature
    ASSERT_EQ(ast_type_builtin(TYPE_VOID), sym->type);
    ASSERT_EQ(0, vec_size(&sym->data.function.parameters));

    ast_node_destroy(root);
}

TEST(decl_collector_fixture_t, collect_function_with_array_ptr_and_view)
{
    // Build AST: fn foo(arr: [i32, 5]*) -> view[i32]
    ast_def_t* fn = ast_fn_def_create_va("foo",
        ast_type_view(ast_type_builtin(TYPE_I32)), nullptr,
        ast_param_decl_create("arr",
            ast_type_pointer(ast_type_array_size_unresolved(ast_type_builtin(TYPE_I32), ast_int_lit_val(5)))),
        nullptr);
    ast_root_t* root = ast_root_create_va(fn, nullptr);

    bool result = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_TRUE(result);

    // Verify symbol was added to global scope
    symbol_t* sym = symbol_table_lookup(fix->ctx->global, "foo");
    ASSERT_NEQ(nullptr, sym);
    ASSERT_EQ(SYMBOL_FUNCTION, sym->kind);
    ASSERT_EQ("foo", sym->name);

    // Verify function signature
    ASSERT_EQ(ast_type_view(ast_type_builtin(TYPE_I32)), sym->type);
    ASSERT_EQ(1, vec_size(&sym->data.function.parameters));
    ASSERT_EQ(ast_type_pointer(ast_type_array(ast_type_builtin(TYPE_I32), 5)),
        ((ast_param_decl_t*)vec_get(&sym->data.function.parameters, 0))->type);

    ast_node_destroy(root);
}
