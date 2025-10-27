#include "lexer.h"

#include "ast/node.h"
#include "common/containers/string.h"
#include "common/containers/vec.h"
#include "compiler_error.h"
#include "common/util/ssprintf.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static constexpr char EMPTY_SUFFIX[] = "";

typedef struct
{
    const char* keyword;
    token_type_t type;
} keyword_t;

static keyword_t lexer_keywords[] =
{
    {"as", TOKEN_AS},
    {"bool", TOKEN_BOOL},
    {"break", TOKEN_BREAK},
    {"class", TOKEN_CLASS},
    {"continue", TOKEN_CONTINUE},
    {"else", TOKEN_ELSE},
    {"export", TOKEN_EXPORT},
    {"extern", TOKEN_EXTERN},
    {"f32", TOKEN_F32},
    {"f64", TOKEN_F64},
    {"false", TOKEN_FALSE},
    {"fn", TOKEN_FN},
    {"for", TOKEN_FOR},
    {"if", TOKEN_IF},
    {"import", TOKEN_IMPORT},
    {"i8", TOKEN_I8},
    {"i16", TOKEN_I16},
    {"i32", TOKEN_I32},
    {"i64", TOKEN_I64},
    {"isize", TOKEN_ISIZE},
    {"null", TOKEN_NULL},
    {"return", TOKEN_RETURN},
    {"self", TOKEN_SELF},
    {"string", TOKEN_STRING},
    {"true", TOKEN_TRUE},
    {"u8", TOKEN_U8},
    {"u16", TOKEN_U16},
    {"u32", TOKEN_U32},
    {"u64", TOKEN_U64},
    {"usize", TOKEN_USIZE},
    {"uninit", TOKEN_UNINIT},
    {"var", TOKEN_VAR},
    {"view", TOKEN_VIEW},
    {"void", TOKEN_VOID},
    {"while", TOKEN_WHILE},
    {nullptr, TOKEN_UNKNOWN}
};

static void emit_error(lexer_t* lexer, const char* description, int line, int col)
{
    if (lexer->error_output == nullptr)
    {
        printf("Error: %s at line %d, col %d\n", description, line, col);
    }
    else
    {
        compiler_error_t* error = compiler_error_create_for_source(false,
            description, lexer->filename, line, col);
        lexer->error_output(error, lexer->error_output_arg);
    }
}

string_t token_str(token_t* tok)
{
    string_t out = STRING_INIT;
    string_append_cstr(&out, token_type_str(tok->type));
    switch (tok->type)
    {
        case TOKEN_IDENTIFIER:
        case TOKEN_STRING_LIT:
        case TOKEN_INTEGER:
        case TOKEN_FLOAT:
            string_append_cstr(&out, ssprintf(" (%s)", tok->value));
            break;
        default:
            break;
    }
    return out;
}

const char* token_type_str(token_type_t type)
{
    switch (type)
    {
        case TOKEN_AT: return "@";
        case TOKEN_STRING: return "string";
        case TOKEN_AS: return "as";
        case TOKEN_EXTERN: return "extern";
        case TOKEN_EXPORT: return "export";
        case TOKEN_IMPORT: return "import";
        case TOKEN_PLUSPLUS: return "++";
        case TOKEN_MINUSMINUS: return "--";
        case TOKEN_SELF: return "self";
        case TOKEN_CLASS: return "class";
        case TOKEN_UNINIT: return "uninit";
        case TOKEN_VIEW: return "view";
        case TOKEN_LBRACKET: return "[";
        case TOKEN_RBRACKET: return "]";
        case TOKEN_AMPERSAND: return "&";
        case TOKEN_NULL: return "null";
        case TOKEN_FALSE: return "false";
        case TOKEN_TRUE: return "true";
        case TOKEN_FLOAT: return "float";
        case TOKEN_BOOL: return "bool";
        case TOKEN_BREAK: return "break";
        case TOKEN_CONTINUE: return "continue";
        case TOKEN_VOID: return "void";
        case TOKEN_I8: return "i8";
        case TOKEN_I16: return "i16";
        case TOKEN_I32: return "i32";
        case TOKEN_I64: return "i64";
        case TOKEN_ISIZE: return "isize";
        case TOKEN_U8: return "u8";
        case TOKEN_U16: return "u16";
        case TOKEN_U32: return "u32";
        case TOKEN_U64: return "u64";
        case TOKEN_USIZE: return "usize";
        case TOKEN_RETURN: return "return";
        case TOKEN_IDENTIFIER: return "identifier";
        case TOKEN_INTEGER: return "number";
        case TOKEN_STRING_LIT: return "string-literal";
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
        case TOKEN_FOR: return "for";
        case TOKEN_IF: return "if";
        case TOKEN_VAR: return "var";
        case TOKEN_WHILE: return "while";
        case TOKEN_COLON: return ":";
        case TOKEN_COMMA: return ",";
        case TOKEN_ARROW: return "->";
        case TOKEN_DOT: return ".";
        case TOKEN_DOTDOT: return "..";
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

// lexer_peek_n(lexer, 0) is equivalent to lexer_peek(lexer)
static char lexer_peek_n(lexer_t* lexer, size_t offset)
{
    if (lexer->pos + offset >= lexer->length)
        return '\0';
    return lexer->source[lexer->pos + offset];
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

static void lexer_skip_whitespace(lexer_t* lexer)
{
    while (isspace(lexer_peek(lexer)))
        lexer_advance(lexer);
}

static void lexer_skip_until_next_token(lexer_t* lexer)
{
    while (true)
    {
        lexer_skip_whitespace(lexer);

        // Check for comments
        if (lexer_peek(lexer) != '/')
            break;

        // Save position in case it's not a comment
        size_t saved_pos = lexer->pos;
        int saved_line = lexer->line;
        int saved_col = lexer->column;

        lexer_advance(lexer); // consume '/'

        if (lexer_peek(lexer) == '/')
        {
            // Single-line comment: skip until newline
            lexer_advance(lexer); // consume second '/'
            while (lexer_peek(lexer) != '\n' && lexer_peek(lexer) != '\0')
                lexer_advance(lexer);
        }
        else if (lexer_peek(lexer) == '*')
        {
            // Multi-line comment: skip until */
            lexer_advance(lexer); // consume '*'
            while (true)
            {
                if (lexer_peek(lexer) == '\0')
                {
                    emit_error(lexer, "unterminated multi-line comment", saved_line, saved_col);
                    break;
                }
                if (lexer_peek(lexer) == '*')
                {
                    lexer_advance(lexer);
                    if (lexer_peek(lexer) == '/')
                    {
                        lexer_advance(lexer); // consume '/'
                        break; // End of multi-line comment
                    }
                }
                else
                {
                    lexer_advance(lexer);
                }
            }
        }
        else
        {
            // Not a comment, restore position
            lexer->pos = saved_pos;
            lexer->line = saved_line;
            lexer->column = saved_col;
            break;
        }
    }
}

static token_t* token_create(lexer_t* lexer, token_type_t type, const char* value, int line, int col)
{
    token_t* tok = malloc(sizeof(*tok));

    *tok = (token_t){
        .type = type,
        .value = value ? strdup(value) : nullptr,
        .suffix = (char*)EMPTY_SUFFIX,
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
        if (tok->suffix != EMPTY_SUFFIX)
            free(tok->suffix);
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
    token_type_t type = TOKEN_INTEGER;
    string_t value = STRING_INIT;
    string_t suffix = STRING_INIT;

    // Check for minus sign
    if (lexer_peek(lexer) == '-')
        string_append_char(&value, lexer_advance(lexer));

    // Lex integer part
    while (isdigit(lexer_peek(lexer)) || lexer_peek(lexer) == '_')
    {
        char c = lexer_advance(lexer);
        if (isdigit(c))
            string_append_char(&value, c);
    }

    // Check for decimal point
    if (lexer_peek_n(lexer, 0) == '.' && lexer_peek_n(lexer, 1) != '.')  // avoid misinterpreting ".."
    {
        type = TOKEN_FLOAT;
        string_append_char(&value, lexer_advance(lexer));  // consume '.'

        while (isdigit(lexer_peek(lexer)) || lexer_peek(lexer) == '_')
        {
            char c = lexer_advance(lexer);
            if (isdigit(c))
                string_append_char(&value, c);
        }
    }

    // Check for exponent (e or E)
    if (lexer_peek(lexer) == 'e' || lexer_peek(lexer) == 'E')
    {
        type = TOKEN_FLOAT;
        string_append_char(&value, lexer_advance(lexer));

        // Optional sign
        if (lexer_peek(lexer) == '+' || lexer_peek(lexer) == '-')
            string_append_char(&value, lexer_advance(lexer));

        // Exponent digits (required)
        if (!isdigit(lexer_peek(lexer)))
        {
            emit_error(lexer, "missing exponent", lexer->line, lexer->column);
            type = TOKEN_UNKNOWN;
            goto end;
        }

        while (isdigit(lexer_peek(lexer)) || lexer_peek(lexer) == '_')
        {
            char c = lexer_advance(lexer);
            if (isdigit(c))
                string_append_char(&value, c);
        }
    }

    // Lex suffix (if present)
    if (isalpha(lexer_peek(lexer)) || lexer_peek(lexer) == '_')
    {
        while (isalnum(lexer_peek(lexer)) || lexer_peek(lexer) == '_')
        {
            char c = lexer_advance(lexer);
            if (isalnum(c))
                string_append_char(&suffix, c);
        }
    }

end:
    token_t* tok = token_create(lexer, type, string_cstr(&value), start_line, start_col);
    if (string_len(&suffix) > 0)
        tok->suffix = string_release(&suffix);

    string_deinit(&value);
    string_deinit(&suffix);
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

    token_t* tok = token_create(lexer, TOKEN_STRING_LIT, value, start_line, start_col);
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
        .peeked_tokens = VEC_INIT(nullptr),
        .created_tokens = VEC_INIT(token_destroy),
    };

    return lexer;
}

void lexer_destroy(lexer_t *lexer)
{
    if (lexer != nullptr)
    {
        vec_deinit(&lexer->peeked_tokens);
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
        case '[': return token_create(lexer, TOKEN_LBRACKET, "[", line, col);
        case ']': return token_create(lexer, TOKEN_RBRACKET, "]", line, col);
        case ';': return token_create(lexer, TOKEN_SEMICOLON, ";", line, col);
        case ':': return token_create(lexer, TOKEN_COLON, ":", line, col);
        case ',': return token_create(lexer, TOKEN_COMMA, ",", line, col);
        case '&': return token_create(lexer, TOKEN_AMPERSAND, "&", line, col);
        case '@': return token_create(lexer, TOKEN_AT, "@", line, col);

        case '+':
            if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                return token_create(lexer, TOKEN_PLUS_ASSIGN, "+=", line, col);
            }
            else if (lexer_peek(lexer) == '+') {
                lexer_advance(lexer);
                return token_create(lexer, TOKEN_PLUSPLUS, "++", line, col);
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
            else if (lexer_peek(lexer) == '-') {
                lexer_advance(lexer);
                return token_create(lexer, TOKEN_MINUSMINUS, "--", line, col);
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

        case '.':
            if (lexer_peek(lexer) == '.') {
                lexer_advance(lexer);
                return token_create(lexer, TOKEN_DOTDOT, "..", line, col);
            }
            return token_create(lexer, TOKEN_DOT, ".", line, col);

        default:
        {
            char val[2] = {c, '\0'};
            return token_create(lexer, TOKEN_UNKNOWN, val, line, col);
        }
    }
}

static token_t* lex_next_token(lexer_t* lexer)
{
    lexer_skip_until_next_token(lexer);  // skips comments & whitespace

    if (lexer->pos >= lexer->length)
        return token_create(lexer, TOKEN_EOF, NULL, lexer->line, lexer->column);

    const char c = lexer_peek(lexer);

    if (isalpha(c) || c == '_')
        return lex_identifier(lexer);

    if (isdigit(c) || (c == '-' && isdigit(lexer_peek_n(lexer, 1))))
        return lex_number(lexer);

    if (c == '"')
        return lex_string(lexer);

    return lex_symbol(lexer);
}

token_t* lexer_peek_token_n(lexer_t* lexer, size_t n)
{
    // Ensure we have enough peeked tokens
    while (vec_size(&lexer->peeked_tokens) <= n)
    {
        token_t* tok = lex_next_token(lexer);
        vec_push(&lexer->peeked_tokens, tok);
    }

    return vec_get(&lexer->peeked_tokens, n);
}

token_t* lexer_next_token(lexer_t* lexer)
{
    token_t* tok;
    if (vec_size(&lexer->peeked_tokens) == 0)
    {
        tok = lex_next_token(lexer);
    }
    else
    {
        tok = vec_get(&lexer->peeked_tokens, 0);
        vec_remove(&lexer->peeked_tokens, 0);
    }

    lexer->last_consumed_end_line = lexer->line;
    lexer->last_consumed_end_column = lexer->column;
    return tok;
}

token_t* lexer_peek_token(lexer_t* lexer)
{
    return lexer_peek_token_n(lexer, 0);
}

void lexer_emit_token_malformed(lexer_t* lexer, token_t* tok, const char* description)
{
    string_t err = token_str(tok);
    if (lexer->error_output == nullptr)
    {
        printf("Error: %s for tok '%s' in File %s at Line %d, Col %d\n", description, string_cstr(&err),
            lexer->filename, tok->line, tok->column);
    }
    else
    {
        string_append_cstr(&err, ": ");
        string_append_cstr(&err, description);
        compiler_error_t* error = compiler_error_create_for_source(false, string_cstr(&err), lexer->filename,
            tok->line, tok->column);
        lexer->error_output(error, lexer->error_output_arg);
    }
    string_deinit(&err);
}

void lexer_emit_error_for_token(lexer_t* lexer, token_t* actual, token_type_t expected)
{
    int line = lexer->last_consumed_end_line;
    int column = lexer->last_consumed_end_column;
    if (lexer->error_output == nullptr)
    {
        printf("Error: Expected token %s, but found %s in File %s at Line %d, Col %d\n", token_type_str(expected),
            token_type_str(actual->type), lexer->filename, line, column);
    }
    else
    {
        char* err;
        if (expected == TOKEN_UNKNOWN)
        {
            line = actual->line;
            column = actual->column;
            string_t tok_str = token_str(actual);
            err = ssprintf("token '%s' is not valid in this context", string_cstr(&tok_str));
            string_deinit(&tok_str);
        }
        else
        {
            err = ssprintf("expected '%s'", token_type_str(expected));
        }
        compiler_error_t* error = compiler_error_create_for_source(false, err, lexer->filename, line, column);
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

bool token_type_is_unary_op(token_type_t token_type)
{
    switch (token_type)
    {
        case TOKEN_AMPERSAND:
        case TOKEN_STAR:
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
