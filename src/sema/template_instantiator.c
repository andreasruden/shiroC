#include "template_instantiator.h"

#include "ast/def/class_def.h"
#include "ast/def/fn_def.h"
#include "ast/transformer.h"
#include "ast/util/cloner.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/util/ssprintf.h"
#include "sema/semantic_analyzer.h"
#include "sema/semantic_context.h"
#include <string.h>

// Type substitution transformer: replaces type variables with concrete types
typedef struct type_substitutor
{
    ast_transformer_t base;
    symbol_t** type_param_symbols;  // array of type parameter symbols
    ast_type_t** type_args;          // array of concrete type arguments
    size_t num_type_params;
} type_substitutor_t;

// Forward declarations
static ast_type_t* substitute_type(type_substitutor_t* sub, ast_type_t* type);
static void* transform_var_decl_subst(void* self_, ast_var_decl_t* var, void* out_);
static void* transform_param_decl_subst(void* self_, ast_param_decl_t* param, void* out_);
static void* transform_member_decl_subst(void* self_, ast_member_decl_t* member, void* out_);
static void* transform_fn_def_subst(void* self_, ast_fn_def_t* fn, void* out_);

// Substitute type variables in a type with concrete types
static ast_type_t* substitute_type(type_substitutor_t* sub, ast_type_t* type)
{
    if (type == nullptr || type == ast_type_invalid())
        return type;

    // If this is a type variable, replace it
    if (type->kind == AST_TYPE_VARIABLE)
    {
        for (size_t i = 0; i < sub->num_type_params; ++i)
        {
            if (strcmp(type->data.type_variable.name, sub->type_param_symbols[i]->name) == 0)
                return sub->type_args[i];
        }
        // Type variable not found in substitution - shouldn't happen
        panic("Type variable '%s' not found in substitution map", type->data.type_variable.name);
    }

    // Recursively substitute in composite types
    if (type->kind == AST_TYPE_POINTER)
        return ast_type_pointer(substitute_type(sub, type->data.pointer.pointee));

    if (type->kind == AST_TYPE_ARRAY)
    {
        if (type->data.array.size_known)
            return ast_type_array(substitute_type(sub, type->data.array.element_type), type->data.array.size);
        else
            return ast_type_array_size_unresolved(substitute_type(sub, type->data.array.element_type),
                type->data.array.size_expr);
    }

    if (type->kind == AST_TYPE_HEAP_ARRAY)
        return ast_type_heap_array(substitute_type(sub, type->data.heap_array.element_type));

    if (type->kind == AST_TYPE_VIEW)
        return ast_type_view(substitute_type(sub, type->data.view.element_type));

    // Builtin and user types don't need substitution
    return type;
}

static void* transform_var_decl_subst(void* self_, ast_var_decl_t* var, void* out_)
{
    (void)out_;
    type_substitutor_t* sub = self_;
    if (var->type != nullptr)
        var->type = substitute_type(sub, var->type);
    // Recursively transform init expression if present
    if (var->init_expr != nullptr)
        var->init_expr = ast_transformer_transform(self_, var->init_expr, nullptr);
    return var;
}

static void* transform_param_decl_subst(void* self_, ast_param_decl_t* param, void* out_)
{
    (void)out_;
    type_substitutor_t* sub = self_;
    if (param->type != nullptr)
        param->type = substitute_type(sub, param->type);
    return param;
}

static void* transform_member_decl_subst(void* self_, ast_member_decl_t* member, void* out_)
{
    (void)out_;
    type_substitutor_t* sub = self_;
    if (member->base.type != nullptr)
        member->base.type = substitute_type(sub, member->base.type);
    // Recursively transform init expression if present
    if (member->base.init_expr != nullptr)
        member->base.init_expr = ast_transformer_transform(self_, member->base.init_expr, nullptr);
    return member;
}

static void* transform_fn_def_subst(void* self_, ast_fn_def_t* fn, void* out_)
{
    (void)out_;
    type_substitutor_t* sub = self_;
    if (fn->return_type != nullptr)
        fn->return_type = substitute_type(sub, fn->return_type);
    // Recursively transform parameters and body
    for (size_t i = 0; i < vec_size(&fn->params); ++i)
    {
        ast_param_decl_t* param = vec_get(&fn->params, i);
        ast_transformer_transform(self_, param, nullptr);
    }
    if (fn->body != nullptr)
        fn->body = ast_transformer_transform(self_, fn->body, nullptr);
    return fn;
}

__attribute__((unused))
static type_substitutor_t* type_substitutor_create(symbol_t** type_param_symbols, ast_type_t** type_args,
    size_t num_type_params)
{
    type_substitutor_t* sub = malloc(sizeof(*sub));
    ast_transformer_init(&sub->base);

    // Override transformers that need type substitution
    sub->base.transform_var_decl = transform_var_decl_subst;
    sub->base.transform_param_decl = transform_param_decl_subst;
    sub->base.transform_member_decl = transform_member_decl_subst;
    sub->base.transform_fn_def = transform_fn_def_subst;

    sub->type_param_symbols = type_param_symbols;
    sub->type_args = type_args;
    sub->num_type_params = num_type_params;

    return sub;
}

__attribute__((unused))
static void type_substitutor_destroy(type_substitutor_t* sub)
{
    free(sub);
}

// Check if an instantiation with the given type args already exists
static symbol_t* find_cached_instantiation_fn(symbol_t* template_symbol, vec_t* type_args)
{
    size_t num_type_args = vec_size(type_args);
    for (size_t i = 0; i < vec_size(&template_symbol->data.template_fn.instantiations); ++i)
    {
        symbol_t* instance = vec_get(&template_symbol->data.template_fn.instantiations, i);

        // Check if type arguments match
        if (vec_size(&instance->data.template_fn_inst.type_arguments) != num_type_args)
            continue;

        bool match = true;
        for (size_t j = 0; j < num_type_args; ++j)
        {
            ast_type_t* cached_arg = vec_get(&instance->data.template_fn_inst.type_arguments, j);
            ast_type_t* arg = vec_get(type_args, j);
            if (cached_arg != arg)
            {
                match = false;
                break;
            }
        }

        if (match)
            return instance;
    }

    return nullptr;
}

static symbol_t* find_cached_instantiation_class(symbol_t* template_symbol, vec_t* type_args)
{
    size_t num_type_args = vec_size(type_args);
    for (size_t i = 0; i < vec_size(&template_symbol->data.template_class.instantiations); ++i)
    {
        symbol_t* instance = vec_get(&template_symbol->data.template_class.instantiations, i);

        // Check if type arguments match
        if (vec_size(&instance->data.template_class_inst.type_arguments) != num_type_args)
            continue;

        bool match = true;
        for (size_t j = 0; j < num_type_args; ++j)
        {
            ast_type_t* cached_arg = vec_get(&instance->data.template_class_inst.type_arguments, j);
            ast_type_t* arg = vec_get(type_args, j);
            if (cached_arg != arg)
            {
                match = false;
                break;
            }
        }

        if (match)
            return instance;
    }

    return nullptr;
}

symbol_t* instantiate_template_function(semantic_context_t* ctx, symbol_t* template_symbol, vec_t* type_args)
{
    panic_if(template_symbol->kind != SYMBOL_TEMPLATE_FN);

    size_t num_type_args = vec_size(type_args);
    // Check type argument count
    size_t num_type_params = vec_size(&template_symbol->data.template_fn.type_parameters);
    if (num_type_params != num_type_args)
    {
        semantic_context_add_error(ctx, template_symbol->ast,
            ssprintf("Template '%s' expects %zu type arguments, got %zu", template_symbol->name,
                num_type_params, num_type_args));
        vec_destroy(type_args);
        return nullptr;
    }

    // Check cache
    symbol_t* cached = find_cached_instantiation_fn(template_symbol, type_args);
    if (cached != nullptr)
    {
        vec_destroy(type_args);
        return cached;
    }

    // Clone the template AST
    ast_fn_def_t* template_fn = (ast_fn_def_t*)template_symbol->ast;
    ast_fn_def_t* cloned_fn = ast_fn_def_clone(template_fn);
    if (cloned_fn == nullptr)
    {
        semantic_context_add_error(ctx, template_symbol->ast,
            ssprintf("Failed to clone template function '%s'", template_symbol->name));
        vec_destroy(type_args);
        return nullptr;
    }

    // Create type parameter symbol array
    symbol_t** type_param_symbols = malloc(sizeof(symbol_t*) * num_type_params);
    for (size_t i = 0; i < num_type_params; ++i)
        type_param_symbols[i] = vec_get(&template_symbol->data.template_fn.type_parameters, i);

    // Create type args array for substitutor
    ast_type_t** type_args_array = malloc(sizeof(ast_type_t*) * num_type_args);
    for (size_t i = 0; i < num_type_args; ++i)
        type_args_array[i] = vec_get(type_args, i);

    // Perform type substitution
    type_substitutor_t* sub = type_substitutor_create(type_param_symbols, type_args_array, num_type_params);
    ast_transformer_transform(sub, cloned_fn, nullptr);
    type_substitutor_destroy(sub);
    free(type_param_symbols);
    free(type_args_array);

    // Create mangled name using ast_type_string
    // TODO: Implement proper name mangling with ast_type_string for each type arg
    // For now, use simple mangling
    char* mangled_name = ssprintf("%s<...>", template_symbol->name);  // Placeholder

    // Create instance symbol
    symbol_t* instance_symbol = symbol_create(mangled_name, SYMBOL_TEMPLATE_FN_INST,
        cloned_fn, template_symbol->parent_namespace);

    // Fill in template_fn_inst data
    instance_symbol->data.template_fn_inst.template_symbol = template_symbol;
    vec_move(&instance_symbol->data.template_fn_inst.type_arguments, type_args);
    instance_symbol->data.template_fn_inst.instantiated_ast = (ast_node_t*)cloned_fn;

    // Set the symbol on the cloned AST
    cloned_fn->symbol = instance_symbol;

    // Run semantic analysis on the instantiated AST
    semantic_analyzer_t* sema = semantic_analyzer_create(ctx);
    semantic_analyzer_run(sema, (ast_node_t*)cloned_fn);
    semantic_analyzer_destroy(sema);

    // Cache the instantiation
    vec_push(&template_symbol->data.template_fn.instantiations, instance_symbol);

    return instance_symbol;
}

symbol_t* instantiate_template_class(semantic_context_t* ctx, symbol_t* template_symbol, vec_t* type_args)
{
    panic_if(template_symbol->kind != SYMBOL_TEMPLATE_CLASS);

    size_t num_type_args = vec_size(type_args);
    // Check type argument count
    size_t num_type_params = vec_size(&template_symbol->data.template_class.type_parameters);
    if (num_type_params != num_type_args)
    {
        semantic_context_add_error(ctx, template_symbol->ast,
            ssprintf("Template '%s' expects %zu type arguments, got %zu", template_symbol->name,
                num_type_params, num_type_args));
        return nullptr;
    }

    // Check cache
    symbol_t* cached = find_cached_instantiation_class(template_symbol, type_args);
    if (cached != nullptr)
        return cached;

    // Clone the template AST
    ast_class_def_t* template_class = (ast_class_def_t*)template_symbol->ast;
    ast_class_def_t* cloned_class = ast_class_def_clone(template_class);
    if (cloned_class == nullptr)
    {
        semantic_context_add_error(ctx, template_symbol->ast,
            ssprintf("Failed to clone template class '%s'", template_symbol->name));
        return nullptr;
    }

    // Create type parameter symbol array
    symbol_t** type_param_symbols = malloc(sizeof(symbol_t*) * num_type_params);
    for (size_t i = 0; i < num_type_params; ++i)
        type_param_symbols[i] = vec_get(&template_symbol->data.template_class.type_parameters, i);

    // Create type args array for substitutor
    ast_type_t** type_args_array = malloc(sizeof(ast_type_t*) * num_type_args);
    for (size_t i = 0; i < num_type_args; ++i)
        type_args_array[i] = vec_get(type_args, i);

    // Perform type substitution
    type_substitutor_t* sub = type_substitutor_create(type_param_symbols, type_args_array, num_type_params);
    ast_transformer_transform(sub, cloned_class, nullptr);
    type_substitutor_destroy(sub);
    free(type_param_symbols);
    free(type_args_array);

    // Create mangled name
    char* mangled_name = ssprintf("%s<...>", template_symbol->name);  // Placeholder

    // Create instance symbol
    symbol_t* instance_symbol = symbol_create(mangled_name, SYMBOL_TEMPLATE_CLASS_INST,
        cloned_class, template_symbol->parent_namespace);

    // Fill in template_class_inst data
    instance_symbol->data.template_class_inst.template_symbol = template_symbol;
    for (size_t i = 0; i < num_type_args; ++i)
        vec_push(&instance_symbol->data.template_class_inst.type_arguments, vec_get(type_args, i));
    instance_symbol->data.template_class_inst.instantiated_ast = (ast_node_t*)cloned_class;

    // Set the symbol on the cloned AST
    cloned_class->symbol = instance_symbol;

    // Run semantic analysis on the instantiated AST
    semantic_analyzer_t* sema = semantic_analyzer_create(ctx);
    semantic_analyzer_run(sema, (ast_node_t*)cloned_class);
    semantic_analyzer_destroy(sema);

    // Cache the instantiation
    vec_push(&template_symbol->data.template_class.instantiations, instance_symbol);

    return instance_symbol;
}
