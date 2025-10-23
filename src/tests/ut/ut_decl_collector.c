#include "ast/decl/member_decl.h"
#include "ast/decl/param_decl.h"
#include "ast/def/class_def.h"
#include "ast/def/fn_def.h"
#include "ast/def/method_def.h"
#include "ast/expr/int_lit.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/type.h"
#include "sema/decl_collector.h"
#include "sema/semantic_context.h"
#include "sema/symbol.h"
#include "sema/symbol_table.h"
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

    // Verify function signature (FIXME: actualy function type)
    ASSERT_EQ(ast_type_invalid(), sym->type);

    // Verify return type
    ASSERT_EQ(ast_type_builtin(TYPE_I32), sym->data.function.return_type);

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
    ASSERT_EQ(ast_type_invalid(), sym->type);  // FIXME: Function type
    ASSERT_EQ(ast_type_builtin(TYPE_VOID), sym->data.function.return_type);
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
    ASSERT_EQ(ast_type_invalid(), sym->type);  // FIXME
    ASSERT_EQ(ast_type_view(ast_type_builtin(TYPE_I32)), sym->data.function.return_type);
    ASSERT_EQ(1, vec_size(&sym->data.function.parameters));
    ASSERT_EQ(ast_type_pointer(ast_type_array(ast_type_builtin(TYPE_I32), 5)),
        ((ast_param_decl_t*)vec_get(&sym->data.function.parameters, 0))->type);

    ast_node_destroy(root);
}

TEST(decl_collector_fixture_t, collect_class_with_members_and_methods)
{
    // Build AST: class Point { x: i32, y: i32, fn getX() -> i32 }
    ast_def_t* class = ast_class_def_create_va("Point",
        ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
        ast_member_decl_create("y", ast_type_builtin(TYPE_I32), nullptr),
        ast_method_def_create_va("getX", ast_type_builtin(TYPE_I32), ast_compound_stmt_create_empty(), nullptr),
        nullptr);
    ast_root_t* root = ast_root_create_va(class, nullptr);

    bool result = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_TRUE(result);

    // Verify class symbol was added to global scope
    symbol_t* sym = symbol_table_lookup(fix->ctx->global, "Point");
    ASSERT_NEQ(nullptr, sym);
    ASSERT_EQ(SYMBOL_CLASS, sym->kind);
    ASSERT_EQ("Point", sym->name);

    // Verify members were collected into the class symbol
    ast_member_decl_t* member_x = hash_table_find(&sym->data.class.members, "x");
    ASSERT_NEQ(nullptr, member_x);
    ASSERT_EQ("x", member_x->base.name);
    ASSERT_EQ(ast_type_builtin(TYPE_I32), member_x->base.type);

    ast_member_decl_t* member_y = hash_table_find(&sym->data.class.members, "y");
    ASSERT_NEQ(nullptr, member_y);
    ASSERT_EQ("y", member_y->base.name);
    ASSERT_EQ(ast_type_builtin(TYPE_I32), member_y->base.type);

    // Verify methods were collected into the class symbol
    symbol_t* method_getX = symbol_table_lookup(sym->data.class.methods, "getX");
    ASSERT_NEQ(nullptr, method_getX);
    ASSERT_EQ("getX", method_getX->name);
    ASSERT_EQ(SYMBOL_METHOD, method_getX->kind);
    ASSERT_EQ(ast_type_builtin(TYPE_I32), method_getX->data.function.return_type);

    ast_node_destroy(root);
}

TEST(decl_collector_fixture_t, duplicate_class_names)
{
    ast_def_t* error_node = ast_class_def_create_va("Point", nullptr);

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        error_node,
        nullptr);

    bool result = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_FALSE(result);

    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* node = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, node);

    ast_node_destroy(root);
}

TEST(decl_collector_fixture_t, duplicate_member_names_in_class)
{
    ast_decl_t* error_node = ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr);

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Point",
            ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr),
            error_node,
            nullptr),
        nullptr);

    bool result = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_FALSE(result);

    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* node = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, node);

    ast_node_destroy(root);
}

TEST(decl_collector_fixture_t, duplicate_function_signature_error)
{
    // fn foo(x: i32) -> i32
    ast_def_t* fn1 = ast_fn_def_create_va("foo", ast_type_builtin(TYPE_I32), nullptr,
        ast_param_decl_create("x", ast_type_builtin(TYPE_I32)), nullptr);

    // fn foo(x: i32) -> i32  // Duplicate signature - should error
    ast_def_t* error_node = ast_fn_def_create_va("foo", ast_type_builtin(TYPE_I32), nullptr,
        ast_param_decl_create("x", ast_type_builtin(TYPE_I32)), nullptr);

    ast_root_t* root = ast_root_create_va(fn1, error_node, nullptr);

    bool result = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_FALSE(result);

    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* node = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, node);
    ASSERT_NEQ(nullptr, node->errors);
    compiler_error_t* error = vec_get(node->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "redeclaration"));

    ast_node_destroy(root);
}

TEST(decl_collector_fixture_t, duplicate_method_signature_error)
{
    // class Calc { fn compute(x: i32) -> i32, fn compute(x: i32) -> i32 }
    ast_def_t* method1 = ast_method_def_create_va("compute", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_empty(), ast_param_decl_create("x", ast_type_builtin(TYPE_I32)), nullptr);

    ast_def_t* error_node = ast_method_def_create_va("compute", ast_type_builtin(TYPE_I32),
        ast_compound_stmt_create_empty(), ast_param_decl_create("x", ast_type_builtin(TYPE_I32)), nullptr);

    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("Calc",
            method1,
            error_node,
            nullptr),
        nullptr);

    bool result = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_FALSE(result);

    ASSERT_EQ(1, vec_size(&fix->ctx->error_nodes));
    ast_node_t* node = vec_get(&fix->ctx->error_nodes, 0);
    ASSERT_EQ(error_node, node);
    ASSERT_NEQ(nullptr, node->errors);
    compiler_error_t* error = vec_get(node->errors, 0);
    ASSERT_NEQ(nullptr, strstr(error->description, "redeclaration"));

    ast_node_destroy(root);
}

TEST(decl_collector_fixture_t, forward_reference_to_class_in_method_param)
{
    // class First { fn method(second: Second*) }
    // class Second { var i: i32 = 0; }
    ast_root_t* root = ast_root_create_va(
        ast_class_def_create_va("First",
            ast_method_def_create_va("method", nullptr,
                ast_compound_stmt_create_empty(),
                ast_param_decl_create("second", ast_type_pointer(ast_type_user("Second"))),
                nullptr),
            nullptr),
        ast_class_def_create_va("Second",
            ast_member_decl_create("i", ast_type_builtin(TYPE_I32), nullptr),
            nullptr),
        nullptr);

    bool result = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_TRUE(result);
    ASSERT_EQ(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(root);
}

TEST(decl_collector_fixture_t, exported_function_in_export_table)
{
    vec_t params = VEC_INIT(ast_node_destroy);
    ast_def_t* fn = ast_fn_def_create("foo", &params, ast_type_builtin(TYPE_I32), nullptr, true);
    ast_root_t* root = ast_root_create_va(fn, nullptr);

    bool result = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_TRUE(result);

    symbol_t* global_sym = symbol_table_lookup(fix->ctx->global, "foo");
    ASSERT_NEQ(nullptr, global_sym);
    ASSERT_EQ(SYMBOL_FUNCTION, global_sym->kind);

    symbol_t* export_sym = symbol_table_lookup(fix->ctx->export, "foo");
    ASSERT_NEQ(nullptr, export_sym);
    ASSERT_EQ(SYMBOL_FUNCTION, export_sym->kind);

    ast_node_destroy(root);
}

TEST(decl_collector_fixture_t, exported_class_in_export_table)
{
    vec_t members = VEC_INIT(ast_node_destroy);
    vec_push(&members, ast_member_decl_create("x", ast_type_builtin(TYPE_I32), nullptr));
    vec_t methods = VEC_INIT(ast_node_destroy);
    vec_push(&methods, ast_method_def_create_va("method", nullptr, ast_compound_stmt_create_empty(), nullptr));
    ast_def_t* cls = ast_class_def_create("Point", &members, &methods, true);
    ast_root_t* root = ast_root_create_va(cls, nullptr);

    bool result = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_TRUE(result);

    symbol_t* global_sym = symbol_table_lookup(fix->ctx->global, "Point");
    ASSERT_NEQ(nullptr, global_sym);
    ASSERT_EQ(SYMBOL_CLASS, global_sym->kind);

    symbol_t* export_sym = symbol_table_lookup(fix->ctx->export, "Point");
    ASSERT_NEQ(nullptr, export_sym);
    ASSERT_EQ(SYMBOL_CLASS, export_sym->kind);
    ASSERT_EQ(1, export_sym->data.class.members.size);
    ASSERT_EQ(1, export_sym->data.class.methods->map.size);
    symbol_t* method_sym = symbol_table_lookup(export_sym->data.class.methods, "method");
    ASSERT_NEQ(nullptr, method_sym);
    ASSERT_EQ(SYMBOL_METHOD, method_sym->kind);

    ast_node_destroy(root);
}
