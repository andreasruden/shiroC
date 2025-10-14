#ifndef UT_UTIL_SEMA__H
#define UT_UTIL_SEMA__H

#define ASSERT_SEMA_ERROR(node, offender_node, error_substring) \
    do { \
        bool res = semantic_analyzer_run((fix->sema), (node)); \
        ASSERT_FALSE(res); \
        ASSERT_LT(0, vec_size(&(fix->ctx)->error_nodes)); \
        ast_node_t* offender = vec_get(&(fix->ctx)->error_nodes, 0); \
        ASSERT_EQ((offender_node), offender); \
        compiler_error_t* error = vec_get(offender->errors, 0); \
        ASSERT_NEQ(nullptr, strstr(error->description, (error_substring))); \
    } while(0)

#define ASSERT_SEMA_WARNING(node, offender_node, warning_substring) \
    do { \
        bool res = semantic_analyzer_run((fix->sema), (node)); \
        ASSERT_FALSE(res); \
        ASSERT_LT(0, vec_size(&(fix->ctx)->warning_nodes)); \
        ast_node_t* offender = vec_get(&(fix->ctx)->error_nodes, 0); \
        ASSERT_EQ((offender_node), offender); \
        compiler_error_t* error = vec_get(offender->errors, 0); \
        ASSERT_NEQ(nullptr, strstr(error->description, (warning_substring))); \
    } while(0)

#endif
