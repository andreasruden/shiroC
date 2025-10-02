#ifndef PARSER__H
#define PARSER__H

#include "lexer.h"
#include "ast/root.h"

typedef struct parser
{
    lexer_t* lexer;
} parser_t;

parser_t* parser_create(lexer_t* lexer);

void parser_destroy(parser_t* parser);

ast_root_t* parser_parse(parser_t* parser);

#endif
