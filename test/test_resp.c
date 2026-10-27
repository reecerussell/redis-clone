#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test.h"
#include "../src/resp.h"

// Parses a whole message from the top level (depth 0).
static RespParseResult parse(const char *input, RespNode_t **node, size_t *consumed)
{
    *node = NULL;
    *consumed = 0;
    return try_parse(input, strlen(input), 0, node, consumed);
}

#define CHECK_RESULT(input, expected)                  \
    do                                                 \
    {                                                  \
        RespNode_t *n_;                                \
        size_t c_;                                     \
        CHECK(parse((input), &n_, &c_) == (expected)); \
        resp_node_free(n_);                            \
    } while (0)

static RespNode_t *child(RespNode_t *node, long long i)
{
    return ((RespNode_t **)node->value)[i];
}

static void test_simple_string(void)
{
    printf("simple string\n");
    RespNode_t *node;
    size_t consumed;

    REQUIRE(parse("+hello world\r\n", &node, &consumed) == OK);
    CHECK(consumed == 14);
    CHECK(node->type == SIMPLE_STRING);
    CHECK(node->length == 11);
    CHECK_STR((char *)node->value, "hello world");
    resp_node_free(node);

    REQUIRE(parse("+\r\n", &node, &consumed) == OK);
    CHECK(consumed == 3);
    CHECK(node->length == 0);
    CHECK_STR((char *)node->value, "");
    resp_node_free(node);
}

static void test_long_simple_string(void)
{
    printf("long simple string\n");
    // Longer than a single 1kb read block.
    char input[2000];
    input[0] = '+';
    memset(input + 1, 'a', 1996);
    memcpy(input + 1997, "\r\n", 3);

    RespNode_t *node;
    size_t consumed;
    REQUIRE(parse(input, &node, &consumed) == OK);
    CHECK(consumed == 1999);
    CHECK(node->length == 1996);
    CHECK(strspn((char *)node->value, "a") == 1996);
    resp_node_free(node);
}

static void test_integer(void)
{
    printf("integer\n");
    RespNode_t *node;
    size_t consumed;

    REQUIRE(parse(":1234\r\n", &node, &consumed) == OK);
    CHECK(consumed == 7);
    CHECK(node->type == INTEGER);
    CHECK(*(long long *)node->value == 1234);
    resp_node_free(node);

    REQUIRE(parse(":-7\r\n", &node, &consumed) == OK);
    CHECK(*(long long *)node->value == -7);
    resp_node_free(node);

    CHECK_RESULT(":abc\r\n", ERROR);
    CHECK_RESULT(":\r\n", ERROR);
}

static void test_bulk_string(void)
{
    printf("bulk string\n");
    RespNode_t *node;
    size_t consumed;

    REQUIRE(parse("$5\r\nhello\r\n", &node, &consumed) == OK);
    CHECK(consumed == 11);
    CHECK(node->type == BULK_STRING);
    CHECK(node->length == 5);
    CHECK_STR((char *)node->value, "hello");
    resp_node_free(node);

    // Bulk strings are binary safe, so a CRLF in the payload is just data.
    REQUIRE(parse("$7\r\nhe\r\nllo\r\n", &node, &consumed) == OK);
    CHECK(node->length == 7);
    CHECK(memcmp(node->value, "he\r\nllo", 7) == 0);
    resp_node_free(node);

    REQUIRE(parse("$0\r\n\r\n", &node, &consumed) == OK);
    CHECK(consumed == 6);
    CHECK(node->length == 0);
    resp_node_free(node);

    CHECK_RESULT("$3\r\nhello\r\n", ERROR); // payload longer than specified
    CHECK_RESULT("$-2\r\n", ERROR);
    CHECK_RESULT("$\r\n", ERROR);
}

static void test_array(void)
{
    printf("array\n");
    RespNode_t *node;
    size_t consumed;

    REQUIRE(parse("*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n", &node, &consumed) == OK);
    CHECK(consumed == 31);
    CHECK(node->type == ARRAY);
    REQUIRE(node->length == 3);
    CHECK(child(node, 0)->type == BULK_STRING);
    CHECK_STR((char *)child(node, 0)->value, "SET");
    CHECK_STR((char *)child(node, 1)->value, "foo");
    CHECK_STR((char *)child(node, 2)->value, "bar");
    resp_node_free(node);

    REQUIRE(parse("*3\r\n+OK\r\n:42\r\n$2\r\nhi\r\n", &node, &consumed) == OK);
    REQUIRE(node->length == 3);
    CHECK(child(node, 0)->type == SIMPLE_STRING);
    CHECK(child(node, 1)->type == INTEGER);
    CHECK(child(node, 2)->type == BULK_STRING);
    resp_node_free(node);

    REQUIRE(parse("*0\r\n", &node, &consumed) == OK);
    CHECK(consumed == 4);
    CHECK(node->length == 0);
    resp_node_free(node);

    REQUIRE(parse("*-1\r\n", &node, &consumed) == OK); // null array
    CHECK(node->length == -1);
    CHECK(node->value == NULL);
    resp_node_free(node);

    CHECK_RESULT("*-2\r\n", ERROR);
    CHECK_RESULT("*\r\n", ERROR);
    CHECK_RESULT("*1\r\n!x\r\n", ERROR);              // unsupported element type
    CHECK_RESULT("*1048577\r\n", ERROR);              // over the element limit
    CHECK_RESULT("*1\r\n$10\r\nfoo\r\n", INCOMPLETE); // waiting on the rest of the bulk
}

static void test_depth(void)
{
    printf("depth\n");
    CHECK_RESULT("*1\r\n:1\r\n", OK);          // elements sit at depth 1
    CHECK_RESULT("*1\r\n*1\r\n:1\r\n", ERROR); // nested element at depth 2
    CHECK_RESULT("*1\r\n*1\r\n*1\r\n:1\r\n", ERROR);

    // A nested array is over the limit even with no elements to parse.
    CHECK_RESULT("*1\r\n*0\r\n", ERROR);
    CHECK_RESULT("*1\r\n*-1\r\n", ERROR);

    RespNode_t *node = NULL;
    size_t consumed = 0;
    CHECK(try_parse(":1\r\n", 4, 2, &node, &consumed) == ERROR);
    CHECK(try_parse(":1\r\n", 4, -1, &node, &consumed) == ERROR);
    CHECK(node == NULL);
}

static void test_incomplete(void)
{
    printf("incomplete\n");
    CHECK_RESULT("", INCOMPLETE);
    CHECK_RESULT("+OK", INCOMPLETE);
    CHECK_RESULT("+OK\r", INCOMPLETE);
    CHECK_RESULT("$5\r\nhel", INCOMPLETE);
    CHECK_RESULT("*2\r\n$3\r\nfoo\r\n", INCOMPLETE);

    // Feeding a command in one byte at a time, as a slow socket might.
    const char *input = "*1\r\n$4\r\nPING\r\n";
    size_t len = strlen(input);
    for (size_t n = 0; n < len; n++)
    {
        RespNode_t *node = NULL;
        size_t consumed = 0;
        RespParseResult res = try_parse(input, n, 0, &node, &consumed);
        if (res != INCOMPLETE)
        {
            FAILF("prefix of %zu/%zu bytes: expected INCOMPLETE but got %d", n, len, res);
        }
        resp_node_free(node);
    }
    CHECK_RESULT(input, OK);
}

static void test_pipelined(void)
{
    printf("pipelined\n");
    const char *input = "*1\r\n+a\r\n:42\r\n";
    RespNode_t *node;
    size_t consumed;

    REQUIRE(parse(input, &node, &consumed) == OK);
    CHECK(consumed == 8);
    CHECK(node->type == ARRAY);
    resp_node_free(node);

    REQUIRE(parse(input + consumed, &node, &consumed) == OK);
    CHECK(consumed == 5);
    CHECK(node->type == INTEGER);
    resp_node_free(node);
}

static void test_errors(void)
{
    printf("errors\n");
    CHECK_RESULT("!garbage", ERROR); // fails on the type byte without waiting for CRLF
    CHECK_RESULT("\r\n", ERROR);

    RespNode_t *node = NULL;
    size_t consumed = 0;
    CHECK(try_parse(NULL, 0, 0, &node, &consumed) == ERROR);
    CHECK(try_parse("+OK\r\n", 5, 0, NULL, &consumed) == ERROR);
    CHECK(try_parse("+OK\r\n", 5, 0, &node, NULL) == ERROR);
}

static void test_simple_string_node_to_string(void)
{
    printf("test_simple_string_node_to_string\n");

    char *value = "hello";
    RespNode_t node = {
        .type = SIMPLE_STRING,
        .length = 5,
        .value = value};

    char *result = resp_node_to_string(&node);
    CHECK_STR(result, "+hello\r\n");
    free(result);
}

static void test_bulk_string_node_to_string(void)
{
    printf("test_bulk_string_node_to_string\n");

    char *value = "hello";
    RespNode_t node = {
        .type = BULK_STRING,
        .length = 5,
        .value = value};

    char *result = resp_node_to_string(&node);
    CHECK_STR(result, "$5\r\nhello\r\n");
    free(result);
}

static void test_integer_node_to_string(void)
{
    printf("test_integer_node_to_string\n");

    long long value = 1234;
    RespNode_t node = {
        .type = INTEGER,
        .length = 0,
        .value = &value};

    // Positive integer
    char *result = resp_node_to_string(&node);
    CHECK_STR(result, ":1234\r\n");
    free(result);

    // Negative integer
    value = -1234;
    result = resp_node_to_string(&node);
    CHECK_STR(result, ":-1234\r\n");
    free(result);
}

static void test_array_node_to_string(void)
{
    printf("test_array_node_to_string\n");

    char *value = "hello";
    RespNode_t child = {
        .type = BULK_STRING,
        .length = 5,
        .value = value};

    RespNode_t *nodes[1];
    nodes[0] = &child;

    RespNode_t parent = {
        .type = ARRAY,
        .length = 1,
        .value = nodes};

    // Positive integer
    char *result = resp_node_to_string(&parent);
    CHECK_STR(result, "*1\r\n$5\r\nhello\r\n");
    free(result);
}

int main(void)
{
    test_simple_string();
    test_long_simple_string();
    test_integer();
    test_bulk_string();
    test_array();
    test_depth();
    test_incomplete();
    test_pipelined();
    test_errors();
    test_simple_string_node_to_string();
    test_bulk_string_node_to_string();
    test_integer_node_to_string();
    test_array_node_to_string();

    return TEST_SUMMARY();
}
