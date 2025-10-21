#include "common/toml_parser.h"
#include "common/test-runner/test_runner.h"
#include "common/containers/vec.h"
#include <string.h>

TEST_FIXTURE(toml_parser_fixture_t)
{
    hash_table_t* root;
};

TEST_SETUP(toml_parser_fixture_t)
{
    fix->root = nullptr;
}

TEST_TEARDOWN(toml_parser_fixture_t)
{
    if (fix->root)
        toml_destroy(fix->root);
}

TEST(toml_parser_fixture_t, parse_simple_section)
{
    const char* toml = "[package]\nname = \"Shiro\"";
    fix->root = toml_parse_string(toml);

    ASSERT_NEQ(nullptr, fix->root);
    hash_table_t* package = hash_table_find(fix->root, "package");
    ASSERT_NEQ(nullptr, package);
    char* name = hash_table_find(package, "name");
    ASSERT_NEQ(nullptr, name);
    ASSERT_EQ(0, strcmp(name, "Shiro"));
}

TEST(toml_parser_fixture_t, parse_multiple_keys)
{
    const char* toml = "[package]\nname = \"Shiro\"\nversion = \"0.1\"";
    fix->root = toml_parse_string(toml);

    ASSERT_NEQ(nullptr, fix->root);
    hash_table_t* package = hash_table_find(fix->root, "package");
    ASSERT_NEQ(nullptr, package);

    char* name = hash_table_find(package, "name");
    ASSERT_EQ(0, strcmp(name, "Shiro"));

    char* version = hash_table_find(package, "version");
    ASSERT_EQ(0, strcmp(version, "0.1"));
}

TEST(toml_parser_fixture_t, parse_array_of_tables)
{
    const char* toml = "[[lib]]\nname = \"AST\"\n\n[[lib]]\nname = \"Parser\"";
    fix->root = toml_parse_string(toml);

    ASSERT_NEQ(nullptr, fix->root);
    vec_t* lib_array = hash_table_find(fix->root, "lib");
    ASSERT_NEQ(nullptr, lib_array);
    ASSERT_EQ(2, vec_size(lib_array));

    hash_table_t* lib1 = vec_get(lib_array, 0);
    char* name1 = hash_table_find(lib1, "name");
    ASSERT_EQ(0, strcmp(name1, "AST"));

    hash_table_t* lib2 = vec_get(lib_array, 1);
    char* name2 = hash_table_find(lib2, "name");
    ASSERT_EQ(0, strcmp(name2, "Parser"));
}

TEST(toml_parser_fixture_t, parse_full_config)
{
    const char* toml =
        "[package]\n"
        "name = \"Shiro\"\n"
        "version = \"0.1\"\n"
        "\n"
        "[[lib]]\n"
        "name = \"AST\"\n"
        "source = \"src/ast\"\n"
        "\n"
        "[[lib]]\n"
        "name = \"Parser\"\n"
        "namespace = \"Parser\"\n"
        "source = \"src/parser\"\n"
        "\n"
        "[[bin]]\n"
        "name = \"shiro\"\n"
        "source = \"src/compiler\"\n";

    fix->root = toml_parse_string(toml);
    ASSERT_NEQ(nullptr, fix->root);

    // Check package section
    hash_table_t* package = hash_table_find(fix->root, "package");
    ASSERT_NEQ(nullptr, package);
    ASSERT_EQ(0, strcmp((char*)hash_table_find(package, "name"), "Shiro"));
    ASSERT_EQ(0, strcmp((char*)hash_table_find(package, "version"), "0.1"));

    // Check lib array
    vec_t* lib_array = hash_table_find(fix->root, "lib");
    ASSERT_NEQ(nullptr, lib_array);
    ASSERT_EQ(2, vec_size(lib_array));

    hash_table_t* lib1 = vec_get(lib_array, 0);
    ASSERT_EQ(0, strcmp((char*)hash_table_find(lib1, "name"), "AST"));
    ASSERT_EQ(0, strcmp((char*)hash_table_find(lib1, "source"), "src/ast"));

    hash_table_t* lib2 = vec_get(lib_array, 1);
    ASSERT_EQ(0, strcmp((char*)hash_table_find(lib2, "name"), "Parser"));
    ASSERT_EQ(0, strcmp((char*)hash_table_find(lib2, "namespace"), "Parser"));
    ASSERT_EQ(0, strcmp((char*)hash_table_find(lib2, "source"), "src/parser"));

    // Check bin array
    vec_t* bin_array = hash_table_find(fix->root, "bin");
    ASSERT_NEQ(nullptr, bin_array);
    ASSERT_EQ(1, vec_size(bin_array));

    hash_table_t* bin1 = vec_get(bin_array, 0);
    ASSERT_EQ(0, strcmp((char*)hash_table_find(bin1, "name"), "shiro"));
    ASSERT_EQ(0, strcmp((char*)hash_table_find(bin1, "source"), "src/compiler"));
}

TEST(toml_parser_fixture_t, parse_with_comments)
{
    const char* toml =
        "# This is a comment\n"
        "[package]\n"
        "name = \"Shiro\"  # inline comment\n"
        "# Another comment\n"
        "version = \"0.1\"\n";

    fix->root = toml_parse_string(toml);
    ASSERT_NEQ(nullptr, fix->root);

    hash_table_t* package = hash_table_find(fix->root, "package");
    ASSERT_NEQ(nullptr, package);
    ASSERT_EQ(0, strcmp((char*)hash_table_find(package, "name"), "Shiro"));
    ASSERT_EQ(0, strcmp((char*)hash_table_find(package, "version"), "0.1"));
}

TEST(toml_parser_fixture_t, parse_empty_string)
{
    fix->root = toml_parse_string("");
    ASSERT_NEQ(nullptr, fix->root);
}

TEST(toml_parser_fixture_t, parse_null_returns_null)
{
    fix->root = toml_parse_string(nullptr);
    ASSERT_EQ(nullptr, fix->root);
}

TEST(toml_parser_fixture_t, parse_string_with_escapes)
{
    const char* toml = "[test]\nstr = \"hello\\nworld\\t!\"";
    fix->root = toml_parse_string(toml);

    ASSERT_NEQ(nullptr, fix->root);
    hash_table_t* test = hash_table_find(fix->root, "test");
    char* str = hash_table_find(test, "str");
    ASSERT_EQ(0, strcmp(str, "hello\nworld\t!"));
}
