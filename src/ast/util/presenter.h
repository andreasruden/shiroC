#ifndef AST_PRESENTER__H
#define AST_PRESENTER__H

#include "ast/node.h"

/*
 * Get a single line representation of the given node. This visitor does not generally recurse
 * down the tree, but always returns a single line representation. The intent is to be able
 * to inject it into IR to more easily see what part of the source generated what IR.
 */

typedef struct ast_presenter ast_presenter_t;

ast_presenter_t* ast_presenter_create();

void ast_presenter_destroy(ast_presenter_t* presenter);

char* ast_presenter_present_node(ast_presenter_t* presenter, ast_node_t* node);

#endif
