#ifndef BUILDER__H
#define BUILDER__H

#include "common/containers/hash_table.h"

/* Entry point to build system. Manages compiling a whole project as defined
 * by shiro.toml in the root directory.
 */
typedef struct builder
{
    char* root_dir;
    char* build_dir;  // Intermediate build artifacts (.ll, .o files)
    char* bin_dir;    // Final executables
    hash_table_t modules;  // All modules to be built; name of module (char*) -> module_t*
} builder_t;

builder_t* builder_create(const char* root_dir);

void builder_destroy(builder_t* builder);

bool builder_run(builder_t* builder);

#endif
