#include "ast/decl/param_decl.h"
#include "ast/decl/type_param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/class_def.h"
#include "ast/def/fn_def.h"
#include "ast/def/method_def.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/member_access.h"
#include "ast/expr/ref_expr.h"
#include "ast/expr/self_expr.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "ast/type.h"
#include "sema/decl_collector.h"
#include "sema/semantic_analyzer.h"
#include "sema/symbol.h"
#include "sema/symbol_table.h"
#include "sema/template_instantiator.h"
#include "test_runner.h"
#include "sema_shared.h"

TEST_FIXTURE(ut_sema_templates_fixture_t)
{
    semantic_analyzer_t* sema;
    semantic_context_t* ctx;
    decl_collector_t* collector;
};

TEST_SETUP(ut_sema_templates_fixture_t)
{
    fix->ctx = semantic_context_create("test", "sema_templates");
    ASSERT_NEQ(nullptr, fix->ctx);

    fix->collector = decl_collector_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->collector);

    fix->sema = semantic_analyzer_create(fix->ctx);
    ASSERT_NEQ(nullptr, fix->sema);
}

TEST_TEARDOWN(ut_sema_templates_fixture_t)
{
    semantic_analyzer_destroy(fix->sema);
    decl_collector_destroy(fix->collector);
    semantic_context_destroy(fix->ctx);
    ast_type_cache_reset();
}

// Test that a simple templated class is registered as a template symbol
TEST(ut_sema_templates_fixture_t, template_class_registered_as_template)
{
    // Create a simple templated class: class Box<T> { var value: T; }
    vec_t type_params = VEC_INIT(ast_node_destroy);
    vec_push(&type_params, ast_type_param_decl_create("T"));

    vec_t members = VEC_INIT(ast_node_destroy);
    vec_push(&members, ast_member_decl_create("value", ast_type_variable("T"), nullptr));

    vec_t methods = VEC_INIT(ast_node_destroy);

    ast_class_def_t* class_def = (ast_class_def_t*)ast_class_def_create("Box", &members, &methods, false);
    class_def->type_params = type_params;

    ast_root_t* root = ast_root_create_va((ast_def_t*)class_def, nullptr);

    // Run decl_collector
    bool res = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_TRUE(res);

    // Look up the symbol
    symbol_t* symbol = symbol_table_lookup(fix->ctx->global, "Box");
    ASSERT_NEQ(nullptr, symbol);
    ASSERT_EQ(SYMBOL_TEMPLATE_CLASS, symbol->kind);

    ast_node_destroy(root);
}

// Test that a simple templated function is registered as a template symbol
TEST(ut_sema_templates_fixture_t, template_function_registered_as_template)
{
    // Create a simple templated function: fn identity<T>(value: T) -> T { return value; }
    vec_t type_params = VEC_INIT(ast_node_destroy);
    vec_push(&type_params, ast_type_param_decl_create("T"));

    vec_t params = VEC_INIT(ast_node_destroy);
    vec_push(&params, ast_param_decl_create("value", ast_type_variable("T")));

    ast_fn_def_t* fn_def = (ast_fn_def_t*)ast_fn_def_create("identity", &params, ast_type_variable("T"),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(ast_ref_expr_create("value")),
            nullptr),
        false);
    fn_def->type_params = type_params;

    ast_root_t* root = ast_root_create_va((ast_def_t*)fn_def, nullptr);

    // Run decl_collector
    bool res = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_TRUE(res);

    // Look up the symbol
    symbol_t* symbol = symbol_table_lookup(fix->ctx->global, "identity");
    ASSERT_NEQ(nullptr, symbol);
    ASSERT_EQ(SYMBOL_TEMPLATE_FN, symbol->kind);

    ast_node_destroy(root);
}

// Test that type parameters are registered in the template's scope
TEST(ut_sema_templates_fixture_t, type_parameters_registered_in_scope)
{
    // Create: class Pair<T, U> { var first: T; var second: U; }
    vec_t type_params = VEC_INIT(ast_node_destroy);
    vec_push(&type_params, ast_type_param_decl_create("T"));
    vec_push(&type_params, ast_type_param_decl_create("U"));

    vec_t members = VEC_INIT(ast_node_destroy);
    vec_push(&members, ast_member_decl_create("first", ast_type_variable("T"), nullptr));
    vec_push(&members, ast_member_decl_create("second", ast_type_variable("U"), nullptr));

    vec_t methods = VEC_INIT(ast_node_destroy);

    ast_class_def_t* class_def = (ast_class_def_t*)ast_class_def_create("Pair", &members, &methods, false);
    class_def->type_params = type_params;

    ast_root_t* root = ast_root_create_va((ast_def_t*)class_def, nullptr);

    // Run decl_collector
    bool res = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_TRUE(res);

    // Look up the class symbol
    symbol_t* class_symbol = symbol_table_lookup(fix->ctx->global, "Pair");
    ASSERT_NEQ(nullptr, class_symbol);
    ASSERT_EQ(SYMBOL_TEMPLATE_CLASS, class_symbol->kind);

    // Verify type parameters are in the template's scope
    ASSERT_EQ(2, vec_size(&class_symbol->data.template_class.type_parameters));

    ast_node_destroy(root);
}

// Test validation of template definition without instantiation
TEST(ut_sema_templates_fixture_t, validate_template_definition_structure)
{
    // Create a simple valid template: class Box<T> { var value: T; }
    vec_t type_params = VEC_INIT(ast_node_destroy);
    vec_push(&type_params, ast_type_param_decl_create("T"));

    vec_t members = VEC_INIT(ast_node_destroy);
    vec_push(&members, ast_member_decl_create("value", ast_type_variable("T"), nullptr));

    vec_t methods = VEC_INIT(ast_node_destroy);

    ast_class_def_t* class_def = (ast_class_def_t*)ast_class_def_create("Box", &members, &methods, false);
    class_def->type_params = type_params;

    ast_root_t* root = ast_root_create_va((ast_def_t*)class_def, nullptr);

    // Run both decl_collector and semantic_analyzer
    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    ast_node_destroy(root);
}

// Test templated class with templated methods
TEST(ut_sema_templates_fixture_t, templated_class_with_methods)
{
    // Create: class Container<T> { var data: T; fn get() -> T { return self.data; } }
    vec_t type_params = VEC_INIT(ast_node_destroy);
    vec_push(&type_params, ast_type_param_decl_create("T"));

    vec_t members = VEC_INIT(ast_node_destroy);
    vec_push(&members, ast_member_decl_create("data", ast_type_variable("T"), nullptr));

    vec_t methods = VEC_INIT(ast_node_destroy);
    vec_push(&methods, ast_method_def_create_va("get", ast_type_variable("T"),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(
                ast_member_access_create(ast_self_expr_create(false), "data")),
            nullptr),
        nullptr));

    ast_class_def_t* class_def = (ast_class_def_t*)ast_class_def_create("Container", &members, &methods, false);
    class_def->type_params = type_params;

    ast_root_t* root = ast_root_create_va((ast_def_t*)class_def, nullptr);

    // Run decl_collector and semantic_analyzer
    ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(AST_NODE(root));

    ast_node_destroy(root);
}

// Test that type variables are only valid within template scope
TEST(ut_sema_templates_fixture_t, type_variable_outside_template_scope_error)
{
    // Try to use type variable T outside of any template context
    ast_decl_t* error_node = ast_var_decl_create("x", ast_type_variable("T"), nullptr);

    ast_root_t* root = ast_root_create_va(
        ast_fn_def_create_va("main", ast_type_builtin(TYPE_VOID),
            ast_compound_stmt_create_va(
                ast_decl_stmt_create(error_node),
                nullptr),
            nullptr),
        nullptr);

    // This should fail during semantic analysis
    bool decl_res = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_TRUE(decl_res);
    bool res = semantic_analyzer_run(fix->sema, AST_NODE(root));
    ASSERT_FALSE(res);
    ASSERT_LT(0, vec_size(&fix->ctx->error_nodes));

    ast_node_destroy(root);
}

// Test multiple template definitions
TEST(ut_sema_templates_fixture_t, multiple_template_definitions)
{
    // Create two separate template classes
    vec_t type_params1 = VEC_INIT(ast_node_destroy);
    vec_push(&type_params1, ast_type_param_decl_create("T"));

    vec_t members1 = VEC_INIT(ast_node_destroy);
    vec_push(&members1, ast_member_decl_create("value", ast_type_variable("T"), nullptr));

    vec_t methods1 = VEC_INIT(ast_node_destroy);

    ast_class_def_t* class_def1 = (ast_class_def_t*)ast_class_def_create("Box", &members1, &methods1, false);
    class_def1->type_params = type_params1;

    // Second template
    vec_t type_params2 = VEC_INIT(ast_node_destroy);
    vec_push(&type_params2, ast_type_param_decl_create("A"));
    vec_push(&type_params2, ast_type_param_decl_create("B"));

    vec_t members2 = VEC_INIT(ast_node_destroy);
    vec_push(&members2, ast_member_decl_create("first", ast_type_variable("A"), nullptr));
    vec_push(&members2, ast_member_decl_create("second", ast_type_variable("B"), nullptr));

    vec_t methods2 = VEC_INIT(ast_node_destroy);

    ast_class_def_t* class_def2 = (ast_class_def_t*)ast_class_def_create("Pair", &members2, &methods2, false);
    class_def2->type_params = type_params2;

    ast_root_t* root = ast_root_create_va(
        (ast_def_t*)class_def1,
        (ast_def_t*)class_def2,
        nullptr);

    // Run decl_collector
    bool res = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_TRUE(res);

    // Look up both symbols
    symbol_t* box_symbol = symbol_table_lookup(fix->ctx->global, "Box");
    ASSERT_NEQ(nullptr, box_symbol);
    ASSERT_EQ(SYMBOL_TEMPLATE_CLASS, box_symbol->kind);
    ASSERT_EQ(1, vec_size(&box_symbol->data.template_class.type_parameters));

    symbol_t* pair_symbol = symbol_table_lookup(fix->ctx->global, "Pair");
    ASSERT_NEQ(nullptr, pair_symbol);
    ASSERT_EQ(SYMBOL_TEMPLATE_CLASS, pair_symbol->kind);
    ASSERT_EQ(2, vec_size(&pair_symbol->data.template_class.type_parameters));

    ast_node_destroy(root);
}

// Test templated function with multiple type parameters
TEST(ut_sema_templates_fixture_t, template_function_multiple_type_params)
{
    // Create: fn convert<T, U>(value: T) -> U { ... }
    vec_t type_params = VEC_INIT(ast_node_destroy);
    vec_push(&type_params, ast_type_param_decl_create("T"));
    vec_push(&type_params, ast_type_param_decl_create("U"));

    vec_t params = VEC_INIT(ast_node_destroy);
    vec_push(&params, ast_param_decl_create("value", ast_type_variable("T")));

    ast_fn_def_t* fn_def = (ast_fn_def_t*)ast_fn_def_create("convert", &params, ast_type_variable("U"),
        ast_compound_stmt_create_va(
            ast_return_stmt_create(ast_ref_expr_create("value")),
            nullptr),
        false);
    fn_def->type_params = type_params;

    ast_root_t* root = ast_root_create_va((ast_def_t*)fn_def, nullptr);

    // Run decl_collector
    bool res = decl_collector_run(fix->collector, AST_NODE(root));
    ASSERT_TRUE(res);

    // Look up the symbol
    symbol_t* symbol = symbol_table_lookup(fix->ctx->global, "convert");
    ASSERT_NEQ(nullptr, symbol);
    ASSERT_EQ(SYMBOL_TEMPLATE_FN, symbol->kind);
    ASSERT_EQ(2, vec_size(&symbol->data.template_fn.type_parameters));

    ast_node_destroy(root);
}
