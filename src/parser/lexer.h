#ifndef LEXER__H
#define LEXER__H

#include "ast/node.h"
#include "common/containers/string.h"
#include "common/containers/vec.h"
#include <stddef.h>

typedef enum
{
    // Keywords
    TOKEN_AS,
    TOKEN_BOOL,
    TOKEN_BREAK,
    TOKEN_CLASS,
    TOKEN_CONTINUE,
    TOKEN_ELSE,
    TOKEN_EXPORT,
    TOKEN_EXTERN,
    TOKEN_F32,
    TOKEN_F64,
    TOKEN_FALSE,
    TOKEN_FN,
    TOKEN_FOR,
    TOKEN_IF,
    TOKEN_IMPORT,
    TOKEN_I8,
    TOKEN_I16,
    TOKEN_I32,
    TOKEN_I64,
    TOKEN_ISIZE,
    TOKEN_NULL,
    TOKEN_RETURN,
    TOKEN_SELF,
    TOKEN_TRUE,
    TOKEN_U8,
    TOKEN_U16,
    TOKEN_U32,
    TOKEN_U64,
    TOKEN_USIZE,
    TOKEN_UNINIT,
    TOKEN_VAR,
    TOKEN_VIEW,
    TOKEN_VOID,
    TOKEN_WHILE,

    // Literals
    TOKEN_INTEGER,
    TOKEN_STRING,
    TOKEN_FLOAT,

    // Delimiters
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_SEMICOLON,
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_ARROW,
    TOKEN_DOT,
    TOKEN_DOTDOT,

    // Unary opeerators
    TOKEN_PLUSPLUS,
    TOKEN_MINUSMINUS,

    // Unary & Binary Operators
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_NOT,
    TOKEN_AMPERSAND,

    // Binary Operators
    TOKEN_DIV,
    TOKEN_MODULO,
    TOKEN_EQ,
    TOKEN_NEQ,
    TOKEN_LT,
    TOKEN_LTE,
    TOKEN_GT,
    TOKEN_GTE,
    TOKEN_ASSIGN,
    TOKEN_PLUS_ASSIGN,
    TOKEN_MINUS_ASSIGN,
    TOKEN_MUL_ASSIGN,
    TOKEN_DIV_ASSIGN,
    TOKEN_MODULO_ASSIGN,

    // Misc
    TOKEN_IDENTIFIER,
    TOKEN_EOF,
    TOKEN_UNKNOWN
} token_type_t;

typedef struct token
{
    token_type_t type;
    char* value;
    char* suffix;  // Defaults to "". Used for TOKEN_INTEGER and TOKEN_FLOAT
    int line;
    int column;
} token_t;

typedef void (*lexer_error_output_fn)(compiler_error_t* error, void* arg);

typedef struct lexer
{
    char* source;
    int line;
    int column;
    size_t pos;
    size_t length;
    vec_t peeked_tokens;
    int last_consumed_end_line;
    int last_consumed_end_column;
    char* filename;
    lexer_error_output_fn error_output;
    void* error_output_arg;
    vec_t created_tokens;
} lexer_t;

// If error_output is not nullptr, any error/warning that happens during lexing will be
// created as a compiler_error_t* and added to this vector.
lexer_t* lexer_create(const char* filename, const char* source, lexer_error_output_fn error_output,
    void* error_output_arg);

void lexer_destroy(lexer_t* lexer);

token_t* lexer_next_token(lexer_t* lexer);

void lexer_emit_token_malformed(lexer_t* lexer, token_t* tok, const char* description);

void lexer_emit_error_for_token(lexer_t* lexer, token_t* actual, token_type_t expected);

token_t* lexer_next_token_iff(lexer_t* lexer, token_type_t token_type);

token_t* lexer_peek_token(lexer_t* lexer);

token_t* lexer_peek_token_n(lexer_t* lexer, size_t n);

int token_type_get_precedence(token_type_t token_type);

bool token_type_is_bin_op(token_type_t token_type);

bool token_type_is_right_associative(token_type_t token_type);

bool token_type_is_assignment_op(token_type_t token_type);

bool token_type_is_arithmetic_op(token_type_t token_type);

bool token_type_is_relation_op(token_type_t token_type);

bool token_type_is_unary_op(token_type_t token_type);

string_t token_str(token_t* tok);

const char* token_type_str(token_type_t type);

void lexer_get_token_location(lexer_t* lexer, token_t* token, source_location_t* out);

void lexer_get_current_location(lexer_t* lexer, source_location_t* out);

#endif
