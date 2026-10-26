#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

static int checks = 0;
static int failures = 0;

#define CHECK(cond)                                                   \
    do                                                                \
    {                                                                 \
        checks++;                                                     \
        if (!(cond))                                                  \
        {                                                             \
            printf("  FAIL %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
            failures++;                                               \
        }                                                             \
    } while (0)

// This is like CHECK, but return instead of carrying on. It's useful
// for tests where future cases might cause a null-deref.
#define REQUIRE(cond)                     \
    do                                    \
    {                                     \
        int failures_before_ = failures;  \
        CHECK(cond);                      \
        if (failures != failures_before_) \
        {                                 \
            return;                       \
        }                                 \
    } while (0)

#define CHECK_STR(actual, expected)                     \
    do                                                  \
    {                                                   \
        const char *a_ = (actual);                      \
        CHECK(a_ != NULL && strcmp(a_, expected) == 0); \
    } while (0)

#define FAILF(...)           \
    do                       \
    {                        \
        printf("  FAIL ");   \
        printf(__VA_ARGS__); \
        printf("\n");        \
        failures++;          \
    } while (0)

#define TEST_SUMMARY() \
    (printf("\n%d checks, %d failed\n", checks, failures), failures ? 1 : 0)

#endif
