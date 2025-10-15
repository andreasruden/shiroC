#ifndef TEST_RUNNER__H
#define TEST_RUNNER__H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#define MAX_TESTS 256

#define COLOR_RESET       "\x1b[0m"
#define COLOR_RED         "\x1b[31m"
#define COLOR_GREEN       "\x1b[32m"
#define COLOR_CYAN        "\x1b[36m"
#define COLOR_BOLD_RED    "\x1b[1;31m"
#define COLOR_BOLD_GREEN  "\x1b[1;32m"

typedef struct test_entry
{
    const char* name;
    void (*test_fn)(void);
    void* fixture;
    void (*setup_fn)(void*);
    void (*teardown_fn)(void*);
    size_t fixture_size;
} test_entry_t;

extern test_entry_t g_tests[MAX_TESTS];
extern int g_test_count;
extern void* g_current_fixture;
extern jmp_buf g_test_jmp_buf;
extern bool g_test_failed;

// This macro defines a test-fixture. It must be used once in ever test-suite.
#define TEST_FIXTURE(fixture_name) \
    typedef struct fixture_name fixture_name; \
    struct fixture_name

// This macro defines a function that will be called to setup the test-fixture
// before every test. Providing a setup function is optional.
#define TEST_SETUP(fixture_name) \
    void fixture_name##_setup(fixture_name* fix); \
    static void __setup_wrapper_##fixture_name(void* fix) { \
        fixture_name##_setup((fixture_name*)fix); \
    } \
    void fixture_name##_setup(fixture_name* fix)

// This macro defines a function that will be called to teardown the
// test-fixture after every test. Providing a teardown function is optional.
#define TEST_TEARDOWN(fixture_name) \
    void fixture_name##_teardown(fixture_name* fix); \
    static void __teardown_wrapper_##fixture_name(void* fix) { \
        fixture_name##_teardown((fixture_name*)fix); \
    } \
    void fixture_name##_teardown(fixture_name* fix)

// This macro defines a test function. Every test function will be
// automatically ran by the test-runner's main function.
#define TEST(fixture_name, test_name) \
    void test_name##_impl(fixture_name* fix); \
    static void test_name##_wrapper(void) { \
        test_name##_impl((fixture_name*)g_current_fixture); \
    } \
    static void __attribute__((constructor)) register_##test_name(void) { \
        g_tests[g_test_count].name = #test_name; \
        g_tests[g_test_count].test_fn = test_name##_wrapper; \
        g_tests[g_test_count].fixture_size = sizeof(fixture_name); \
        g_tests[g_test_count].setup_fn = __setup_wrapper_##fixture_name; \
        g_tests[g_test_count].teardown_fn = __teardown_wrapper_##fixture_name; \
        g_test_count++; \
    } \
    void test_name##_impl(fixture_name* fix)

#define get_fixture() (g_current_fixture)

static inline int str_compare(const char* a, const char* b) { return (!a || !b) ? ((a == nullptr) - (b == nullptr)) : (strcmp(a, b)); }
static inline int int_compare(int a, int b) { return (a > b) - (a < b); }
static inline int uint_compare(unsigned int a, unsigned int b) { return (a > b) - (a < b); }
static inline int long_compare(long a, long b) { return (a > b) - (a < b); }
static inline int ulong_compare(unsigned long a, unsigned long b) { return (a > b) - (a < b); }
static inline int llong_compare(long long a, long long b) { return (a > b) - (a < b); }
static inline int ullong_compare(unsigned long long a, unsigned long long b) { return (a > b) - (a < b); }
static inline int float_compare(float a, float b) { return (a > b) - (a < b); }
static inline int double_compare(double a, double b) { return (a > b) - (a < b); }
static inline int char_compare(char a, char b) { return (a > b) - (a < b); }
static inline int uchar_compare(unsigned char a, unsigned char b) { return (a > b) - (a < b); }
static inline int short_compare(short a, short b) { return (a > b) - (a < b); }
static inline int ushort_compare(unsigned short a, unsigned short b) { return (a > b) - (a < b); }
static inline int ptr_compare(const void* a, const void* b) { return (a > b) - (a < b); }

#define CMP_FUNC(x) _Generic((x), \
    int: int_compare, \
    unsigned int: uint_compare, \
    long: long_compare, \
    unsigned long: ulong_compare, \
    long long: llong_compare, \
    unsigned long long: ullong_compare, \
    float: float_compare, \
    double: double_compare, \
    char: char_compare, \
    unsigned char: uchar_compare, \
    short: short_compare, \
    unsigned short: ushort_compare, \
    char*: str_compare, \
    const char*: str_compare, \
    default: ptr_compare \
)

static inline void print_int(int x) { printf("%d", x); }
static inline void print_uint(unsigned int x) { printf("%u", x); }
static inline void print_long(long x) { printf("%ld", x); }
static inline void print_ulong(unsigned long x) { printf("%lu", x); }
static inline void print_llong(long long x) { printf("%lld", x); }
static inline void print_ullong(unsigned long long x) { printf("%llu", x); }
static inline void print_float(float x) { printf("%f", x); }
static inline void print_double(double x) { printf("%f", x); }
static inline void print_char(char x) { printf("%c", x); }
static inline void print_uchar(unsigned char x) { printf("%u", x); }
static inline void print_short(short x) { printf("%d", x); }
static inline void print_ushort(unsigned short x) { printf("%u", x); }
static inline void print_str(const char* x) { printf("%s", x); }
static inline void print_ptr(const void* x) { printf("%p", x); }

#define PRINT_VALUE(x) _Generic((x), \
    int: print_int, \
    unsigned int: print_uint, \
    long: print_long, \
    unsigned long: print_ulong, \
    long long: print_llong, \
    unsigned long long: print_ullong, \
    float: print_float, \
    double: print_double, \
    char: print_char, \
    unsigned char: print_uchar, \
    short: print_short, \
    unsigned short: print_ushort, \
    char*: print_str, \
    const char*: print_str, \
    default: print_ptr \
)(x)

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            printf("  ASSERTION FAILED: %s (at %s:%d)\n", #expr, __FILE__, __LINE__); \
            g_test_failed = true; \
            longjmp(g_test_jmp_buf, 1); \
        } \
    } while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

// Assert a == b.
// Do not use expressions with side-effects as expressions are evaluated multiple times.
// Expressionf of type [const] char* are compared with strcmp.
#define ASSERT_EQ(a, b) \
    do { \
        if (CMP_FUNC(b)(a, b) != 0) { \
            printf(COLOR_BOLD_RED "ASSERTION FAILED: " COLOR_RESET); \
            printf(COLOR_CYAN #a " == " #b COLOR_RESET); \
            printf(" (at %s:%d)\n", __FILE__, __LINE__); \
            printf(COLOR_GREEN "  Expected: " COLOR_RESET); PRINT_VALUE(a); printf("\n"); \
            printf(COLOR_RED   "  Got: " COLOR_RESET); PRINT_VALUE(b); printf("\n"); \
            g_test_failed = true; \
            longjmp(g_test_jmp_buf, 1); \
        } \
    } while(0)

// Assert a != b.
// Do not use expressions with side-effects as expressions are evaluated multiple times.
// Expressionf of type [const] char* are compared with strcmp.
#define ASSERT_NEQ(a, b) \
    do { \
        if (CMP_FUNC(b)(a, b) == 0) { \
            printf(COLOR_BOLD_RED "ASSERTION FAILED: " COLOR_RESET); \
            printf(COLOR_CYAN #a " != " #b COLOR_RESET); \
            printf(" (at %s:%d)\n", __FILE__, __LINE__); \
            printf(COLOR_RED "  Both values: " COLOR_RESET); PRINT_VALUE(a); printf("\n"); \
            g_test_failed = true; \
            longjmp(g_test_jmp_buf, 1); \
        } \
    } while(0)

// Assert a < b.
// Do not use expressions with side-effects as expressions are evaluated multiple times.
// Expressionf of type [const] char* are compared with strcmp.
#define ASSERT_LT(a, b) \
    do { \
        if (CMP_FUNC(b)(a, b) >= 0) { \
            printf(COLOR_BOLD_RED "ASSERTION FAILED: " COLOR_RESET); \
            printf(COLOR_CYAN #a " < " #b COLOR_RESET); \
            printf(" (at %s:%d)\n", __FILE__, __LINE__); \
            printf(COLOR_GREEN "  Left: " COLOR_RESET); PRINT_VALUE(a); printf("\n"); \
            printf(COLOR_RED   "  Right: " COLOR_RESET); PRINT_VALUE(b); printf("\n"); \
            g_test_failed = true; \
            longjmp(g_test_jmp_buf, 1); \
        } \
    } while(0)

// Assert a <= b.
// Do not use expressions with side-effects as expressions are evaluated multiple times.
// Expressionf of type [const] char* are compared with strcmp.
#define ASSERT_LE(a, b) \
    do { \
        if (CMP_FUNC(b)(a, b) > 0) { \
            printf(COLOR_BOLD_RED "ASSERTION FAILED: " COLOR_RESET); \
            printf(COLOR_CYAN #a " <= " #b COLOR_RESET); \
            printf(" (at %s:%d)\n", __FILE__, __LINE__); \
            printf(COLOR_GREEN "  Left: " COLOR_RESET); PRINT_VALUE(a); printf("\n"); \
            printf(COLOR_RED   "  Right: " COLOR_RESET); PRINT_VALUE(b); printf("\n"); \
            g_test_failed = true; \
            longjmp(g_test_jmp_buf, 1); \
        } \
    } while(0)

// Assert a > b.
// Do not use expressions with side-effects as expressions are evaluated multiple times.
// Expressionf of type [const] char* are compared with strcmp.
#define ASSERT_GT(a, b) \
    do { \
        if (CMP_FUNC(b)(a, b) <= 0) { \
            printf(COLOR_BOLD_RED "ASSERTION FAILED: " COLOR_RESET); \
            printf(COLOR_CYAN #a " > " #b COLOR_RESET); \
            printf(" (at %s:%d)\n", __FILE__, __LINE__); \
            printf(COLOR_GREEN "  Left: " COLOR_RESET); PRINT_VALUE(a); printf("\n"); \
            printf(COLOR_RED   "  Right: " COLOR_RESET); PRINT_VALUE(b); printf("\n"); \
            g_test_failed = true; \
            longjmp(g_test_jmp_buf, 1); \
        } \
    } while(0)

// Assert a >= b.
// Do not use expressions with side-effects as expressions are evaluated multiple times.
// Expressionf of type [const] char* are compared with strcmp.
#define ASSERT_GE(a, b) \
    do { \
        if (CMP_FUNC(b)(a, b) < 0) { \
            printf(COLOR_BOLD_RED "ASSERTION FAILED: " COLOR_RESET); \
            printf(COLOR_CYAN #a " >= " #b COLOR_RESET); \
            printf(" (at %s:%d)\n", __FILE__, __LINE__); \
            printf(COLOR_GREEN "  Left: " COLOR_RESET); PRINT_VALUE(a); printf("\n"); \
            printf(COLOR_RED   "  Right: " COLOR_RESET); PRINT_VALUE(b); printf("\n"); \
            g_test_failed = true; \
            longjmp(g_test_jmp_buf, 1); \
        } \
    } while(0)

#endif
