#ifndef SEMA_TYPE_EXPR_SOLVER__H
#define SEMA_TYPE_EXPR_SOLVER__H

#include "ast/type.h"
#include "sema/semantic_context.h"

// Some types are unresolved by the parser, and need to be resolved with more context.
// This resolve those, returning the resolved type. If an error is encounted, the returned type is
// ast_type_invalid() and errors are added to the ctx.
ast_type_t* type_resolver_solve(semantic_context_t* ctx, ast_type_t*, void* node);

#endif
