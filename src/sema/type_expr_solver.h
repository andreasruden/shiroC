#ifndef SEMA_TYPE_EXPR_SOLVER__H
#define SEMA_TYPE_EXPR_SOLVER__H

#include "ast/type.h"
#include "sema/semantic_context.h"

// The parser leaves potentially unresolved expressions needed to be evaluated to construct a type fully.
// This resolve those, returning the resolved type. If an error is encounted, the returned type is
// ast_type_invalid() and errors are added to the ctx.
ast_type_t* type_expr_solver_solve(semantic_context_t* ctx, ast_type_t*, void* node);

#endif
