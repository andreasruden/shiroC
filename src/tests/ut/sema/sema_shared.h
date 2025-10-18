#ifndef UT_UTIL_SEMA__H
#define UT_UTIL_SEMA__H

#define ASSERT_SEMA_SUCCESS(node) \
    do { \
        bool res = semantic_analyzer_run((fix->sema), (node)); \
        ASSERT_TRUE(res); \
        ASSERT_EQ(0, vec_size(&(fix->ctx)->error_nodes)); \
    } while(0)

#define ASSERT_SEMA_SUCCESS_WITH_DECL_COLLECTOR(node) \
    do { \
        bool decl_res = decl_collector_run((fix->collector), (node)); \
        ASSERT_TRUE(decl_res); \
        bool res = semantic_analyzer_run((fix->sema), (node)); \
        ASSERT_TRUE(res); \
        ASSERT_EQ(0, vec_size(&(fix->ctx)->error_nodes)); \
    } while(0)

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

#define ASSERT_SEMA_ERROR_WITH_DECL_COLLECTOR(node, offender_node, error_substring) \
    do { \
        bool decl_res = decl_collector_run((fix->collector), (node)); \
        ASSERT_TRUE(decl_res); \
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

#define ASSERT_SEMA_WARNING_WITH_DECL_COLLECTOR(node, offender_node, warning_substring) \
    do { \
        bool decl_res = decl_collector_run((fix->collector), (node)); \
        ASSERT_TRUE(decl_res); \
        bool res = semantic_analyzer_run((fix->sema), (node)); \
        ASSERT_TRUE(res); \
        ASSERT_EQ(0, vec_size(&(fix->ctx)->error_nodes)); \
        ASSERT_EQ(1, vec_size(&(fix->ctx)->warning_nodes)); \
        ast_node_t* offender = vec_get(&(fix->ctx)->warning_nodes, 0); \
        ASSERT_EQ((offender_node), offender); \
        compiler_error_t* warning = vec_get(offender->errors, 0); \
        ASSERT_TRUE(warning->is_warning); \
        ASSERT_NEQ(nullptr, strstr(warning->description, (warning_substring))); \
    } while(0)

#endif
