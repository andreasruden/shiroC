#ifndef LEXER__H
#define LEXER__H

#include <stddef.h>

typedef enum
{
    // Keywords
    TOKEN_INT,
    TOKEN_RETURN,

    // Literals
    TOKEN_NUMBER,
    TOKEN_STRING,

    // Delimiters
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_SEMICOLON,

    // Misc
    TOKEN_IDENTIFIER,
    TOKEN_EOF,
    TOKEN_UNKNOWN
} token_type_t;

typedef struct
{
    token_type_t type;
    char* value;
    int line;
    int column;
} token_t;

typedef struct lexer
{
    const char* source;
    size_t pos;
    size_t length;
    int line;
    int column;
    token_t* peeked_token;
} lexer_t;

lexer_t* lexer_create(const char* source);

void lexer_destroy(lexer_t* lexer);

token_t* lexer_next_token(lexer_t* lexer);

token_t* lexer_next_token_iff(lexer_t* lexer, token_type_t token_type);

token_t* lexer_peek_token(lexer_t* lexer);

bool lexer_consume_token(lexer_t* lexer, token_type_t token_type);

void token_destroy(token_t* tok);

#endif
