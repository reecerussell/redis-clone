#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "../include/resp.h"
#include "../include/buffer.h"

int create_in_memory_buffer(const char *data)
{
    FILE *f = tmpfile();
    if (!f)
    {
        printf("ERROR: failed to create tmp file\n");
        exit(1);
    }

    int fd = dup(fileno(f));
    fclose(f);

    if (fd == -1)
    {
        printf("ERROR: failed to dupe fd\n");
        exit(1);
    }

    size_t len = strlen(data);
    if (write(fd, data, len) != (ssize_t)len)
    {
        close(fd);
        printf("ERROR: failed to write to tmp file\n");
        exit(1);
    }

    lseek(fd, 0, SEEK_SET);
    return fd;
}

int assert_parsing_fails(const char *input)
{
    int fd = create_in_memory_buffer(input);
    buffer_t *buf = buf_create(fd);

    RespNode_t *node = NULL;
    int ec = resp_parse(&node, buf);

    close(fd);
    buf_free(buf);
    resp_node_free(node);

    return ec;
}

int test_simple_bulk_string()
{
    printf("RUNNING: test_simple_bulk_string\n");

    char input[] = "$5\r\nhello\r\n";
    int fd = create_in_memory_buffer(input);
    buffer_t *buf = buf_create(fd);

    RespNode_t *node;
    int ec;
    if ((ec = resp_parse(&node, buf)) != 0)
    {
        printf("FAIL: test_simple_bulk_string -> failed to parse node\n");
    }

    if (ec == 0 && !node)
    {
        printf("FAIL: test_simple_bulk_string -> expected node to not be null\n");
        ec = -1;
    }

    if (ec == 0 && node->type != BULK_STRING)
    {
        printf("FAIL: test_simple_bulk_string -> expected type BULK_STRING but got %s\n", resp_node_type_name(node->type));
        ec = -1;
    }

    if (ec == 0 && node->length != 5)
    {
        printf("FAIL: test_simple_bulk_string -> expected length of 5 but got '%ld'\n", node->length);
        ec = -1;
    }

    if (ec == 0 && strcmp((char *)node->value, "hello") != 0)
    {
        printf("FAIL: test_simple_bulk_string -> expected value of 'hello' but got '%s'\n", (char *)node->value);
        ec = -1;
    }

    if (ec == 0)
    {
        printf("PASSED: test_simple_bulk_string\n");
    }
    else
    {
        printf("FAILED: test_simple_bulk_string\n");
    }

    close(fd);
    buf_free(buf);
    resp_node_free(node);

    return ec;
}

int test_edge_cases_simple_bulk_strings()
{
    printf("RUNNING: test_edge_cases_simple_bulk_strings\n");

    int ec = 0;
    printf("\t> data longer than specified\n");
    if (assert_parsing_fails("$3\r\nhello\r\n") == 0)
    { // Data is longer than specified
        printf("FAIL: test_edge_cases_simple_bulk_strings -> data longer than specified should fail\n");
        ec = -1;
    }

    printf("\t> data shorter than specified\n");
    if (assert_parsing_fails("$5\r\nhel\r\n") == 0)
    { // Data is shorter than specified
        printf("FAIL: test_edge_cases_simple_bulk_strings -> data shorter than specified should fail\n");
        ec = -1;
    }

    printf("\t> data containing CRLF\n");
    if (assert_parsing_fails("$7\r\nhe\r\nllo\r\n") != 0)
    { // Data contains CRLF
        printf("FAIL: test_edge_cases_simple_bulk_strings -> data containing CRLF should not fail\n");
        ec = -1;
    }

    printf("\t> truncated data\n");
    if (assert_parsing_fails("$7\r\nhe") == 0)
    { // Data contains CRLF
        printf("FAIL: test_edge_cases_simple_bulk_strings -> truncated data should fail\n");
        ec = -1;
    }

    if (ec == 0)
    {
        printf("PASSED: test_negative_simple_bulk_strings\n");
    }
    else
    {
        printf("FAILED: test_negative_simple_bulk_strings\n");
    }

    return ec;
}

int test_simple_string()
{
    printf("RUNNING: test_simple_string\n");

    char input[] = "+hello world\r\n";
    int fd = create_in_memory_buffer(input);
    buffer_t *buf = buf_create(fd);

    RespNode_t *node;
    int ec;
    if ((ec = resp_parse(&node, buf)) != 0)
    {
        printf("FAIL: test_simple_string -> failed to parse node\n");
    }

    if (ec == 0 && !node)
    {
        printf("FAIL: test_simple_string -> expected node to not be null\n");
        ec = -1;
    }

    if (ec == 0 && node->type != SIMPLE_STRING)
    {
        printf("FAIL: test_simple_string -> expected type SIMPLE_STRING but got %s\n", resp_node_type_name(node->type));
        ec = -1;
    }

    if (ec == 0 && node->length != 11)
    {
        printf("FAIL: test_simple_string -> expected length of 11 but got '%ld'\n", node->length);
        ec = -1;
    }

    if (ec == 0 && strcmp((char *)node->value, "hello world") != 0)
    {
        printf("FAIL: test_simple_string -> expected value of 'hello world' but got '%s'\n", (char *)node->value);
        ec = -1;
    }

    if (ec == 0)
    {
        printf("PASSED: test_simple_string\n");
    }
    else
    {
        printf("FAILED: test_simple_string\n");
    }

    close(fd);
    buf_free(buf);
    resp_node_free(node);

    return ec;
}

int test_long_simple_string()
{
    printf("RUNNING: test_long_simple_string\n");

    char *input = malloc(sizeof(char) * 2000);
    if (!input)
    {
        printf("ERROR: test_long_simple_string -> failed to allocate input string");
        return -1;
    }

    input[0] = '+';
    input[1997] = '\r';
    input[1998] = '\n';
    input[1999] = '\0';

    for (int i = 1; i < 1997; i++)
    {
        input[i] = 'a';
    }

    int fd = create_in_memory_buffer(input);
    buffer_t *buf = buf_create(fd);

    RespNode_t *node;
    int ec;
    if ((ec = resp_parse(&node, buf)) != 0)
    {
        printf("FAIL: test_long_simple_string -> failed to parse node\n");
    }

    if (ec == 0 && !node)
    {
        printf("FAIL: test_long_simple_string -> expected node to not be null\n");
        ec = -1;
    }

    if (ec == 0 && node->type != SIMPLE_STRING)
    {
        printf("FAIL: test_long_simple_string -> expected type SIMPLE_STRING but got %s\n", resp_node_type_name(node->type));
        ec = -1;
    }

    if (ec == 0 && node->length != 1996)
    {
        printf("FAIL: test_long_simple_string -> expected length of 1996 but got '%ld'\n", node->length);
        ec = -1;
    }

    if (ec == 0)
    {
        for (size_t i = 0; i < node->length; i++)
        {
            char c = ((char *)node->value)[i];
            if (c != 'a')
            {
                printf("FAIL: test_long_simple_string -> expected value at index '%ld' to be 'a' but got '%hhd'\n", i, c);
                ec = -1;
            }
        }
    }

    if (ec == 0)
    {
        printf("PASSED: test_long_simple_string\n");
    }
    else
    {
        printf("FAILED: test_long_simple_string\n");
    }

    free(input);
    close(fd);
    buf_free(buf);
    resp_node_free(node);

    return ec;
}

// Tests giving a buffer over 512mb fails
int test_too_long_simple_string()
{
    printf("RUNNING: test_too_long_simple_string\n");

    char *input = malloc(sizeof(char) * 536870916);
    if (!input)
    {
        printf("ERROR: test_too_long_simple_string -> failed to allocate input string");
        return -1;
    }

    input[0] = '+';
    input[536870913] = '\r';
    input[536870914] = '\n';
    input[536870915] = '\0';

    for (int i = 1; i < 536870912; i++)
    {
        input[i] = 'a';
    }

    int ec = 0;
    if ((ec = assert_parsing_fails(input)) == 0)
    {
        printf("FAIL: test_too_long_simple_string -> should fail to parse node\n");
    }
    else
    {
        printf("PASSED: test_too_long_simple_string\n");
    }

    free(input);

    return ec == 0 ? -1 : 0;
}

int test_integer()
{
    printf("RUNNING: test_integer\n");

    char input[] = ":1234\r\n";
    int fd = create_in_memory_buffer(input);
    buffer_t *buf = buf_create(fd);

    RespNode_t *node;
    int ec;
    if ((ec = resp_parse(&node, buf)) != 0)
    {
        printf("FAIL: test_integer -> failed to parse node\n");
    }

    if (ec == 0 && !node)
    {
        printf("FAIL: test_integer -> expected node to not be null\n");
        ec = -1;
    }

    if (ec == 0 && node->type != INTEGER)
    {
        printf("FAIL: test_integer -> expected type INTEGER but got %s\n", resp_node_type_name(node->type));
        ec = -1;
    }

    if (ec == 0 && *(int *)node->value != 1234)
    {
        printf("FAIL: test_integer -> expected value of 1234 but got '%d'\n", *(int *)node->value);
        ec = -1;
    }

    if (ec == 0)
    {
        printf("PASSED: test_integer\n");
    }
    else
    {
        printf("FAILED: test_integer\n");
    }

    close(fd);
    buf_free(buf);
    resp_node_free(node);

    return ec;
}

int test_bulk_string_array()
{
    printf("RUNNING: test_bulk_string_array\n");

    char input[] = "*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    int fd = create_in_memory_buffer(input);
    buffer_t *buf = buf_create(fd);

    RespNode_t *node;
    int ec;
    if ((ec = resp_parse(&node, buf)) != 0)
    {
        printf("FAIL: test_bulk_string_array -> failed to parse node\n");
    }

    if (ec == 0 && !node)
    {
        printf("FAIL: test_bulk_string_array -> expected node to not be null\n");
        ec = -1;
    }

    if (ec == 0 && node->type != ARRAY)
    {
        printf("FAIL: test_bulk_string_array -> expected type ARRAY but got %s\n", resp_node_type_name(node->type));
        ec = -1;
    }

    if (ec == 0 && node->length != 2)
    {
        printf("FAIL: test_bulk_string_array -> expected length of 2 but got '%ld'\n", node->length);
        ec = -1;
    }

    RespNode_t **nodes = (RespNode_t **)node->value;
    if (ec == 0 && nodes[0]->type != BULK_STRING)
    {
        printf("FAIL: test_bulk_string_array -> node[0] expected type BULK_STRING but got %s\n", resp_node_type_name(nodes[0]->type));
        ec = -1;
    }

    if (ec == 0 && nodes[0]->length != 3)
    {
        printf("FAIL: test_bulk_string_array -> node[0] expected type ARRAY but got %s\n", resp_node_type_name(nodes[0]->type));
        ec = -1;
    }

    if (ec == 0 && strcmp((char *)nodes[0]->value, "foo") != 0)
    {
        printf("FAIL: test_bulk_string_array -> node[0] expected value of 'foo' but got '%s'\n", (char *)node->value);
        ec = -1;
    }

    if (ec == 0 && nodes[1]->type != BULK_STRING)
    {
        printf("FAIL: test_bulk_string_array -> node[1] expected type BULK_STRING but got %s\n", resp_node_type_name(nodes[0]->type));
        ec = -1;
    }

    if (ec == 0 && nodes[1]->length != 3)
    {
        printf("FAIL: test_bulk_string_array -> node[1] expected type ARRAY but got %s\n", resp_node_type_name(nodes[0]->type));
        ec = -1;
    }

    if (ec == 0 && strcmp((char *)nodes[1]->value, "bar") != 0)
    {
        printf("FAIL: test_bulk_string_array -> node[1] expected value of 'bar' but got '%s'\n", (char *)node->value);
        ec = -1;
    }

    if (ec == 0)
    {
        printf("PASSED: test_bulk_string_array\n");
    }

    close(fd);
    buf_free(buf);
    resp_node_free(node);

    return ec;
}

int main(void)
{
    int success = 0, cases = 7;
    success += test_simple_bulk_string() == 0 ? 1 : 0;
    success += test_edge_cases_simple_bulk_strings() == 0 ? 1 : 0;
    success += test_simple_string() == 0 ? 1 : 0;
    success += test_long_simple_string() == 0 ? 1 : 0;
    success += test_too_long_simple_string() == 0 ? 1 : 0;
    success += test_integer() == 0 ? 1 : 0;
    success += test_bulk_string_array() == 0 ? 1 : 0;

    if (success == cases)
    {
        printf("ALL TESTS PASSED\n");
    }
    else
    {
        printf("SUCCESS: %d, FAIL: %d\n", success, cases - success);
    }

    return success == cases ? 0 : 1;
}