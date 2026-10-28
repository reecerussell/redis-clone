#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "resp.h"

#define DELIMETER "\r\n"

// RESP2 Data Types
#define SIMPLE_STRING_KEY '+'
#define BULK_STRING_KEY '$'
#define INTEGER_KEY ':'
#define ARRAY_KEY '*'
#define ERROR_KEY '-'

#define BUFFER_BLOCK_SIZE 1024  // 1kb
#define MAX_READ_SIZE 536870912 // 512mb
#define MAX_ARRAY_LENGTH 1024 * 1024
#define MAX_DEPTH 1

void resp_node_free(RespNode_t *node)
{
    if (!node)
    {
        return;
    }

    if (node->value)
    {
        if (node->type == ARRAY)
        {
            RespNode_t **nodes = (RespNode_t **)node->value;
            for (long long i = 0; i < node->length; i++)
            {
                resp_node_free(nodes[i]);
            }
        }

        free(node->value);
    }

    free(node);
}

static int parse_int(const char *data, size_t length, long long *out)
{
    char *end;
    long long result = strtoll(data, &end, 10);
    if (data + length != end)
    {
        return -1;
    }

    *out = result;
    return 0;
}

static bool is_delimeter(const char *data, size_t index, size_t length)
{
    return data[index] == '\r' && index + 1 < length && data[index + 1] == '\n';
}

static int parse_node_type(const char c, RespNodeType *out)
{
    switch (c)
    {
    case SIMPLE_STRING_KEY:
        *out = SIMPLE_STRING;
        break;
    case INTEGER_KEY:
        *out = INTEGER;
        break;
    case BULK_STRING_KEY:
        *out = BULK_STRING;
        break;
    case ARRAY_KEY:
        *out = ARRAY;
        break;
    case ERROR_KEY:
        *out = RESP_ERROR;
        break;
    default:
        return -1;
    }
    return 0;
}

static int node_type_key(RespNodeType type, char *out)
{
    switch (type)
    {
    case SIMPLE_STRING:
        *out = SIMPLE_STRING_KEY;
        break;
    case INTEGER:
        *out = INTEGER_KEY;
        break;
    case BULK_STRING:
        *out = BULK_STRING_KEY;
        break;
    case ARRAY:
        *out = ARRAY_KEY;
        break;
    case RESP_ERROR:
        *out = ERROR_KEY;
        break;
    default:
        return -1;
    }
    return 0;
}

RespParseResult try_parse(const char *data, size_t length, int depth, RespNode_t **out, size_t *bytes_consumed)
{
    if (!data || !out || !bytes_consumed || depth < 0 || depth > MAX_DEPTH || length > MAX_READ_SIZE)
    {
        return ERROR;
    }

    if (length < 1)
    {
        return INCOMPLETE;
    }

    RespNodeType type;
    if (parse_node_type(data[0], &type) != 0)
    {
        return ERROR;
    }

    size_t start_index = 1;
    for (size_t i = start_index; i < length; i++)
    {
        if (!is_delimeter(data, i, length))
        {
            continue;
        }

        size_t current_line_length = i - start_index;
        if (current_line_length == 0 && type != SIMPLE_STRING)
        {
            return ERROR;
        }

        const char *current_line = data + start_index;

        RespNode_t *node = calloc(1, sizeof(RespNode_t));
        if (!node)
        {
            return ERROR;
        }

        node->type = type;

        if (type == SIMPLE_STRING || type == RESP_ERROR)
        {
            node->length = current_line_length;
            node->value = strndup(current_line, current_line_length);
            *out = node;
            *bytes_consumed = i + 2;
            return OK;
        }

        if (type == INTEGER)
        {
            long long *value = malloc(sizeof(long long));
            if (!value)
            {
                resp_node_free(node);
                return ERROR;
            }

            if (parse_int(current_line, current_line_length, value) != 0)
            {
                resp_node_free(node);
                free(value);
                return ERROR;
            }

            node->length = 0;
            node->value = value;
            *out = node;
            *bytes_consumed = i + 2;
            return OK;
        }

        if (type == BULK_STRING)
        {
            long long bulk_length;
            if (parse_int(current_line, current_line_length, &bulk_length) != 0)
            {
                resp_node_free(node);
                return ERROR;
            }

            // Move indexes forward to the bulk string payload
            start_index = i + 2;

            if (bulk_length < 0)
            {
                resp_node_free(node);
                return ERROR;
            }

            if (bulk_length + 2 > (long long)(length - start_index))
            {
                resp_node_free(node);
                return INCOMPLETE;
            }

            const char *bulk_data = data + start_index;
            if (bulk_data[bulk_length] != '\r' || bulk_data[bulk_length + 1] != '\n')
            {
                resp_node_free(node);
                return ERROR;
            }

            node->length = bulk_length;
            node->value = strndup(bulk_data, bulk_length);
            *out = node;
            *bytes_consumed = start_index + bulk_length + 2;
            return OK;
        }

        if (type == ARRAY)
        {
            long long array_length;
            if (parse_int(current_line, current_line_length, &array_length) != 0)
            {
                resp_node_free(node);
                return ERROR;
            }

            if (array_length < -1 || array_length > MAX_ARRAY_LENGTH || depth >= MAX_DEPTH)
            {
                resp_node_free(node);
                return ERROR;
            }

            node->length = array_length;
            if (array_length < 1)
            {
                node->value = NULL;
                *out = node;
                *bytes_consumed = i + 2;
                return OK;
            }

            RespNode_t **nodes = calloc(array_length, sizeof(RespNode_t *));
            if (!nodes)
            {
                resp_node_free(node);
                return ERROR;
            }
            node->value = nodes;

            // Move indexes forward to the first array item
            start_index = i + 2;

            for (long long j = 0; j < array_length; j++)
            {
                RespNode_t *subnode;
                size_t sub_bytes_read;
                RespParseResult res = try_parse(data + start_index, length - start_index, depth + 1, &subnode, &sub_bytes_read);
                if (res != OK)
                {
                    resp_node_free(node);
                    return res;
                }
                nodes[j] = subnode;
                start_index += sub_bytes_read;
            }

            *out = node;
            *bytes_consumed = start_index;

            return OK;
        }
    }

    return INCOMPLETE;
}

const char *resp_node_type_name(RespNodeType type)
{
    switch (type)
    {
    case SIMPLE_STRING:
        return "Simple String";
    case BULK_STRING:
        return "Bulk String";
    case ARRAY:
        return "Array";
    case INTEGER:
        return "Integer";
    default:
        return "Unknown";
    }
}

static int length_of_integer(long long value)
{
    int n = 1;

    if (value < 0)
    {
        n++;
        value *= -1;
    }

    while (value >= 10)
    {
        n++;
        value /= 10;
    }

    return n;
}

static long long calculate_node_string_length(RespNode_t *node)
{
    const long long delim_length = 2;
    long long length = 1 + delim_length; // starting key + ending delim;

    switch (node->type)
    {
    case SIMPLE_STRING:
    case RESP_ERROR:
        return length + node->length;
    case BULK_STRING:
        return length + length_of_integer(node->length) + delim_length + node->length;
    case INTEGER:
        return length + length_of_integer(*((long long *)node->value));
    case ARRAY:
    {
        length += length_of_integer(node->length) + delim_length;
        RespNode_t **nodes = (RespNode_t **)node->value;
        for (long long i = 0; i < node->length; i++)
        {
            length += calculate_node_string_length(nodes[i]);
        }
        return length;
    }
    default:
        return -1;
    }
}

static int write_node_to_string(RespNode_t *node, char *result, long long *written)
{
    if (node_type_key(node->type, result) != 0)
    {
        return -1;
    }

    long long n = 1;
    switch (node->type)
    {
    case SIMPLE_STRING:
    case RESP_ERROR:
        memcpy(result + n, (char *)node->value, node->length);
        n += node->length;
        break;
    case BULK_STRING:
        n += sprintf(result + n, "%lld", node->length);
        result[n++] = '\r';
        result[n++] = '\n';
        memcpy(result + n, (char *)node->value, node->length);
        n += node->length;
        break;
    case INTEGER:
        n += sprintf(result + n, "%lld", *((long long *)node->value));
        break;
    case ARRAY:
    {
        n += sprintf(result + n, "%lld", node->length);
        result[n++] = '\r';
        result[n++] = '\n';
        RespNode_t **nodes = (RespNode_t **)node->value;
        for (long long i = 0; i < node->length; i++)
        {
            long long sub_written;
            if (write_node_to_string(nodes[i], result + n, &sub_written) != 0)
            {
                return -1;
            }
            n += sub_written;
        }
        break;
    }
    default:
        return -1;
    }

    if (node->type != ARRAY)
    {
        result[n++] = '\r';
        result[n++] = '\n';
    }

    *written = n;
    return 0;
}

char *resp_node_to_string(RespNode_t *node)
{
    if (!node)
    {
        return NULL;
    }

    const long long length = calculate_node_string_length(node) + 1;
    char *result = malloc(sizeof(char) * length);
    if (!result)
    {
        return NULL;
    }

    long long written;
    if (write_node_to_string(node, result, &written) != 0)
    {
        free(result);
        return NULL;
    }

    result[written] = '\0';
    return result;
}

// Helpers

RespNode_t *resp_child_node_at(RespNode_t *node, size_t index)
{
    if (!node || node->type != ARRAY || (long long)index >= node->length)
    {
        return NULL;
    }

    RespNode_t **nodes = (RespNode_t **)node->value;
    return nodes[index];
}

RespNode_t *resp_create_string(char *value)
{
    RespNode_t *node = malloc(sizeof(RespNode_t));
    if (!node)
    {
        return NULL;
    }
    node->type = BULK_STRING;
    node->length = strlen(value);
    node->value = strdup(value);
    return node;
}

RespNode_t *resp_create_error(char *value)
{
    RespNode_t *node = malloc(sizeof(RespNode_t));
    if (!node)
    {
        return NULL;
    }
    node->type = RESP_ERROR;
    node->length = strlen(value);
    node->value = strdup(value);
    return node;
}

RespNode_t *resp_create_integer(long long value)
{
    RespNode_t *node = malloc(sizeof(RespNode_t));
    if (!node)
    {
        return NULL;
    }
    long long *forward_value = malloc(sizeof(long long));
    if (!forward_value)
    {
        free(node);
        return NULL;
    }
    *forward_value = value;
    node->type = INTEGER;
    node->length = 0;
    node->value = forward_value;
    return node;
}

RespNode_t *resp_create_array(size_t len, ...)
{
    RespNode_t **nodes = calloc(len, sizeof(RespNode_t *));
    if (!nodes)
    {
        return NULL;
    }

    va_list args;
    va_start(args, len);

    for (size_t i = 0; i < len; i++)
    {
        RespNode_t *node = (RespNode_t *)va_arg(args, RespNode_t *);
        if (!node)
        {
            free(nodes);
            return NULL;
        }
        nodes[i] = node;
    }

    va_end(args);

    RespNode_t *node = malloc(sizeof(RespNode_t));
    if (!node)
    {
        free(nodes);
        return NULL;
    }
    node->type = ARRAY;
    node->length = (long long)len;
    node->value = nodes;
    return node;
}