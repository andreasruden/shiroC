#ifndef BUILDER__H
#define BUILDER__H

#include "common/containers/hash_table.h"
#include "common/containers/vec.h"

/* Represents a dependency project loaded from [[dep]] in shiro.toml */
typedef struct dependency
{
    char* name;              // Dependency project name (e.g., "math-utils")
    char* src_path;          // Path to dependency project root
    vec_t modules;           // module_t* - modules from this dependency
    char* project_name;      // Parsed from dependency's shiro.toml [project] section
} dependency_t;

dependency_t* dependency_create(const char* name, const char* src_path);

void dependency_destroy(dependency_t* dep);

void dependency_destroy_void(void* dep);

/* Entry point to build system. Manages compiling a whole project as defined
 * by shiro.toml in the root directory.
 */
typedef struct builder
{
    char* project;
    char* root_dir;
    char* build_dir;  // Intermediate build artifacts (.ll, .o files)
    char* bin_dir;    // Final executables
    hash_table_t modules;  // All modules to be built; name of module (char*) -> module_t*
    vec_t dependencies;    // dependency_t* - all loaded dependency projects
} builder_t;

builder_t* builder_create(const char* root_dir, const char* compiler_path);

void builder_destroy(builder_t* builder);

bool builder_run(builder_t* builder);

#endif
