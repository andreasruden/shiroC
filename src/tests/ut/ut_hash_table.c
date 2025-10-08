#include "common/containers/hash_table.h"
#include "common/test-runner/test_runner.h"

TEST_FIXTURE(hash_table_fixture_t)
{
    hash_table_t* table;
};

TEST_SETUP(hash_table_fixture_t)
{
    fix->table = hash_table_create(nullptr);
}

TEST_TEARDOWN(hash_table_fixture_t)
{
    hash_table_destroy(fix->table);
}

TEST(hash_table_fixture_t, insert_and_find)
{
    int value = 42;
    hash_table_insert(fix->table, "key", &value);

    int* found = (int*)hash_table_find(fix->table, "key");
    ASSERT_NEQ(nullptr, found);
    ASSERT_EQ(42, *found);
}

TEST(hash_table_fixture_t, find_nonexistent)
{
    void* found = hash_table_find(fix->table, "nonexistent");
    ASSERT_EQ(nullptr, found);
    ASSERT_EQ(0, fix->table->size);
}

TEST(hash_table_fixture_t, multiple_insertions)
{
    int v1 = 1, v2 = 2, v3 = 3;
    hash_table_insert(fix->table, "one", &v1);
    hash_table_insert(fix->table, "two", &v2);
    hash_table_insert(fix->table, "three", &v3);

    ASSERT_EQ(3, fix->table->size);
    ASSERT_EQ(1, *(int*)hash_table_find(fix->table, "one"));
    ASSERT_EQ(2, *(int*)hash_table_find(fix->table, "two"));
    ASSERT_EQ(3, *(int*)hash_table_find(fix->table, "three"));
}

TEST(hash_table_fixture_t, remove)
{
    int value = 42;
    hash_table_insert(fix->table, "key", &value);

    hash_table_remove(fix->table, "key");

    ASSERT_EQ(0, fix->table->size);
    ASSERT_EQ(nullptr, hash_table_find(fix->table, "key"));
}

TEST(hash_table_fixture_t, growth)
{
    ASSERT_EQ(0, fix->table->size);
    ASSERT_EQ(16, fix->table->num_buckets);

    // Insert enough to trigger two resizes
    for (int i = 0; i < 45; ++i) {
        char key[32];
        sprintf(key, "key%d", i);
        hash_table_insert(fix->table, key, (void*)(intptr_t)(i + 1));
    }

    ASSERT_EQ(45, fix->table->size);
    ASSERT_EQ(64, fix->table->num_buckets);

    // Verify all keys still findable after resize
    for (int i = 0; i < 45; i++) {
        char key[32];
        sprintf(key, "key%d", i);
        void* found = hash_table_find(fix->table, key);
        ASSERT_NEQ(nullptr, found);
        ASSERT_EQ(i + 1, (int)(intptr_t)found);
    }
}

static int delete_count = 0;
static void count_deletes(void* value) {
    (void)value;
    delete_count++;
}

TEST(hash_table_fixture_t, delete_function_called)
{
    (void)fix;

    delete_count = 0;
    hash_table_t* table = hash_table_create(count_deletes);

    int v1 = 1, v2 = 2, v3 = 3;
    hash_table_insert(table, "one", &v1);
    hash_table_insert(table, "two", &v2);
    hash_table_insert(table, "three", &v3);

    hash_table_remove(table, "two");
    ASSERT_EQ(1, delete_count);

    hash_table_destroy(table);
    ASSERT_EQ(3, delete_count);
}
