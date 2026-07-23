#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdio.h>
#include <stdint.h>

static int failures;

#define CHECK(msg, cond) \
    do { \
        if (!(cond)) { \
            printf("  FAIL: %s\n", msg); \
            failures++; \
        } \
    } while (0)

#define CHECK_VALUE(name, actual, expected) \
    do { \
        if ((uint32_t)(actual) != (uint32_t)(expected)) { \
            printf("FAIL: %s actual=%lu expected=%lu\n", \
                   (name), (unsigned long)(uint32_t)(actual), (unsigned long)(uint32_t)(expected)); \
            failures++; \
        } \
    } while (0)

#define TEST_RESULT(name, failures) \
    do { \
        if ((failures) == 0) { \
            printf("%s: PASSED\n", (name)); \
        } else { \
            printf("%s: FAILED (%d failures)\n", (name), (failures)); \
        } \
    } while (0)

#endif /* TEST_HELPERS_H */
