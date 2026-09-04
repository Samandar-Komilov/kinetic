#ifndef KTC_TEST_HARNESS_H
#define KTC_TEST_HARNESS_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run = 0;
static int g_tests_failed = 0;
static const char *g_current_rfc = "";
static const char *g_current_test = "";

#define KTC_TEST_SUITE_START(suite_name) printf("\n=== Running Test Suite: %s ===\n", suite_name)

#define KTC_TEST_CASE(rfc_id, test_name)                                                           \
    do {                                                                                           \
        g_current_rfc = rfc_id;                                                                    \
        g_current_test = test_name;                                                                \
    } while (0)

#define KTC_ASSERT(condition, msg)                                                                 \
    do {                                                                                           \
        g_tests_run++;                                                                             \
        if (!(condition)) {                                                                        \
            g_tests_failed++;                                                                      \
            fprintf(stderr, "  [FAIL] [%s] %s -> %s (%s:%d)\n", g_current_rfc, g_current_test,     \
                    msg, __FILE__, __LINE__);                                                      \
        } else {                                                                                   \
            printf("  [PASS] [%s] %s -> %s\n", g_current_rfc, g_current_test, msg);                \
        }                                                                                          \
    } while (0)

#define KTC_ASSERT_TRUE(condition) KTC_ASSERT(condition, #condition " is TRUE")
#define KTC_ASSERT_FALSE(condition) KTC_ASSERT(!(condition), #condition " is FALSE")

#define KTC_TEST_SUITE_END()                                                                       \
    do {                                                                                           \
        printf("\n--- Test Suite Summary: %d run, %d failed ---\n", g_tests_run, g_tests_failed);  \
        return (g_tests_failed == 0) ? 0 : 1;                                                      \
    } while (0)

#endif
