#include "decl_collector.h"

#include "ast/decl/param_decl.h"
#include "ast/decl/type_param_decl.h"
#include "ast/def/class_def.h"
#include "ast/def/fn_def.h"
#include "ast/def/import_def.h"
#include "ast/def/method_def.h"
#include "ast/node.h"
#include "ast/type.h"
#include "ast/visitor.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/util/ssprintf.h"
#include "sema/expr_evaluator.h"
#include "sema/semantic_context.h"
#include "sema/symbol.h"
#include "sema/symbol_table.h"
#include "sema/type_resolver.h"
#include <bits/types/cookie_io_functions_t.h>
#include <stddef.h>
#include <string.h>

struct decl_collector
{
    ast_visitor_t base;
    semantic_context_t* ctx;  // decl_collector does not own ctx
    symbol_t* current_class;
    expr_evaluator_t* expr_eval;
};

// Pass 1: Register all user types
static void register_class_symbol(decl_collector_t* collector, ast_class_def_t* class_def)
{
    vec_t* overloads = symbol_table_overloads(collector->ctx->global, class_def->base.name);
    if (overloads != nullptr)
    {
        // Allow collision with namespace, but nothing else
        bool is_valid = true;
        for (size_t i = 0; i < vec_size(overloads) && is_valid; ++i)
        {
            symbol_t* collider = vec_get(overloads, i);
            if (collider->kind != SYMBOL_NAMESPACE)
                is_valid = false;
        }

        if (!is_valid)
        {
            semantic_context_add_error(collector->ctx, class_def, ssprintf("redeclaration of name '%s'",
                class_def->base.name));
                // TODO:
                // ssprintf("redeclaration of name '%s', previously from <%s:%d>", class_def->base.name,
                    // prev_symbol->ast->source_begin.filename, prev_symbol->ast->source_begin.line));
        }
        return;
    }

    // Check if this is a template class
    bool is_template = vec_size(&class_def->type_params) > 0;
    symbol_kind_t symbol_kind = is_template ? SYMBOL_TEMPLATE_CLASS : SYMBOL_CLASS;

    symbol_t* parent_namespace = class_def->exported ? collector->ctx->module_namespace : nullptr;
    class_def->symbol = symbol_create(class_def->base.name, symbol_kind, class_def, parent_namespace);

    // If template, register type parameters
    if (is_template)
    {
        class_def->symbol->data.template_class.template_ast = AST_NODE(class_def);
        for (size_t i = 0; i < vec_size(&class_def->type_params); ++i)
        {
            ast_type_param_decl_t* type_param = vec_get(&class_def->type_params, i);
            symbol_t* type_param_symbol = symbol_create(type_param->name, SYMBOL_TYPE_PARAMETER, type_param, nullptr);
            type_param_symbol->type = ast_type_variable(type_param->name);
            vec_push(&class_def->symbol->data.template_class.type_parameters, type_param_symbol);
            type_param->symbol = type_param_symbol;
        }
    }
    else
    {
        class_def->symbol->type = ast_type_user(class_def->symbol);
    }

    symbol_table_insert(collector->ctx->global, class_def->symbol);
}

// Pass 2: Visit members & methods
static void collect_class_def(void* self_, ast_class_def_t* class_def, void* out_)
{
    (void)out_;
    decl_collector_t* collector = self_;

    if (class_def->symbol == nullptr)
        return;  // already failed by register_class_symbol

    collector->current_class = class_def->symbol;

    // Push class scope so members are visible when resolving member types
    vec_push(&collector->ctx->scope_stack, class_def->symbol->data.class.symbols);
    collector->ctx->current = class_def->symbol->data.class.symbols;

    // For template classes, add type parameters to the class scope
    if (class_def->symbol->kind == SYMBOL_TEMPLATE_CLASS)
    {
        vec_t* type_params = &class_def->symbol->data.template_class.type_parameters;
        for (size_t i = 0; i < vec_size(type_params); ++i)
        {
            symbol_t* type_param = vec_get(type_params, i);
            symbol_table_insert(class_def->symbol->data.class.symbols, type_param);
        }
    }

    for (size_t i = 0; i < vec_size(&class_def->members); ++i)
        ast_visitor_visit(collector, vec_get(&class_def->members, i), nullptr);

    for (size_t i = 0; i < vec_size(&class_def->methods); ++i)
        ast_visitor_visit(collector, vec_get(&class_def->methods, i), nullptr);

    if (class_def->exported)
        symbol_table_insert(collector->ctx->exports, class_def->symbol);

    vec_pop(&collector->ctx->scope_stack);
    collector->ctx->current = vec_last(&collector->ctx->scope_stack);
    collector->current_class = nullptr;
}

static bool is_valid_overload(vec_t* symbols, vec_t* parameters)
{
    size_t num_params = vec_size(parameters);
    for (size_t i = 0; i < vec_size(symbols); ++i)
    {
        symbol_t* other_symbol = vec_get(symbols, i);
        if (other_symbol->kind != SYMBOL_FUNCTION && other_symbol->kind != SYMBOL_METHOD &&
            other_symbol->kind != SYMBOL_NAMESPACE)
        {
            return false;
        }

        if (other_symbol->kind == SYMBOL_NAMESPACE)
            continue;

        if (vec_size(&other_symbol->data.function.parameters) != num_params)
            continue;

        bool differs = false;
        for (size_t j = 0; j < num_params && !differs; ++j)
        {
            ast_param_decl_t* param = vec_get(parameters, j);
            symbol_t* other_param = vec_get(&other_symbol->data.function.parameters, j);
            if (param->type != other_param->type)
                differs = true;
        }

        if (!differs)
            return false;
    }

    return true;
}

static void collect_fn_def(void* self_, ast_fn_def_t* fn_def, void* out_)
{
    (void)out_;
    decl_collector_t* collector = self_;

    // Check if this is a template function
    bool is_template = vec_size(&fn_def->type_params) > 0;
    symbol_kind_t symbol_kind = is_template ? SYMBOL_TEMPLATE_FN : SYMBOL_FUNCTION;

    // Create symbol & resolve parameter types
    symbol_t* parent_namespace = fn_def->exported ? collector->ctx->module_namespace : nullptr;
    symbol_t* symbol = symbol_create(fn_def->base.name, symbol_kind, fn_def, parent_namespace);

    // If template, we need a function scope for type parameters, and we need to register them
    if (is_template)
    {
        semantic_context_push_scope(collector->ctx, SCOPE_FUNCTION);
        symbol->data.template_fn.template_ast = AST_NODE(fn_def);

        for (size_t i = 0; i < vec_size(&fn_def->type_params); ++i)
        {
            ast_type_param_decl_t* type_param = vec_get(&fn_def->type_params, i);
            symbol_t* type_param_symbol = symbol_create(type_param->name, SYMBOL_TYPE_PARAMETER, type_param, nullptr);
            type_param_symbol->type = ast_type_variable(type_param->name);
            vec_push(&symbol->data.template_fn.type_parameters, type_param_symbol);
            symbol_table_insert(collector->ctx->current, type_param_symbol);
            type_param->symbol = type_param_symbol;
        }
    }

    for (size_t i = 0; i < vec_size(&fn_def->params); ++i)
        ast_visitor_visit(collector, vec_get(&fn_def->params, i), is_template ? nullptr : symbol);

    // Pop template scope
    if (is_template)
        semantic_context_pop_scope(collector->ctx);

    // Handle function clashes with overloading rules (skip templates until instantiation)
    vec_t* symbols = symbol_table_overloads(collector->ctx->global, fn_def->base.name);
    size_t num_prev_defs = symbols ? vec_size(symbols) : 0;
    if (!is_template && symbols != nullptr && !is_valid_overload(symbols, &fn_def->params))
    {
        // ast_node_t* prev_def = ((symbol_t*)vec_get(symbols, 0))->ast;
        semantic_context_add_error(collector->ctx, fn_def, ssprintf("redeclaration of '%s'", fn_def->base.name));
            // TODO:
            // ssprintf("redeclaration of '%s', previously from <%s:%d>",
            // fn_def->base.name, AST_NODE(prev_def)->source_begin.filename, AST_NODE(prev_def)->source_begin.line));
        symbol_destroy(symbol);
        return;
    }

    // Resolve return type
    if (fn_def->return_type == nullptr)
        fn_def->return_type = ast_type_builtin(TYPE_VOID);
    else
    {
        fn_def->return_type = type_resolver_solve(collector->ctx, fn_def->return_type, fn_def, false);
        if (fn_def->return_type == ast_type_invalid())
        {
            symbol_destroy(symbol);
            return;
        }
    }

    // Fill in rest of symbol data (only for non-templates)
    if (!is_template)
    {
        symbol->type = ast_type_invalid();  // TODO: actual function type
        symbol->data.function.return_type = fn_def->return_type == nullptr ?
            ast_type_builtin(TYPE_VOID) : fn_def->return_type;
        fn_def->overload_index = num_prev_defs;
        symbol->data.function.overload_index = fn_def->overload_index;
        symbol->data.function.extern_abi = fn_def->extern_abi ? strdup(fn_def->extern_abi) : nullptr;
    }

    fn_def->symbol = symbol;

    symbol_table_insert(collector->ctx->global, symbol);
    if (fn_def->exported)
        symbol_table_insert(collector->ctx->exports, symbol);
}

static void collect_import_def(void* self_, ast_import_def_t* import, void* out_)
{
    (void)out_;
    decl_collector_t* collector = self_;

    for (size_t i = 0; i < vec_size(&collector->ctx->imports); ++i)
    {
        ast_import_def_t* other_import = vec_get(&collector->ctx->imports, i);
        if (strcmp(other_import->project_name, import->project_name) != 0)
            continue;
        if (strcmp(other_import->module_name, import->module_name) != 0)
            continue;
        semantic_context_add_error(collector->ctx, import, ssprintf("%s.%s has already been imported",
            import->project_name, import->module_name));
        return;
    }

    vec_push(&collector->ctx->imports, import);
}

static void collect_member_decl(void* self_, ast_member_decl_t* member, void* out_)
{
    (void)out_;
    decl_collector_t* collector = self_;
    panic_if(collector->current_class == nullptr);

    // TODO: Parser should not allow this construct if we are commited to not allow this
    //       reason against: decl collector would need to determine the type, not its responsibility
    //       counter-arg:    yes, but we need a constant expr interpreter anyway, it could just use that
    if (member->base.type == nullptr)
    {
        semantic_context_add_error(collector->ctx, member, "type annotation is mandatory for class members (TODO)");
        return;
    }

    member->base.type = type_resolver_solve(collector->ctx, member->base.type, member, true);
    if (member->base.type == ast_type_invalid())
        return;

    // Verify class does not already contain member by given name
    // Note: data.class.symbols works for both regular and template classes because
    // data.template.symbols is the first field and aliases with data.class.symbols
    symbol_t* prev_def = symbol_table_lookup_local(collector->current_class->data.class.symbols, member->base.name);
    if (prev_def != nullptr)
    {
        semantic_context_add_error(collector->ctx, member, ssprintf("redeclaration of '%s'", member->base.name));
            // TODO:
            // ssprintf("redeclaration of '%s', previously from <%s:%d>", member->base.name,
               // AST_NODE(prev_def)->source_begin.filename, AST_NODE(prev_def)->source_begin.line));
        return;
    }

    // Evaluate the init-expression (returned node is a copy that symbol will own)
    ast_expr_t* default_expr = nullptr;
    if (member->base.init_expr != nullptr)
    {
        default_expr = expr_evaluator_eval(collector->expr_eval, member->base.init_expr);
        if (default_expr == nullptr)
        {
            semantic_context_add_error(collector->ctx, member->base.init_expr, collector->expr_eval->last_error);
            return;
        }

        // NOTE: semantic_analyzer verifies type of init_expr, here we do not
        default_expr->type = member->base.type;
    }

    symbol_t* member_symb = symbol_create(member->base.name, SYMBOL_MEMBER, member, collector->current_class);
    member_symb->data.member.default_value = default_expr;
    member_symb->type = member->base.type;
    symbol_table_insert(collector->current_class->data.class.symbols, member_symb);
}

static bool handle_trait_impl(decl_collector_t* collector, ast_method_def_t* method)
{
    const char* name = method->base.base.name;

    bool bad_signature = false;

    if (strcmp("destruct", name) == 0)
    {
        if (method->base.return_type != ast_type_builtin(TYPE_VOID) || vec_size(&method->base.params) != 0)
            bad_signature = true;
        else
        {
            ast_type_set_trait(ast_type_user(collector->current_class), TRAIT_EXPLICIT_DESTRUCTOR);
            ast_type_clear_trait(ast_type_user(collector->current_class), TRAIT_COPYABLE);
        }
    }
    else
    {
        semantic_context_add_error(collector->ctx, method, ssprintf("there exists no '%s' trait", name));
        return false;
    }

    if (bad_signature)
    {
        semantic_context_add_error(collector->ctx, method, ssprintf("incorrect signature for trait '%s'", name));
        return false;
    }

    return true;
}

static void collect_method_def(void* self_, ast_method_def_t* method, void* out_)
{
    // TODO: This is very similar to fn_def, try to share implementation

    (void)out_;
    decl_collector_t* collector = self_;
    panic_if(collector->current_class == nullptr);

    // Resolve parameter types
    for (size_t i = 0; i < vec_size(&method->base.params); ++i)
        ast_visitor_visit(collector, vec_get(&method->base.params, i), nullptr);

    const char* method_name = method->is_trait_impl ? ssprintf("@%s", method->base.base.name) : method->base.base.name;

    // Handle method clashes with overloading rules
    vec_t* symbols = symbol_table_overloads(collector->current_class->data.class.symbols, method_name);
    size_t num_prev_defs = symbols ? vec_size(symbols) : 0;
    if (symbols != nullptr && !is_valid_overload(symbols, &method->base.params))
    {
        // ast_node_t* prev_def = ((symbol_t*)vec_get(symbols, 0))->ast;
        semantic_context_add_error(collector->ctx, method, ssprintf("redeclaration of '%s'", method->base.base.name));
            // TODO:
            // ssprintf("redeclaration of '%s', previously from <%s:%d>",
            // method->base.base.name, AST_NODE(prev_def)->source_begin.filename, AST_NODE(prev_def)->source_begin.line));
        return;
    }

    // Resolve return type
    if (method->base.return_type == nullptr)
        method->base.return_type = ast_type_builtin(TYPE_VOID);
    else
    {
        method->base.return_type = type_resolver_solve(collector->ctx, method->base.return_type, method, false);
        if (method->base.return_type == ast_type_invalid())
            return;
    }

    // Special verification for trait implementations
    if (method->is_trait_impl && !handle_trait_impl(collector, method))
        return;

    // Build symbol for method
    symbol_t* method_symbol = symbol_create(method_name,
        method->is_trait_impl ? SYMBOL_TRAIT_IMPL : SYMBOL_METHOD,
        method, collector->current_class);
    method_symbol->type = ast_type_invalid();  // TODO: actual function type
    method_symbol->data.function.return_type = method->base.return_type == nullptr ?
        ast_type_builtin(TYPE_VOID) : method->base.return_type;
    for (size_t i = 0; i < vec_size(&method->base.params); ++i)
    {
        ast_param_decl_t* param = vec_get(&method->base.params, i);
        symbol_t* param_symb = symbol_create(param->name, SYMBOL_PARAMETER, param, nullptr);
        param_symb->type = param->type;
        vec_push(&method_symbol->data.function.parameters, param_symb);
    }
    method->symbol = method_symbol;
    method->overload_index = num_prev_defs;
    method_symbol->data.function.overload_index = method->overload_index;

    symbol_table_insert(collector->current_class->data.class.symbols, method_symbol);
}

static void collect_param_decl(void* self_, ast_param_decl_t* param_decl, void* out_)
{
    decl_collector_t* collector = self_;
    symbol_t* fn = out_;

    param_decl->type = type_resolver_solve(collector->ctx, param_decl->type, param_decl, false);
    if (param_decl->type == ast_type_invalid())
        return;

    if (fn != nullptr)
    {
        symbol_t* param_symb = symbol_create(param_decl->name, SYMBOL_PARAMETER, param_decl, nullptr);
        param_symb->type = param_decl->type;
        vec_push(&fn->data.function.parameters, param_symb);
    }
}

decl_collector_t* decl_collector_create(semantic_context_t* ctx)
{
    decl_collector_t* collector = malloc(sizeof(*collector));

    *collector = (decl_collector_t){
        .ctx = ctx,
        .expr_eval = expr_evaluator_create(),
    };

    ast_visitor_init(&collector->base);
    collector->base.visit_class_def = collect_class_def;
    collector->base.visit_fn_def = collect_fn_def;
    collector->base.visit_import_def = collect_import_def;
    collector->base.visit_member_decl = collect_member_decl;
    collector->base.visit_method_def = collect_method_def;
    collector->base.visit_param_decl = collect_param_decl;

    return collector;
}

void decl_collector_destroy(decl_collector_t* collector)
{
    if (collector == nullptr)
        return;

    expr_evaluator_destroy(collector->expr_eval);
    free(collector);
}

bool decl_collector_run(decl_collector_t* collector, ast_node_t* node)
{
    size_t errors = vec_size(&collector->ctx->error_nodes);

    // Register all user-types first
    if (AST_KIND(node) == AST_ROOT)
    {
        ast_root_t* root = (ast_root_t*)node;
        for (size_t i = 0; i < vec_size(&root->tl_defs); ++i)
        {
            ast_def_t* def = vec_get(&root->tl_defs, i);
            if (AST_KIND(def) == AST_DEF_CLASS)
                register_class_symbol(collector, (ast_class_def_t*)def);
        }
    }

    ast_visitor_visit(collector, node, nullptr);

    return errors == vec_size(&collector->ctx->error_nodes);  // no new errors
}
