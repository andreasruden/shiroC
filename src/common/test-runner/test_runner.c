#include "test_runner.h"

// Global test registry
test_entry_t g_tests[MAX_TESTS];
int g_test_count = 0;
void* g_current_fixture = NULL;
jmp_buf g_test_jmp_buf;
bool g_test_failed = false;

int main(int argc, char** argv)
{
    char* test_name_pattern = nullptr;
    bool complete_match = false;
    if (argc >= 2)
    {
        test_name_pattern = argv[1];
        int len = strlen(test_name_pattern);
        if (test_name_pattern[len - 1] == '$')
        {
            test_name_pattern[len - 1] = '\0';
            complete_match = true;
        }
    }

    int passed = 0;
    int failed = 0;
    int total = 0;

    for (int i = 0; i < g_test_count; i++)
    {
        if (test_name_pattern == nullptr)
        {
            ++total;
            continue;
        }

        test_entry_t* test = &g_tests[i];
        if (complete_match ? (strcmp(test->name, test_name_pattern) == 0) :
            (strstr(test->name, test_name_pattern) != nullptr))
        {
            ++total;
        }
    }

    printf("=================================================\n");
    printf("Running %d test(s)\n", total);
    printf("=================================================\n\n");

    for (int i = 0; i < g_test_count; i++)
    {
        test_entry_t* test = &g_tests[i];

        if (test_name_pattern != nullptr)
        {
            if (complete_match ? (strcmp(test->name, test_name_pattern) != 0) :
                (strstr(test->name, test_name_pattern) == nullptr))
            {
                continue;
            }
        }

        void* fixture = calloc(1, test->fixture_size);
        if (!fixture)
        {
            printf("[FAIL] %s - fixture allocation failed\n", test->name);
            failed++;
            continue;
        }
        g_current_fixture = fixture;
        g_test_failed = false;

        if (test->setup_fn)
            test->setup_fn(fixture);

        // Run test with setjmp for assertion handling
        printf("[ RUN ] %s\n", test->name);
        if (setjmp(g_test_jmp_buf) == 0) {
            test->test_fn();
        }

        // Run teardown
        if (test->teardown_fn) {
            test->teardown_fn(fixture);
        }

        // Report result
        if (g_test_failed) {
            printf( "[" COLOR_BOLD_RED " FAIL" COLOR_RESET "] %s\n\n", test->name);
            failed++;
        } else {
            printf( "[" COLOR_BOLD_GREEN " PASS" COLOR_RESET "] %s\n\n", test->name);
            passed++;
        }

        // Clean up
        free(fixture);
        g_current_fixture = NULL;
    }

    printf("=================================================\n");
    printf("Test Results:\n");
    printf("  Passed: %d\n", passed);
    printf("  Failed: %d\n", failed);
    printf("  Total:  %d\n", total);
    printf("=================================================\n");

    return (failed > 0) ? 1 : 0;
}
