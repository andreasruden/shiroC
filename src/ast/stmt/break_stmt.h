#ifndef AST_STMT_BREAK_STMT__H
#define AST_STMT_BREAK_STMT__H

#include "stmt.h"

typedef struct ast_break_stmt
{
    ast_stmt_t base;
} ast_break_stmt_t;

ast_stmt_t* ast_break_stmt_create(void);

#endif
