#include "decl_collector.h"

#include "ast/decl/param_decl.h"
#include "ast/def/class_def.h"
#include "ast/def/fn_def.h"
#include "ast/def/import_def.h"
#include "ast/def/method_def.h"
#include "ast/node.h"
#include "ast/type.h"
#include "ast/visitor.h"
#include "common/containers/hash_table.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/util/ssprintf.h"
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
};

// Pass 1: Register all user types
static void register_class_symbol(decl_collector_t* collector, ast_class_def_t* class_def)
{
    symbol_t* prev_symbol;
    if ((prev_symbol = symbol_table_lookup(collector->ctx->global, class_def->base.name)) != nullptr)
    {
        semantic_context_add_error(collector->ctx, class_def, ssprintf("redeclaration of name '%s'",
            class_def->base.name));
            // TODO:
            // ssprintf("redeclaration of name '%s', previously from <%s:%d>", class_def->base.name,
                // prev_symbol->ast->source_begin.filename, prev_symbol->ast->source_begin.line));
        return;
    }

    const char* proj_name = class_def->exported ? collector->ctx->project_name : nullptr;
    const char* mod_name = class_def->exported ? collector->ctx->module_name : nullptr;
    class_def->symbol = symbol_create(class_def->base.name, SYMBOL_CLASS, class_def, proj_name, mod_name, nullptr);
    class_def->symbol->type = ast_type_user(class_def->symbol);
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

    for (size_t i = 0; i < vec_size(&class_def->members); ++i)
        ast_visitor_visit(collector, vec_get(&class_def->members, i), nullptr);

    for (size_t i = 0; i < vec_size(&class_def->methods); ++i)
        ast_visitor_visit(collector, vec_get(&class_def->methods, i), nullptr);

    if (class_def->exported)
        symbol_table_insert(collector->ctx->exports, symbol_clone(class_def->symbol, false));

    collector->current_class = nullptr;
}

static bool is_valid_overload(vec_t* symbols, vec_t* parameters)
{
    size_t num_params = vec_size(parameters);
    for (size_t i = 0; i < vec_size(symbols); ++i)
    {
        symbol_t* other_symbol = vec_get(symbols, i);
        if (other_symbol->kind != SYMBOL_FUNCTION && other_symbol->kind != SYMBOL_METHOD)
            return false;

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

    // Create symbol & resolve parameter types
    const char* proj_name = fn_def->exported ? collector->ctx->project_name : nullptr;
    const char* mod_name = fn_def->exported ? collector->ctx->module_name : nullptr;
    symbol_t* symbol = symbol_create(fn_def->base.name, SYMBOL_FUNCTION, fn_def, proj_name, mod_name, nullptr);
    for (size_t i = 0; i < vec_size(&fn_def->params); ++i)
        ast_visitor_visit(collector, vec_get(&fn_def->params, i), symbol);

    // Handle function clashes with overloading rules
    vec_t* symbols = symbol_table_overloads(collector->ctx->global, fn_def->base.name);
    size_t num_prev_defs = symbols ? vec_size(symbols) : 0;
    if (symbols != nullptr && !is_valid_overload(symbols, &fn_def->params))
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
        fn_def->return_type = type_resolver_solve(collector->ctx, fn_def->return_type, fn_def);
        if (fn_def->return_type == ast_type_invalid())
        {
            symbol_destroy(symbol);
            return;
        }
    }

    // Fill in rest of symbol data
    symbol->type = ast_type_invalid();  // TODO: actual function type
    symbol->data.function.return_type = fn_def->return_type == nullptr ?
        ast_type_builtin(TYPE_VOID) : fn_def->return_type;
    fn_def->symbol = symbol;
    fn_def->overload_index = num_prev_defs;
    symbol->data.function.overload_index = fn_def->overload_index;

    symbol_table_insert(collector->ctx->global, symbol);
    if (fn_def->exported)
        symbol_table_insert(collector->ctx->exports, symbol_clone(symbol, false));
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

    member->base.type = type_resolver_solve(collector->ctx, member->base.type, member);
    if (member->base.type == ast_type_invalid())
        return;

    // Verify class does not already contain member by given name
    symbol_t* prev_def = hash_table_find(&collector->current_class->data.class.members, member->base.name);
    if (prev_def != nullptr)
    {
        semantic_context_add_error(collector->ctx, member, ssprintf("redeclaration of '%s'", member->base.name));
            // TODO:
            // ssprintf("redeclaration of '%s', previously from <%s:%d>", member->base.name,
               // AST_NODE(prev_def)->source_begin.filename, AST_NODE(prev_def)->source_begin.line));
        return;
    }

    // TODO: The information from init-expr needs to be in the symbol
    symbol_t* member_symb = symbol_create(member->base.name, SYMBOL_MEMBER, member, nullptr, nullptr, nullptr);
    member_symb->type = member->base.type;
    hash_table_insert(&collector->current_class->data.class.members, member->base.name, member_symb);
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

    // Handle method clashes with overloading rules
    vec_t* symbols = symbol_table_overloads(collector->current_class->data.class.methods, method->base.base.name);
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
        method->base.return_type = type_resolver_solve(collector->ctx, method->base.return_type, method);
        if (method->base.return_type == ast_type_invalid())
            return;
    }

    // Build symbol for method
    const char* proj_name = collector->current_class->source_project;
    const char* mod_name = collector->current_class->source_module;
    symbol_t* method_symbol = symbol_create(method->base.base.name, SYMBOL_METHOD, method, proj_name, mod_name,
        collector->current_class->name);
    method_symbol->type = ast_type_invalid();  // TODO: actual function type
    method_symbol->data.function.return_type = method->base.return_type == nullptr ?
        ast_type_builtin(TYPE_VOID) : method->base.return_type;
    for (size_t i = 0; i < vec_size(&method->base.params); ++i)
    {
        ast_param_decl_t* param = vec_get(&method->base.params, i);
        symbol_t* param_symb = symbol_create(param->name, SYMBOL_PARAMETER, param, nullptr, nullptr, nullptr);
        param_symb->type = param->type;
        vec_push(&method_symbol->data.function.parameters, param_symb);
    }
    method->symbol = method_symbol;
    method->overload_index = num_prev_defs;
    method_symbol->data.function.overload_index = method->overload_index;

    symbol_table_insert(collector->current_class->data.class.methods, method_symbol);

    // TODO: Warning if method & member share name?
}

static void collect_param_decl(void* self_, ast_param_decl_t* param_decl, void* out_)
{
    decl_collector_t* collector = self_;
    symbol_t* fn = out_;

    param_decl->type = type_resolver_solve(collector->ctx, param_decl->type, param_decl);
    if (param_decl->type == ast_type_invalid())
        return;

    if (fn != nullptr)
    {
        symbol_t* param_symb = symbol_create(param_decl->name, SYMBOL_PARAMETER, param_decl, nullptr, nullptr, nullptr);
        param_symb->type = param_decl->type;
        vec_push(&fn->data.function.parameters, param_symb);
    }
}

decl_collector_t* decl_collector_create(semantic_context_t* ctx)
{
    decl_collector_t* collector = malloc(sizeof(*collector));

    *collector = (decl_collector_t){
        .ctx = ctx,
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
