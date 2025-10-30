#include "access_transformer.h"

#include "ast/expr/access_expr.h"
#include "ast/expr/member_access.h"
#include "ast/expr/ref_expr.h"
#include "ast/node.h"
#include "ast/type.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/util/ssprintf.h"
#include "sema/semantic_analyzer.h"
#include "sema/semantic_context.h"
#include "sema/symbol.h"
#include "sema/symbol_table.h"

#include <stdlib.h>
#include <string.h>

// FIXME: I feel like some of this functionality could be simplified greatly if semantic_analyzer would update
//        its lookup table when doing resolution inside class/namespace

static ast_expr_t* transform_ref_expr(semantic_analyzer_t* sema, ast_ref_expr_t* ref_expr, bool in_call_context,
    symbol_t** out_symbol, symbol_table_t** symbol_table)
{
    symbol_table_t* lookup_in = symbol_table_parent_with_symbol(*symbol_table, ref_expr->name);
    if (lookup_in == nullptr)
    {
        semantic_context_add_error(sema->ctx, ref_expr, ssprintf("no symbol '%s' exists in context", ref_expr->name));
        ref_expr->base.type = ast_type_invalid();
        return (ast_expr_t*)ref_expr;
    }

    // Overload resolution: first match
    vec_t* overloads = symbol_table_overloads(lookup_in, ref_expr->name);
    symbol_t* selected = nullptr;
    symbol_t* selected_ns = nullptr;  // prefer namespaces
    bool ambiguous = false;
    for (size_t i = 0; i < vec_size(overloads); ++i)
    {
        symbol_t* symbol = vec_get(overloads, i);

        switch (symbol->kind)
        {
            case SYMBOL_FUNCTION:
            case SYMBOL_METHOD:
                if (!in_call_context)
                    break;
                if (selected && selected->kind != SYMBOL_FUNCTION && selected->kind != SYMBOL_METHOD)
                    ambiguous = true;
                else
                    selected = symbol;  // for method/function overloads are OK and handled later
                break;
            case SYMBOL_NAMESPACE:
                selected_ns = symbol;
                break;
            case SYMBOL_VARIABLE:
            case SYMBOL_MEMBER:
            case SYMBOL_PARAMETER:
            {
                if (selected)
                    ambiguous = true;
                else
                    selected = symbol;
                break;
            }
            default:
                break;
        }
    }

    selected = selected_ns ? selected_ns : selected;

    if (ambiguous)
    {
        semantic_context_add_error(sema->ctx, ref_expr, ssprintf("ambiguous resolution of '%s",
            ref_expr->name));
        ref_expr->base.type = ast_type_invalid();
        return (ast_expr_t*)ref_expr;
    }

    if (selected == nullptr)
    {
        semantic_context_add_error(sema->ctx, ref_expr, ssprintf("no symbol '%s' is valid in context (%d candidates)",
            ref_expr->name, (int)vec_size(overloads)));
        ref_expr->base.type = ast_type_invalid();
        return (ast_expr_t*)ref_expr;
    }

    *out_symbol = selected;

    // Update symbol table if we are in a new scope
    if (selected->kind == SYMBOL_NAMESPACE)
    {
        *symbol_table = selected->data.namespace.exports;
        ref_expr->base.type = ast_type_builtin(TYPE_VOID);  // NOTE: this is kinda ugly; want to avoid invalid
    }
    else
    {
        // If type of symbol is instance of class or pointer to instance of class (auto-deref),
        // symbol table should now be the class's symbols
        ast_type_t* symb_type = (selected->kind == SYMBOL_METHOD || selected->kind == SYMBOL_FUNCTION) ?
            selected->data.function.return_type : selected->type;
        if (symb_type->kind == AST_TYPE_POINTER)
            symb_type = symb_type->data.pointer.pointee;

        if (symb_type->kind == AST_TYPE_CLASS)
        {
            symbol_t* class_symbol = symb_type->data.class.class_symbol;
            *symbol_table = class_symbol->data.class.symbols;
        }
        else
        {
            symbol_table_t* builtin_methods = semantic_context_builtin_methods_for_type(sema->ctx, symb_type);
            if (builtin_methods != nullptr)
                *symbol_table = builtin_methods;
        }

        ref_expr->base.type = symb_type;
    }

    ref_expr->resolved_symbol = selected;
    return (ast_expr_t*)ref_expr;
}

static ast_expr_t* transform_access_expr(semantic_analyzer_t* sema, ast_access_expr_t* access_expr,
    bool in_call_context, symbol_t** out_symbol, symbol_table_t** symbol_table)
{
    // First resolve outer part of outer-expr.inner-expr to get the correct symbol table
    if (AST_KIND(access_expr->outer) == AST_EXPR_REF)
    {
        // If outer is a ref-expr
        access_expr->outer = transform_ref_expr(sema, (ast_ref_expr_t*)access_expr->outer, in_call_context,
            out_symbol, symbol_table);
        if (access_expr->outer->type == ast_type_invalid())
        {
            access_expr->base.type = ast_type_invalid();
            return (ast_expr_t*)access_expr;
        }
    }
    else if (AST_KIND(access_expr->outer) == AST_EXPR_ACCESS)
    {
        // If outer is a nested access-expr
        ast_access_expr_t* outer = (ast_access_expr_t*)access_expr->outer;
        access_expr->outer = transform_access_expr(sema, outer, in_call_context, out_symbol, symbol_table);
        if (access_expr->outer->type == ast_type_invalid())
        {
            access_expr->base.type = ast_type_invalid();
            return (ast_expr_t*)access_expr;
        }
    }
    else
    {
        // For any other expression type (function call, array subscript, etc.),
        // we can't do symbol-table-based resolution. Just create a member_access node
        // directly and let analyze_member_access handle all the validation.
        panic_if(AST_KIND(access_expr->inner) != AST_EXPR_REF);
        ast_ref_expr_t* inner_ref = (ast_ref_expr_t*)access_expr->inner;

        ast_expr_t* replacement = ast_member_access_create(access_expr->outer, inner_ref->name);
        access_expr->outer = nullptr;
        ast_node_destroy(access_expr);
        return replacement;
    }

    // Resolve inner part
    panic_if(AST_KIND(access_expr->inner) != AST_EXPR_REF);  // inner part is always a ref-expr
    access_expr->inner = transform_ref_expr(sema, (ast_ref_expr_t*)access_expr->inner, in_call_context,
        out_symbol, symbol_table);
    if (access_expr->inner->type == ast_type_invalid())
    {
        access_expr->base.type = ast_type_invalid();
        return (ast_expr_t*)access_expr;
    }

    // Construct the correct type based on inner's symbol
    symbol_t* symb = *out_symbol;
    panic_if(symb == nullptr);  // should have resolved to invalid earlier
    switch (symb->kind)
    {
        case SYMBOL_NAMESPACE:
        {
            ast_expr_t* replacement = access_expr->inner;  // collapse chain to inner ref-expr
            access_expr->inner = nullptr;
            ast_node_destroy(access_expr);
            return replacement;
        }
        case SYMBOL_METHOD:
        {
            ast_expr_t* replacement = access_expr->outer;  // instance (method yielded as symbol)
            access_expr->outer = nullptr;
            ast_node_destroy(access_expr);
            return replacement;
        }
        case SYMBOL_MEMBER:
        {
            ast_ref_expr_t* inner_ref_expr = (ast_ref_expr_t*)access_expr->inner;
            ast_expr_t* replacement = ast_member_access_create(access_expr->outer,
                inner_ref_expr->name);
            replacement->type = inner_ref_expr->base.type;
            access_expr->outer = nullptr;
            ast_node_destroy(access_expr);
            return replacement;
        }
        case SYMBOL_FUNCTION:
        {
            ast_expr_t* replacement = access_expr->inner;
            access_expr->inner = nullptr;
            ast_node_destroy(access_expr);
            return replacement;
        }
        default:
            panic("Unhandled kind %d", symb->kind);
    }
}

ast_expr_t* access_transformer_resolve(semantic_analyzer_t* sema, ast_access_expr_t* access_expr,
    bool in_call_context, symbol_t** out_symbol)
{
    symbol_table_t* table = sema->ctx->current;
    symbol_t* out_symb = nullptr;
    ast_expr_t* out_expr = transform_access_expr(sema, access_expr, in_call_context, &out_symb, &table);

    if (in_call_context && out_symb && out_symb->kind != SYMBOL_METHOD && out_symb->kind != SYMBOL_FUNCTION)
    {
        semantic_context_add_error(sema->ctx, out_expr, ssprintf("symbol '%s' not callable", out_symb->name));
        out_expr->type = ast_type_invalid();
    }

    if (out_symbol != nullptr)
        *out_symbol = out_symb;

    return out_expr;
}
