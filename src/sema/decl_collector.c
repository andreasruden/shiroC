#include "decl_collector.h"

#include "ast/decl/param_decl.h"
#include "ast/def/fn_def.h"
#include "ast/visitor.h"
#include "common/containers/vec.h"
#include "common/util/ssprintf.h"
#include "sema/semantic_context.h"
#include "sema/symbol.h"
#include "sema/symbol_table.h"

struct decl_collector
{
    ast_visitor_t base;
    semantic_context_t* ctx;  // decl_collector does not own ctx
};

void collect_fn_def(void* self_, ast_fn_def_t* fn_def, void* out_)
{
    (void)out_;
    decl_collector_t* collector = self_;

    symbol_t* prev_symbol;
    if ((prev_symbol = symbol_table_lookup(collector->ctx->global, fn_def->base.name)) != nullptr)
    {
        semantic_context_add_error(collector->ctx, fn_def,
            ssprintf("redeclaration of '%s', previously from <%s:%d>", fn_def->base.name,
                prev_symbol->ast->source_begin.filename, prev_symbol->ast->source_begin.line));
        return;
    }

    symbol_t* symbol = symbol_create(fn_def->base.name, SYMBOL_FUNCTION, fn_def);
    symbol->type = fn_def->return_type;
    for (size_t i = 0; i < vec_size(&fn_def->params); ++i)
        ast_visitor_visit(collector, vec_get(&fn_def->params, i), symbol);

    symbol_table_insert(collector->ctx->current, symbol);
}

void collect_param_decl(void* self_, ast_param_decl_t* param_decl, void* out_)
{
    (void)self_;
    symbol_t* fn = out_;

    vec_push(&fn->data.function.parameters, param_decl);
}

decl_collector_t* decl_collector_create(semantic_context_t* ctx)
{
    decl_collector_t* collector = malloc(sizeof(*collector));

    *collector = (decl_collector_t){
        .ctx = ctx,
    };

    ast_visitor_init(&collector->base);
    collector->base.visit_fn_def = collect_fn_def;
    collector->base.visit_param_decl = collect_param_decl;

    return collector;
}

void decl_collector_destroy(decl_collector_t* collector)
{
    if (collector == nullptr)
        return;

    free(collector);
}

bool decl_collector_run(decl_collector_t* collector, ast_node_t* root)
{
    size_t errors = vec_size(&collector->ctx->error_nodes);
    ast_visitor_visit(collector, root, nullptr);
    return errors == vec_size(&collector->ctx->error_nodes);  // no new errors
}
