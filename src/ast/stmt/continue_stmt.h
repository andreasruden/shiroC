#ifndef AST_STMT_CONTINUE_STMT__H
#define AST_STMT_CONTINUE_STMT__H

#include "stmt.h"

typedef struct ast_continue_stmt
{
    ast_stmt_t base;
} ast_continue_stmt_t;

ast_stmt_t* ast_continue_stmt_create(void);

#endif
