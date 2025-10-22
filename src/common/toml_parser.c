#include "toml_parser.h"
#include "containers/string.h"
#include "containers/vec.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct toml_parser
{
    const char* input;
    size_t pos;
    size_t len;
    hash_table_t* root;
    hash_table_t* current_section;
    bool in_array_section;
    bool has_array_sections;
} toml_parser_t;

static void skip_whitespace(toml_parser_t* parser)
{
    while (parser->pos < parser->len && isspace((unsigned char)parser->input[parser->pos]) &&
           parser->input[parser->pos] != '\n')
        parser->pos++;
}

static void skip_line(toml_parser_t* parser)
{
    while (parser->pos < parser->len && parser->input[parser->pos] != '\n')
        parser->pos++;
    if (parser->pos < parser->len && parser->input[parser->pos] == '\n')
        parser->pos++;
}

static bool peek_char(toml_parser_t* parser, char c)
{
    return parser->pos < parser->len && parser->input[parser->pos] == c;
}

static bool consume_char(toml_parser_t* parser, char c)
{
    if (peek_char(parser, c))
    {
        parser->pos++;
        return true;
    }
    return false;
}

static char* parse_identifier(toml_parser_t* parser)
{
    size_t start = parser->pos;
    while (parser->pos < parser->len)
    {
        char c = parser->input[parser->pos];
        if (isalnum((unsigned char)c) || c == '_' || c == '-')
            parser->pos++;
        else
            break;
    }
    if (parser->pos == start)
        return nullptr;
    size_t len = parser->pos - start;
    char* result = malloc(len + 1);
    memcpy(result, &parser->input[start], len);
    result[len] = '\0';
    return result;
}

static char* parse_string(toml_parser_t* parser)
{
    if (!consume_char(parser, '"'))
        return nullptr;
    string_t str = STRING_INIT;
    while (parser->pos < parser->len && parser->input[parser->pos] != '"')
    {
        char c = parser->input[parser->pos++];
        if (c == '\\' && parser->pos < parser->len)
        {
            char next = parser->input[parser->pos++];
            switch (next)
            {
                case 'n':  string_append_char(&str, '\n'); break;
                case 't':  string_append_char(&str, '\t'); break;
                case '\\': string_append_char(&str, '\\'); break;
                case '"':  string_append_char(&str, '"');  break;
                default:   string_append_char(&str, next); break;
            }
        }
        else
            string_append_char(&str, c);
    }
    if (!consume_char(parser, '"'))
    {
        string_deinit(&str);
        return nullptr;
    }
    return string_release(&str);
}

static bool parse_key_value(toml_parser_t* parser)
{
    if (!parser->current_section)
        return false;
    char* key = parse_identifier(parser);
    if (!key)
        return false;
    skip_whitespace(parser);
    if (!consume_char(parser, '='))
    {
        free(key);
        return false;
    }
    skip_whitespace(parser);
    char* value = parse_string(parser);
    if (!value)
    {
        free(key);
        return false;
    }
    hash_table_insert(parser->current_section, key, value);
    free(key);
    return true;
}

static void hash_table_destroy_nested(void* ptr)
{
    hash_table_t* table = ptr;
    hash_table_destroy(table);
}

static void string_value_destroy(void* ptr)
{
    free(ptr);
}

static bool parse_section_header(toml_parser_t* parser)
{
    bool is_array = false;
    if (!consume_char(parser, '['))
        return false;
    if (consume_char(parser, '['))
        is_array = true;
    char* section_name = parse_identifier(parser);
    if (!section_name)
        return false;
    if (!consume_char(parser, ']'))
    {
        free(section_name);
        return false;
    }
    if (is_array && !consume_char(parser, ']'))
    {
        free(section_name);
        return false;
    }

    if (is_array)
    {
        vec_t* array = hash_table_find(parser->root, section_name);
        if (!array)
        {
            array = vec_create(hash_table_destroy_nested);
            hash_table_insert(parser->root, section_name, array);
        }
        parser->current_section = hash_table_create(string_value_destroy);
        vec_push(array, parser->current_section);
        parser->in_array_section = true;
        parser->has_array_sections = true;
    }
    else
    {
        parser->current_section = hash_table_find(parser->root, section_name);
        if (!parser->current_section)
        {
            parser->current_section = hash_table_create(string_value_destroy);
            hash_table_insert(parser->root, section_name, parser->current_section);
        }
        parser->in_array_section = false;
    }

    free(section_name);
    return true;
}

static void root_delete_value(void* ptr)
{
    /* The root hash table can contain either hash_table_t* or vec_t*.
     * We distinguish by checking the delete_fn field which is at the same offset in both structs.
     * - vec_t will have hash_table_destroy_nested
     * - hash_table_t will have string_value_destroy
     */
    vec_t* maybe_vec = ptr;
    if (maybe_vec && maybe_vec->delete_fn == hash_table_destroy_nested)
        vec_destroy(maybe_vec);
    else
    {
        hash_table_t* table = ptr;
        hash_table_destroy(table);
    }
}

static hash_table_t* parse_toml(const char* content)
{
    toml_parser_t parser = {
        .input = content,
        .pos = 0,
        .len = strlen(content),
        .root = hash_table_create(nullptr),
        .current_section = nullptr,
        .in_array_section = false,
        .has_array_sections = false,
    };

    while (parser.pos < parser.len)
    {
        skip_whitespace(&parser);
        if (parser.pos >= parser.len)
            break;
        if (consume_char(&parser, '\n'))
            continue;
        if (consume_char(&parser, '#'))
        {
            skip_line(&parser);
            continue;
        }
        if (peek_char(&parser, '['))
        {
            if (!parse_section_header(&parser))
            {
                hash_table_destroy(parser.root);
                return nullptr;
            }
        }
        else
        {
            if (!parse_key_value(&parser))
            {
                hash_table_destroy(parser.root);
                return nullptr;
            }
        }
        skip_whitespace(&parser);
        if (parser.pos < parser.len && !consume_char(&parser, '\n'))
        {
            if (!peek_char(&parser, '#'))
            {
                hash_table_destroy(parser.root);
                return nullptr;
            }
            skip_line(&parser);
        }
    }

    if (parser.has_array_sections)
        parser.root->delete_fn = root_delete_value;
    else
        parser.root->delete_fn = hash_table_destroy_nested;

    return parser.root;
}

hash_table_t* toml_parse_string(const char* content)
{
    if (!content)
        return nullptr;
    return parse_toml(content);
}

hash_table_t* toml_parse_file(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if (!file)
        return nullptr;
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* content = malloc((size_t)file_size + 1);
    size_t bytes_read = fread(content, 1, (size_t)file_size, file);
    content[bytes_read] = '\0';
    fclose(file);
    hash_table_t* result = parse_toml(content);
    free(content);
    return result;
}

void toml_destroy(hash_table_t* root)
{
    if (root)
        hash_table_destroy(root);
}

bool toml_is_array_section(void* value)
{
    if (!value)
        return false;
    vec_t* maybe_vec = value;
    return maybe_vec->delete_fn == hash_table_destroy_nested;
}

bool toml_is_section(void* value)
{
    if (!value)
        return false;
    return !toml_is_array_section(value);
}

hash_table_t* toml_as_section(void* value)
{
    if (toml_is_section(value))
        return (hash_table_t*)value;
    return nullptr;
}

vec_t* toml_as_array_section(void* value)
{
    if (toml_is_array_section(value))
        return (vec_t*)value;
    return nullptr;
}
