#include "lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    const char* keyword;
    token_type_t type;
} keyword_t;

static keyword_t lexer_keywords[] =
{
    {"int", TOKEN_INT},
    {"return", TOKEN_RETURN},
    {NULL, TOKEN_UNKNOWN}
};

static const char* token_type_str(token_type_t type)
{
    switch (type)
    {
        case TOKEN_INT: return "INT";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_EOF: return "EOF";
        default: return "UNKNOWN";
    }
}

static char lexer_peek(lexer_t* lexer)
{
    if (lexer->pos >= lexer->length)
        return '\0';
    return lexer->source[lexer->pos];
}

static char lexer_advance(lexer_t* lex)
{
    if (lex->pos >= lex->length)
        return '\0';

    char c = lex->source[lex->pos++];
    if (c == '\n')
    {
        ++lex->line;
        lex->column = 1;
    }
    else
        ++lex->column;

    return c;
}

static void lexer_skip_whitespace(lexer_t* lex)
{
    while (isspace(lexer_peek(lex)))
        lexer_advance(lex);
}

static token_t* token_create(token_type_t type, const char* value, int line, int col)
{
    token_t* tok = malloc(sizeof(*tok));
    tok->type = type;
    tok->value = value ? strdup(value) : NULL;
    tok->line = line;
    tok->column = col;
    return tok;
}

void token_destroy(token_t* tok)
{
    if (tok != nullptr)
    {
        free(tok->value);
        free(tok);
    }
}

static token_type_t lookup_keyword(const char* id)
{
    for (int i = 0; lexer_keywords[i].keyword != NULL; ++i)
    {
        if (strcmp(id, lexer_keywords[i].keyword) == 0)
            return lexer_keywords[i].type;
    }

    return TOKEN_IDENTIFIER;
}

static token_t* lex_identifier(lexer_t* lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->column;
    size_t start = lexer->pos;

    while (isalnum(lexer_peek(lexer)) || lexer_peek(lexer) == '_')
    {
        lexer_advance(lexer);
    }

    size_t length = lexer->pos - start;
    char* value = malloc(length + 1);
    strncpy(value, lexer->source + start, length);
    value[length] = '\0';

    token_type_t type = lookup_keyword(value);
    token_t* tok = token_create(type, value, start_line, start_col);
    free(value);
    return tok;
}

static token_t* lex_number(lexer_t* lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->column;
    size_t start = lexer->pos;

    while (isdigit(lexer_peek(lexer)))
        lexer_advance(lexer);

    size_t length = lexer->pos - start;
    char* value = malloc(length + 1);
    strncpy(value, lexer->source + start, length);
    value[length] = '\0';

    token_t* tok = token_create(TOKEN_NUMBER, value, start_line, start_col);
    free(value);
    return tok;
}

static token_t* lex_string(lexer_t* lexer)
{
    int start_line = lexer->line;
    int start_col = lexer->column;
    lexer_advance(lexer); // Skip opening quote
    size_t start = lexer->pos;

    while (lexer_peek(lexer) != '"' && lexer_peek(lexer) != '\0')
    {
        if (lexer_peek(lexer) == '\\')
            lexer_advance(lexer);
        lexer_advance(lexer);
    }

    size_t length = lexer->pos - start;
    char* value = malloc(length + 1);
    strncpy(value, lexer->source + start, length);
    value[length] = '\0';

    lexer_advance(lexer); // Skip closing quote

    token_t* tok = token_create(TOKEN_STRING, value, start_line, start_col);
    free(value);
    return tok;
}

lexer_t* lexer_create(const char* source)
{
    lexer_t* lexer = malloc(sizeof(*lexer));

    *lexer = (lexer_t){
        .source = source ? strdup(source) : nullptr,
        .length = source ? strlen(source) : 0,
        .line = 1,
        .column = 1,
    };

    return lexer;
}

void lexer_set_source(lexer_t* lexer, const char* source)
{
    free(lexer->source);
    lexer->source = strdup(source);
    lexer->length = strlen(source);
    lexer->line = 1;
    lexer->column = 1;
    lexer->pos = 0;
    lexer->peeked_token = nullptr;
}

void lexer_destroy(lexer_t *lexer)
{
    if (lexer != nullptr)
    {
        free(lexer->source);
        free(lexer);
    }
}

token_t* lexer_next_token(lexer_t* lexer)
{
    if (lexer->peeked_token != nullptr)
    {
        token_t* tok = lexer->peeked_token;
        lexer->peeked_token = nullptr;
        return tok;
    }

    lexer_skip_whitespace(lexer);

    if (lexer->pos >= lexer->length)
        return token_create(TOKEN_EOF, NULL, lexer->line, lexer->column);

    char c = lexer_peek(lexer);
    int line = lexer->line;
    int col = lexer->column;

    if (isalpha(c) || c == '_')
        return lex_identifier(lexer);

    if (isdigit(c))
        return lex_number(lexer);

    if (c == '"')
        return lex_string(lexer);

    lexer_advance(lexer);
    switch (c)
    {
        case '(': return token_create(TOKEN_LPAREN, "(", line, col);
        case ')': return token_create(TOKEN_RPAREN, ")", line, col);
        case '{': return token_create(TOKEN_LBRACE, "{", line, col);
        case '}': return token_create(TOKEN_RBRACE, "}", line, col);
        case ';': return token_create(TOKEN_SEMICOLON, ";", line, col);
        default:
        {
            char val[2] = {c, '\0'};
            return token_create(TOKEN_UNKNOWN, val, line, col);
        }
    }
}

token_t* lexer_next_token_iff(lexer_t* lexer, token_type_t token_type)
{
    token_t* token = lexer_next_token(lexer);
    if (token->type == token_type)
        return token;

    printf("Error: Expected token %s, but found %s at Line %d, Col %d\n", token_type_str(token_type),
        token_type_str(token->type), token->line, token->column);
    free(token);
    return nullptr;
}

bool lexer_consume_token(lexer_t* lexer, token_type_t token_type)
{
    token_t* token = lexer_next_token_iff(lexer, token_type);
    token_destroy(token);
    return token != nullptr;
}

token_t* lexer_peek_token(lexer_t* lexer)
{
    if (lexer->peeked_token != nullptr)
        return lexer->peeked_token;

    lexer->peeked_token = lexer_next_token(lexer);
    return lexer->peeked_token;
}
