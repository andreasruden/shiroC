#ifndef LEXER__H
#define LEXER__H

#include "ast/node.h"
#include "common/containers/ptr_vec.h"
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
    TOKEN_COMMA,

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
    char* source;
    int line;
    int column;
    size_t pos;
    size_t length;
    token_t* peeked_token;
    char* filename;
    ptr_vec_t* error_output;
    ast_node_t* ast_node;
    ptr_vec_t created_tokens;
} lexer_t;

// If error_output is not nullptr, any error/warning that happens during lexing will be
// created as a compiler_error_t* and added to this vector.
lexer_t* lexer_create(const char* filename, const char* source, ptr_vec_t* error_output);

void lexer_destroy(lexer_t* lexer);

token_t* lexer_next_token(lexer_t* lexer);

token_t* lexer_next_token_iff(lexer_t* lexer, token_type_t token_type);

token_t* lexer_peek_token(lexer_t* lexer);

bool lexer_consume_token(lexer_t* lexer, token_type_t token_type);

// Same as calling lexer_consume_token between setting and unsetting the ast_node.
bool lexer_consume_token_for_node(lexer_t* lexer, token_type_t token_type, void* ast_node);

// When a lexer error is encountered, the last set ast_node will be automatically appended
// to that error.
void lexer_set_ast_node(lexer_t* lexer, void* ast_node);

#endif
