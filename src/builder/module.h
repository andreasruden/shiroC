#ifndef BUILDER_MODULE__H
#define BUILDER_MODULE__H

#include "ast/root.h"
#include "common/containers/vec.h"
#include "sema/semantic_context.h"

typedef struct builder builder_t;

typedef enum module_kind
{
    MODULE_BINARY,
    MODULE_LIBRARY,
} module_kind_t;

typedef struct module_src
{
    char* filepath;
    ast_root_t* ast;
} module_src_t;

typedef struct module
{
    builder_t* builder;
    module_kind_t kind;
    char* name;
    char* src_dir;
    vec_t sources;  // module_src_t*
    vec_t dependencies;  // name of module (char*)
    semantic_context_t* sema_context;
} module_t;

module_t* module_create(builder_t* builder, const char* name, const char* src_dir, module_kind_t kind);

void module_destroy(module_t* module);

void module_destroy_void(void* module);

// Construct AST from source code into module.ast
bool module_parse_src(module_t* module);

// Build global symbol table into module.sema_context
bool module_decl_collect(module_t* module);

// Populates module.dependencies
bool module_populate_dependencies(module_t* module);

// Compile module into objects
bool module_compile(module_t* module);

// Link module with dependencies, producing an executable
// Should only be used for kind MODULE_BINARY
bool module_link(module_t* module);

#endif
