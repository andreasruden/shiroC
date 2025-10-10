#include "lexer.h"

#include "ast/node.h"
#include "common/containers/vec.h"
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
    {"bool", TOKEN_BOOL},
    {"else", TOKEN_ELSE},
    {"f32", TOKEN_F32},
    {"f64", TOKEN_F64},
    {"fn", TOKEN_FN},
    {"if", TOKEN_IF},
    {"i8", TOKEN_I8},
    {"i16", TOKEN_I16},
    {"i32", TOKEN_I32},
    {"i64", TOKEN_I64},
    {"return", TOKEN_RETURN},
    {"u8", TOKEN_U8},
    {"u16", TOKEN_U16},
    {"u32", TOKEN_U32},
    {"u64", TOKEN_U64},
    {"var", TOKEN_VAR},
    {"void", TOKEN_VOID},
    {"while", TOKEN_WHILE},
    {nullptr, TOKEN_UNKNOWN}
};

const char* token_type_str(token_type_t type)
{
    switch (type)
    {
        case TOKEN_BOOL: return "bool";
        case TOKEN_VOID: return "void";
        case TOKEN_I8: return "i8";
        case TOKEN_I16: return "i16";
        case TOKEN_I32: return "i32";
        case TOKEN_I64: return "i64";
        case TOKEN_U8: return "u8";
        case TOKEN_U16: return "u16";
        case TOKEN_U32: return "u32";
        case TOKEN_U64: return "u64";
        case TOKEN_RETURN: return "return";
        case TOKEN_IDENTIFIER: return "identifier";
        case TOKEN_NUMBER: return "number";
        case TOKEN_STRING: return "string-literal";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_SEMICOLON: return ";";
        case TOKEN_STAR: return "*";
        case TOKEN_DIV: return "/";
        case TOKEN_MODULO: return "%";
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_LT: return "<";
        case TOKEN_LTE: return "<=";
        case TOKEN_GT: return ">";
        case TOKEN_GTE: return ">=";
        case TOKEN_EQ: return "==";
        case TOKEN_NEQ: return "!=";
        case TOKEN_ASSIGN: return "=";
        case TOKEN_NOT: return "!";
        case TOKEN_ELSE: return "else";
        case TOKEN_FN: return "else";
        case TOKEN_IF: return "if";
        case TOKEN_VAR: return "var";
        case TOKEN_WHILE: return "while";
        case TOKEN_COLON: return ":";
        case TOKEN_COMMA: return ",";
        case TOKEN_ARROW: return "->";
        case TOKEN_PLUS_ASSIGN: return "+=";
        case TOKEN_MINUS_ASSIGN: return "-=";
        case TOKEN_MUL_ASSIGN: return "*=";
        case TOKEN_DIV_ASSIGN: return "/=";
        case TOKEN_MODULO_ASSIGN: return "%=";
        case TOKEN_F32: return "f32";
        case TOKEN_F64: return "f64";
        case TOKEN_UNKNOWN: return "UNKNOWN";
        case TOKEN_EOF: return "EOF";
    }
    return "UNHANDLED";
}

static char lexer_peek(lexer_t* lexer)
{
    if (lexer->pos >= lexer->length)
        return '\0';
    return lexer->source[lexer->pos];
}

static char lexer_advance(lexer_t* lexer)
{
    if (lexer->pos >= lexer->length)
        return '\0';

    char c = lexer->source[lexer->pos++];
    if (c == '\n')
    {
        ++lexer->line;
        lexer->column = 1;
    }
    else
        ++lexer->column;

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

    vec_push(&lexer->created_tokens, tok);

    return tok;
}

static void token_destroy(void* tok_)
{
    token_t* tok = tok_;
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

lexer_t* lexer_create(const char* filename, const char* source, lexer_error_output_fn error_output,
    void* error_output_arg)
{
    lexer_t* lexer = malloc(sizeof(*lexer));

    *lexer = (lexer_t){
        .source = strdup(source),
        .length = strlen(source),
        .filename = strdup(filename),
        .line = 1,
        .column = 1,
        .error_output = error_output,
        .error_output_arg = error_output_arg,
        .created_tokens = VEC_INIT(token_destroy),
    };

    return lexer;
}

void lexer_destroy(lexer_t *lexer)
{
    if (lexer != nullptr)
    {
        vec_deinit(&lexer->created_tokens);
        free(lexer->source);
        free(lexer->filename);
        free(lexer);
    }
}

static token_t* lex_symbol(lexer_t* lexer)
{
    const char c = lexer_peek(lexer);
    const int line = lexer->line;
    const int col = lexer->column;
    lexer_advance(lexer);

    switch (c)
    {
        case '(': return token_create(lexer, TOKEN_LPAREN, "(", line, col);
        case ')': return token_create(lexer, TOKEN_RPAREN, ")", line, col);
        case '{': return token_create(lexer, TOKEN_LBRACE, "{", line, col);
        case '}': return token_create(lexer, TOKEN_RBRACE, "}", line, col);
        case ';': return token_create(lexer, TOKEN_SEMICOLON, ";", line, col);
        case ':': return token_create(lexer, TOKEN_COLON, ":", line, col);
        case ',': return token_create(lexer, TOKEN_COMMA, ",", line, col);

        case '+':
            if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                return token_create(lexer, TOKEN_PLUS_ASSIGN, "+=", line, col);
            }
            return token_create(lexer, TOKEN_PLUS, "+", line, col);

        case '*':
            if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                return token_create(lexer, TOKEN_MUL_ASSIGN, "*=", line, col);
            }
            return token_create(lexer, TOKEN_STAR, "*", line, col);

        case '/':;
            if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                return token_create(lexer, TOKEN_DIV_ASSIGN, "/=", line, col);
            }
            return token_create(lexer, TOKEN_DIV, "/", line, col);

        case '%':
            if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                return token_create(lexer, TOKEN_MODULO_ASSIGN, "%=", line, col);
            }
            return token_create(lexer, TOKEN_MODULO, "%", line, col);

        case '-':
            if (lexer_peek(lexer) == '>') {
                lexer_advance(lexer);
                return token_create(lexer, TOKEN_ARROW, "->", line, col);
            }
            else if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                return token_create(lexer, TOKEN_MINUS_ASSIGN, "-=", line, col);
            }
            return token_create(lexer, TOKEN_MINUS, "-", line, col);

        case '=':
            if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                return token_create(lexer, TOKEN_EQ, "==", line, col);
            }
            return token_create(lexer, TOKEN_ASSIGN, "=", line, col);

        case '!':
            if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                return token_create(lexer, TOKEN_NEQ, "!=", line, col);
            }
            return token_create(lexer, TOKEN_NOT, "!", line, col);

        case '<':
            if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                return token_create(lexer, TOKEN_LTE, "<=", line, col);
            }
            return token_create(lexer, TOKEN_LT, "<", line, col);

        case '>':
            if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                return token_create(lexer, TOKEN_GTE, ">=", line, col);
            }
            return token_create(lexer, TOKEN_GT, ">", line, col);

        default:
        {
            char val[2] = {c, '\0'};
            return token_create(lexer, TOKEN_UNKNOWN, val, line, col);
        }
    }
}

static token_t* lex_next_token(lexer_t* lexer)
{
    lexer_skip_whitespace(lexer);

    if (lexer->pos >= lexer->length)
        return token_create(lexer, TOKEN_EOF, NULL, lexer->line, lexer->column);

    const char c = lexer_peek(lexer);

    if (isalpha(c) || c == '_')
        return lex_identifier(lexer);

    if (isdigit(c))
        return lex_number(lexer);

    if (c == '"')
        return lex_string(lexer);

    return lex_symbol(lexer);
}

token_t* lexer_next_token(lexer_t* lexer)
{
    token_t* tok;
    if (lexer->peeked_token == nullptr)
    {
        tok = lex_next_token(lexer);
    }
    else
    {
        tok = lexer->peeked_token;
        lexer->peeked_token = nullptr;
    }

    lexer->last_consumed_end_line = lexer->line;
    lexer->last_consumed_end_column = lexer->column;
    return tok;
}

token_t* lexer_peek_token(lexer_t* lexer)
{
    if (lexer->peeked_token != nullptr)
        return lexer->peeked_token;

    lexer->peeked_token = lex_next_token(lexer);
    return lexer->peeked_token;
}

void lexer_emit_error_for_token(lexer_t* lexer, token_t* actual, token_type_t expected)
{
    const int line = lexer->last_consumed_end_line;
    const int column = lexer->last_consumed_end_column;
    if (lexer->error_output == nullptr)
    {
        printf("Error: Expected token %s, but found %s in File %s at Line %d, Col %d\n", token_type_str(expected),
            token_type_str(actual->type), lexer->filename, line, column);
    }
    else
    {
        compiler_error_t* error = compiler_error_create_for_source(false,
            ssprintf("expected '%s'", token_type_str(expected)), lexer->filename, line, column);
        lexer->error_output(error, lexer->error_output_arg);
    }
}

token_t* lexer_next_token_iff(lexer_t* lexer, token_type_t token_type)
{
    token_t* next_tok = lexer_peek_token(lexer);
    if (next_tok->type == token_type)
        return lexer_next_token(lexer);

    lexer_emit_error_for_token(lexer, next_tok, token_type);

    return nullptr;
}

int token_type_get_precedence(token_type_t token_type)
{
    switch (token_type)
    {
        case TOKEN_STAR:
        case TOKEN_DIV:
        case TOKEN_MODULO:
            return 5;

        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return 4;

        case TOKEN_LT:
        case TOKEN_LTE:
        case TOKEN_GT:
        case TOKEN_GTE:
            return 3;

        case TOKEN_EQ:
        case TOKEN_NEQ:
            return 2;

        // assignment is right-associative
        case TOKEN_ASSIGN:
        case TOKEN_PLUS_ASSIGN:
        case TOKEN_MINUS_ASSIGN:
        case TOKEN_MUL_ASSIGN:
        case TOKEN_DIV_ASSIGN:
        case TOKEN_MODULO_ASSIGN:
            return 1;

        default:
            return 0;
    }
}

bool token_type_is_bin_op(token_type_t token_type)
{
    switch (token_type)
    {
        case TOKEN_STAR:
        case TOKEN_DIV:
        case TOKEN_MODULO:
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_LT:
        case TOKEN_LTE:
        case TOKEN_GT:
        case TOKEN_GTE:
        case TOKEN_EQ:
        case TOKEN_NEQ:
        case TOKEN_ASSIGN:
        case TOKEN_PLUS_ASSIGN:
        case TOKEN_MINUS_ASSIGN:
        case TOKEN_MUL_ASSIGN:
        case TOKEN_DIV_ASSIGN:
        case TOKEN_MODULO_ASSIGN:
            return true;
        default:
            return false;
    }
}

bool token_type_is_right_associative(token_type_t token_type)
{
    switch (token_type)
    {
        case TOKEN_ASSIGN:
        case TOKEN_PLUS_ASSIGN:
        case TOKEN_MINUS_ASSIGN:
        case TOKEN_MUL_ASSIGN:
        case TOKEN_DIV_ASSIGN:
        case TOKEN_MODULO_ASSIGN:
            return true;
        default:
            break;
    }
    return false;
}

bool token_type_is_assignment_op(token_type_t token_type)
{
    switch (token_type)
    {
        case TOKEN_ASSIGN:
        case TOKEN_PLUS_ASSIGN:
        case TOKEN_MINUS_ASSIGN:
        case TOKEN_MUL_ASSIGN:
        case TOKEN_DIV_ASSIGN:
        case TOKEN_MODULO_ASSIGN:
            return true;
        default:
            break;
    }
    return false;
}

bool token_type_is_arithmetic_op(token_type_t token_type)
{
    switch (token_type)
    {
        case TOKEN_STAR:
        case TOKEN_DIV:
        case TOKEN_MODULO:
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return true;
        default:
            break;
    }
    return false;
}

bool token_type_is_relation_op(token_type_t token_type)
{
    switch (token_type)
    {
        case TOKEN_LT:
        case TOKEN_LTE:
        case TOKEN_GT:
        case TOKEN_GTE:
        case TOKEN_EQ:
        case TOKEN_NEQ:
            return true;
        default:
            break;
    }
    return false;
}

void lexer_get_token_location(lexer_t* lexer, token_t* token, source_location_t* out)
{
    set_source_location(out, lexer->filename, token->line, token->column);
}

void lexer_get_current_location(lexer_t* lexer, source_location_t* out)
{
    set_source_location(out, lexer->filename, lexer->last_consumed_end_line, lexer->last_consumed_end_column);
}
