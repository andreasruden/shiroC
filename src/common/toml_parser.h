#ifndef COMMON_TOML_PARSER__H
#define COMMON_TOML_PARSER__H

#include "containers/hash_table.h"
#include "containers/vec.h"

/*
 * The most underdeveloped TOML parser for simple configuration files.
 *
 * Supported:
 *   - Section headers: [name]
 *   - Array-of-tables: [[name]]
 *   - String key-value pairs: key = "value"
 *   - Comments: # comment
 *
 * Not supported:
 *   - Numbers, booleans, dates, arrays
 *   - Nested tables, dotted keys
 *   - Inline tables
 *   - Multi-line strings
 *   - Full escape sequences
 *   - And more
 *
 * Data Structure:
 *   - Returns hash_table_t* where:
 *     - Regular sections [name] map to hash_table_t* (string->string)
 *     - Array sections [[name]] map to vec_t* of hash_table_t* (each entry is string->string)
 */

/* Parse TOML from a file.
 * Returns hash_table_t* on success, nullptr on error.
 * Caller must call toml_destroy() to free the returned hash table.
 */
hash_table_t* toml_parse_file(const char* filename);

/* Parse TOML from a string.
 * Returns hash_table_t* on success, nullptr on error.
 * Caller must call toml_destroy() to free the returned hash table.
 */
hash_table_t* toml_parse_string(const char* content);

/* Destroy a TOML hash table and all nested structures. */
void toml_destroy(hash_table_t* root);

/* Check if a value from the root hash table is a regular section (hash_table_t*).
 * Returns true if the value is a regular section [name], false otherwise.
 */
bool toml_is_section(void* value);

/* Check if a value from the root hash table is an array section (vec_t*).
 * Returns true if the value is an array section [[name]], false otherwise.
 */
bool toml_is_array_section(void* value);

/* Safely cast a value to hash_table_t* if it's a regular section.
 * Returns the hash_table_t* if the value is a regular section, nullptr otherwise.
 */
hash_table_t* toml_as_section(void* value);

/* Safely cast a value to vec_t* if it's an array section.
 * Returns the vec_t* if the value is an array section, nullptr otherwise.
 */
vec_t* toml_as_array_section(void* value);

#endif
