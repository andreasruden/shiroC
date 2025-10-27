#ifndef AST_UTIL_CLONER__H
#define AST_UTIL_CLONER__H

#include "ast/expr/expr.h"

typedef struct ast_fn_def ast_fn_def_t;
typedef struct ast_class_def ast_class_def_t;
typedef struct ast_stmt ast_stmt_t;
typedef struct ast_decl ast_decl_t;

// Deep clone an expression node (returns ownership)
ast_expr_t* ast_expr_clone(ast_expr_t* expr);

// Deep clone a function definition (returns ownership)
ast_fn_def_t* ast_fn_def_clone(ast_fn_def_t* fn);

// Deep clone a class definition (returns ownership)
ast_class_def_t* ast_class_def_clone(ast_class_def_t* class_def);

// Deep clone a statement (returns ownership)
ast_stmt_t* ast_stmt_clone(ast_stmt_t* stmt);

// Deep clone a declaration (returns ownership)
ast_decl_t* ast_decl_clone(ast_decl_t* decl);

#endif
