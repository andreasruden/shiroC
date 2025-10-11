#ifndef AST_EXPR_INT_LIT__H
#define AST_EXPR_INT_LIT__H

#include "ast/expr/expr.h"

#include <stdint.h>

typedef struct ast_int_lit
{
    ast_expr_t base;
    bool has_minus_sign;
    union
    {
        int64_t as_signed;    // set by SEMA if type is signed
        uint64_t as_unsigned; // set by SEMA if type is unsigned
        uint64_t magnitude;   // always positive, set by parser
    } value;
    char* suffix;            // "" if not present
} ast_int_lit_t;

ast_expr_t* ast_int_lit_create(bool has_minus_sign, uint64_t mangitude, const char* suffix);

ast_expr_t* ast_int_lit_create_unsigned(uint64_t, const char* suffix);

ast_expr_t* ast_int_lit_val(int64_t value);

#endif
