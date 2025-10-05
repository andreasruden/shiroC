#include "lexer.h"

#include "common/containers/ptr_vec.h"
#include "compiler_error.h"
#include "common/util/ssprintf.h"

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
        case TOKEN_INT: return "int";
        case TOKEN_RETURN: return "return";
        case TOKEN_IDENTIFIER: return "identifier";
        case TOKEN_NUMBER: return "number";
        case TOKEN_STRING: return "string-literal";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_SEMICOLON: return ";";
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

static token_t* token_create(lexer_t* lexer, token_type_t type, const char* value, int line, int col)
{
    token_t* tok = malloc(sizeof(*tok));

    *tok = (token_t){
        .type = type,
        .value = value ? strdup(value) : nullptr,
        .line = line,
        .column = col,
    };

    ptr_vec_append(&lexer->created_tokens, tok);

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
        lexer_advance(lexer);

    size_t length = lexer->pos - start;
    char* value = malloc(length + 1);
    strncpy(value, lexer->source + start, length);
    value[length] = '\0';

    token_type_t type = lookup_keyword(value);
    token_t* tok = token_create(lexer, type, value, start_line, start_col);
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

    token_t* tok = token_create(lexer, TOKEN_NUMBER, value, start_line, start_col);
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

    token_t* tok = token_create(lexer, TOKEN_STRING, value, start_line, start_col);
    free(value);
    return tok;
}

lexer_t* lexer_create(const char* filename, const char* source, ptr_vec_t* error_output)
{
    lexer_t* lexer = malloc(sizeof(*lexer));

    *lexer = (lexer_t){
        .source = strdup(source),
        .length = strlen(source),
        .filename = strdup(filename),
        .line = 1,
        .column = 1,
        .error_output = error_output,
        .created_tokens = PTR_VEC_INIT,
    };

    return lexer;
}

void lexer_destroy(lexer_t *lexer)
{
    if (lexer != nullptr)
    {
        ptr_vec_deinit(&lexer->created_tokens); // FIXME: needs to call token_destroy for each
        free(lexer->source);
        free(lexer->filename);
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
        return token_create(lexer, TOKEN_EOF, NULL, lexer->line, lexer->column);

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
        case '(': return token_create(lexer, TOKEN_LPAREN, "(", line, col);
        case ')': return token_create(lexer, TOKEN_RPAREN, ")", line, col);
        case '{': return token_create(lexer, TOKEN_LBRACE, "{", line, col);
        case '}': return token_create(lexer, TOKEN_RBRACE, "}", line, col);
        case ';': return token_create(lexer, TOKEN_SEMICOLON, ";", line, col);
        case ',': return token_create(lexer, TOKEN_COMMA, ",", line, col);
        default:
        {
            char val[2] = {c, '\0'};
            return token_create(lexer, TOKEN_UNKNOWN, val, line, col);
        }
    }
}

token_t* lexer_next_token_iff(lexer_t* lexer, token_type_t token_type)
{
    const int line = lexer->line;
    const int column = lexer->column;

    const token_type_t next_tok_type = lexer_peek_token(lexer)->type;
    if (next_tok_type == token_type)
        return lexer_next_token(lexer);

    if (lexer->error_output == nullptr)
    {
        printf("Error: Expected token %s, but found %s in File %s at Line %d, Col %d\n", token_type_str(token_type),
            token_type_str(next_tok_type), lexer->filename, line, column);
    }
    else
    {
        compiler_error_t* error = compiler_error_create(false, lexer->ast_node,
            ssprintf("expected '%s'", token_type_str(token_type)), lexer->filename, line, column);
        ptr_vec_append(lexer->error_output, error);
    }

    return nullptr;
}

bool lexer_consume_token(lexer_t* lexer, token_type_t token_type)
{
    token_t* token = lexer_next_token_iff(lexer, token_type);
    token_destroy(token);
    return token != nullptr;
}

bool lexer_consume_token_for_node(lexer_t* lexer, token_type_t token_type, void* ast_node)
{
    lexer_set_ast_node(lexer, ast_node);
    const bool res = lexer_consume_token(lexer, token_type);
    lexer_set_ast_node(lexer, nullptr);
    return res;
}

token_t* lexer_peek_token(lexer_t* lexer)
{
    if (lexer->peeked_token != nullptr)
        return lexer->peeked_token;

    lexer->peeked_token = lexer_next_token(lexer);
    return lexer->peeked_token;
}

void lexer_set_ast_node(lexer_t* lexer, void* ast_node)
{
    lexer->ast_node = ast_node;
}
