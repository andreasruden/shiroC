#include "type_resolver.h"

#include "ast/expr/int_lit.h"
#include "ast/type.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/util/ssprintf.h"
#include "sema/semantic_context.h"
#include "sema/symbol_table.h"
#include "sema/template_instantiator.h"
#include <string.h>

static ast_type_t* solve_array_size(semantic_context_t* ctx, ast_type_t* type, ast_type_t* inner_type, void* node)
{
    panic_if(type->kind != AST_TYPE_ARRAY || type->data.array.size_known);

    // TODO: More advanced resolution
    if (AST_KIND(type->data.array.size_expr) != AST_EXPR_INT_LIT)
    {
        semantic_context_add_error(ctx, node, "invalid array-size expression");
        return ast_type_invalid();
    }

    if (((ast_int_lit_t*)type->data.array.size_expr)->has_minus_sign)
    {
        semantic_context_add_error(ctx, node, "array-size must be > 0");
        return ast_type_invalid();
    }

    size_t size = ((ast_int_lit_t*)type->data.array.size_expr)->value.as_unsigned;

    return ast_type_array(inner_type, size);
}

static symbol_t* lookup_class_symbol(semantic_context_t* ctx, const char* name, void* node)
{
    vec_t* overloads = symbol_table_overloads(ctx->global, name);
    if (overloads == nullptr || vec_size(overloads) == 0)
    {
        semantic_context_add_error(ctx, node, ssprintf("undefined type '%s'", name));
        return nullptr;
    }

    // NOTE: decl collector will fail a redefine of a type in the same module; so here we just worry about ambiguity
    //       across modules: ambiguity is okay if one name is from our module, but an error if both names are imports.
    symbol_t* selected_class = nullptr;
    for (size_t i = 0; i < vec_size(overloads); ++i)
    {
        symbol_t* class_symb = vec_get(overloads, i);
        bool is_ours = class_symb->parent_namespace == nullptr || class_symb->parent_namespace == ctx->module_namespace;
        if (is_ours)
        {
            selected_class = class_symb;
            break;
        }
        else if (selected_class == nullptr)
        {
            selected_class = class_symb;
            continue;
        }
        else
        {
            semantic_context_add_error(ctx, node, "ambiguous name resolution");
            break;
        }
    }
    return selected_class;
}

ast_type_t* type_resolver_solve(semantic_context_t* ctx, ast_type_t* type, void* node)
{
    // TODO: We could consider cleaning up the no-longer used types already (when the inner type had expressions
    //       that needed resolving, the previous type is replaced and no longer referenced anywhere: the unresolved
    //       types are all unique)

    ast_type_t* inner_type = nullptr;
    switch (type->kind)
    {
        case AST_TYPE_ARRAY:
            inner_type = type_resolver_solve(ctx, type->data.array.element_type, node);
            if (!type->data.array.size_known)
                return solve_array_size(ctx, type, inner_type, node);
            if (inner_type == type->data.array.element_type)
                return type;  // no changes
            return ast_type_array(inner_type, type->data.array.size_known);
        case AST_TYPE_HEAP_ARRAY:
            inner_type = type_resolver_solve(ctx, type->data.heap_array.element_type, node);
            if (inner_type == type->data.heap_array.element_type)
                return type;  // no changes
            return ast_type_heap_array(inner_type);
        case AST_TYPE_VIEW:
            inner_type = type_resolver_solve(ctx, type->data.view.element_type, node);
            if (inner_type == type->data.heap_array.element_type)
                return type;  // no changes
            return ast_type_view(inner_type);
        case AST_TYPE_POINTER:
            inner_type = type_resolver_solve(ctx, type->data.pointer.pointee, node);
            if (inner_type == type->data.heap_array.element_type)
                return type;  // no changes
            return ast_type_pointer(inner_type);
        case AST_TYPE_VARIABLE:
            // Type variables are already resolved, just return them
            return type;
        case AST_TYPE_TEMPLATE_INSTANCE:
            // Template instances are already fully resolved
            return type;
        case AST_TYPE_CLASS:
            if (type->data.class.class_symbol == nullptr)
            {
                // First check if this is a type parameter in the current scope
                // (needed when resolving types inside template definitions)
                symbol_t* type_param = symbol_table_lookup(ctx->current, type->data.class.name);
                if (type_param != nullptr && type_param->kind == SYMBOL_TYPE_PARAMETER)
                    return type_param->type;  // Return the AST_TYPE_VARIABLE

                symbol_t* class_symb = lookup_class_symbol(ctx, type->data.class.name, node);
                if (class_symb == nullptr)
                    return ast_type_invalid();

                // Handle template instantiation
                if (type->data.class.type_arguments != nullptr && vec_size(type->data.class.type_arguments) > 0)
                {
                    size_t num_type_args = vec_size(type->data.class.type_arguments);

                    // First, resolve all type arguments
                    vec_t resolved_args = VEC_INIT(nullptr);
                    for (size_t i = 0; i < num_type_args; ++i)
                    {
                        ast_type_t* arg = vec_get(type->data.class.type_arguments, i);
                        ast_type_t* resolved = type_resolver_solve(ctx, arg, node);
                        if (resolved == ast_type_invalid())
                        {
                            vec_deinit(&resolved_args);
                            return ast_type_invalid();
                        }
                        vec_push(&resolved_args, resolved);
                    }

                    // Check if this is a template class
                    if (class_symb->kind == SYMBOL_TEMPLATE_CLASS)
                    {
                        // Validate type argument count
                        size_t expected = vec_size(&class_symb->data.template_class.type_parameters);
                        if (num_type_args != expected)
                        {
                            semantic_context_add_error(ctx, node, ssprintf(
                                "template '%s' expects %zu type argument%s, got %zu",
                                type->data.class.name, expected, expected == 1 ? "" : "s", num_type_args));
                            vec_deinit(&resolved_args);
                            return ast_type_invalid();
                        }

                        // Instantiate the template (this ensures the instance symbol is created and cached)
                        symbol_t* instance = instantiate_template_class(ctx, class_symb, &resolved_args);

                        if (instance == nullptr)
                        {
                            vec_deinit(&resolved_args);
                            return ast_type_invalid();
                        }

                        // Create the template instance type pointing to the instance symbol
                        ast_type_t* result = ast_type_template_instance(instance, &resolved_args);
                        vec_deinit(&resolved_args);
                        return result;
                    }
                    else
                    {
                        semantic_context_add_error(ctx, node, ssprintf("'%s' is not a template", type->data.class.name));
                        vec_deinit(&resolved_args);
                        return ast_type_invalid();
                    }
                }
                else
                {
                    // No type arguments, regular user type
                    if (class_symb->kind == SYMBOL_TEMPLATE_CLASS)
                    {
                        size_t expected = vec_size(&class_symb->data.template_class.type_parameters);
                        semantic_context_add_error(ctx, node, ssprintf(
                            "template '%s' expects %zu type argument%s",
                            type->data.class.name, expected, expected == 1 ? "" : "s"));
                        return ast_type_invalid();
                    }
                    return ast_type_user(class_symb);
                }
            }
            return type;
        default:
            break;
    }
    return type;
}
