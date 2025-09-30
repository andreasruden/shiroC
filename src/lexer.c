#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

typedef struct
{
    const char* source;
    size_t pos;
    size_t length;
    int line;
    int column;
} lexer_t;

typedef struct
{
    const char* keyword;
    token_type_t type;
} keyword_t;

keyword_t lexer_keywords[] =
{
    {"int", TOKEN_INT},
    {"return", TOKEN_RETURN},
    {NULL, TOKEN_UNKNOWN}
};

const char* token_type_str(token_type_t type)
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

void lexer_init(lexer_t* lex, const char* source)
{
    lex->source = source;
    lex->pos = 0;
    lex->length = strlen(source);
    lex->line = 1;
    lex->column = 1;
}

char lexer_peek(lexer_t* lex)
{
    if (lex->pos >= lex->length)
        return '\0';
    return lex->source[lex->pos];
}

char lexer_advance(lexer_t* lex)
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

void lexer_skip_whitespace(lexer_t* lex)
{
    while (isspace(lexer_peek(lex)))
        lexer_advance(lex);
}

token_t* token_create(token_type_t type, const char* value, int line, int col)
{
    token_t* tok = malloc(sizeof(*tok));
    tok->type = type;
    tok->value = value ? strdup(value) : NULL;
    tok->line = line;
    tok->column = col;
    return tok;
}

void token_free(token_t* tok)
{
    if (tok != nullptr)
    {
        free(tok->value);
        free(tok);
    }
}

token_type_t lookup_keyword(const char* id)
{
    for (int i = 0; lexer_keywords[i].keyword != NULL; ++i)
    {
        if (strcmp(id, lexer_keywords[i].keyword) == 0)
            return lexer_keywords[i].type;
    }

    return TOKEN_IDENTIFIER;
}

token_t* lex_identifier(lexer_t* lexer)
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

token_t* lex_number(lexer_t* lexer)
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

token_t* lex_string(lexer_t* lexer)
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

token_t* lex_next_token(lexer_t* lexer)
{
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

int main()
{
    const char* source = 
        "int main() {\n"
        "  puts(\"Hello world\");\n"
        "  return 0;\n"
        "}\n";
    
    printf("Source code:\n%s\n", source);
    printf("Tokens:\n");
    printf("%-12s %-20s %s\n", "TYPE", "VALUE", "POSITION");
    printf("%-12s %-20s %s\n", "----", "-----", "--------");
    
    lexer_t lexer;
    lexer_init(&lexer, source);
    
    token_t* tok;
    while ((tok = lex_next_token(&lexer))->type != TOKEN_EOF)
    {
        printf("%-12s %-20s Line %d, Col %d\n", 
               token_type_str(tok->type),
               tok->value ? tok->value : "",
               tok->line,
               tok->column);
        token_free(tok);
    }
    token_free(tok);
    
    return 0;
}
