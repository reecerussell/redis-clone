#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "../include/resp.h"
#include "../include/buffer.h"

#define DELIMETER "\r\n"

// RESP2 Data Types
#define SIMPLE_STRING_KEY '+'
#define BULK_STRING_KEY '$'
#define INTEGER_KEY ':'
#define ARRAY_KEY '*'
#define ERROR_KEY '-'

#define BUFFER_BLOCK_SIZE 1024  // 1kb
#define MAX_READ_SIZE 536870912 // 512mb

static int map_resp_type_enum(char c, RespNodeType *type)
{
    switch (c)
    {
    case SIMPLE_STRING_KEY:
        *type = SIMPLE_STRING;
        break;
    case BULK_STRING_KEY:
        *type = BULK_STRING;
        break;
    case ARRAY_KEY:
        *type = ARRAY;
        break;
    case INTEGER_KEY:
        *type = INTEGER;
        break;
    default:
        return -1;
    }

    return 0;
}

static int read_node_type(buffer_t *buf, RespNodeType *type)
{
    char data[2];
    data[1] = '\0';

    if (buf_read(buf, data, 1) < 1)
    {
        return -1;
    }

    return map_resp_type_enum(data[0], type);
}

static int read_integer(buffer_t *buf, ssize_t *out)
{
    char data[23]; // 22 for 19 digits of int64 + delimeter + terminator + optional '-'
    bzero(data, 23);

    size_t start_position = buf_position(buf);
    if (buf_read(buf, data, 22) < 1)
    {
        return -1;
    }

    ssize_t n = 0;
    size_t i = 0;
    bool is_negative = false;
    char p[2];
    p[1] = '\0';

    for (; i < 22; i++)
    {
        char c = data[i];
        if (i == 0 && c == '-')
        {
            is_negative = true;
            continue;
        }

        if (!isdigit(c))
        {
            break;
        }

        p[0] = c;

        n *= 10;
        n += atoi(p); // TODO: optimise using char values
    }

    if (is_negative)
    {
        n *= -1;
    }

    *out = n;

    // Set buffer position to the next char after the read length + delimeter.
    buf_seek(buf, start_position + i + 2);

    return 0;
}

// Finds the index of the first DELIMITER in the input. Returns -1 if not found.
// This isn't optimised for dynamic DELIMETER values.
static int index_of_delimeter(const char *input, size_t len)
{
    if (!input || len < 2)
    {
        return -1;
    }

    for (size_t i = 0; i < len; i++)
    {
        if (input[i] == '\r' && i + 1 < len && input[i + 1] == '\n')
        {
            return i;
        }
    }

    return -1;
}

// Finds the index of the last occuring DELIMETER in the input. Returns -1 if not found.
// This isn't optimised for dynamic DELIMETER values.
static int last_index_of_delimeter(const char *input, size_t len)
{
    if (!input || len < 2)
    {
        return -1;
    }

    for (ssize_t i = ((ssize_t)len) - 1; i > 0; i--)
    {
        if (input[i] == '\n' && i - 1 >= 0 && input[i - 1] == '\r')
        {
            return i - 1;
        }
    }

    return -1;
}

static int read_string(buffer_t *buf, char **out, int len)
{
    ssize_t bytes_requested = len + 2;                         // len + delimeter
    char *data = malloc(sizeof(char) * (bytes_requested + 1)); // + terminator
    if (!data)
    {
        return -1;
    }
    bzero(data, bytes_requested + 1);

    ssize_t bytes_read = buf_read(buf, data, bytes_requested);
    if (bytes_read != bytes_requested || last_index_of_delimeter(data, bytes_requested) != len)
    { // Data size mismatch
        free(data);
        return -1;
    }
    bzero(&data[len], 3); // Terminate at the end of the content
    *out = data;
    return 0;
}

static int read_simple_string(buffer_t *buf, char **out)
{
    size_t cap = BUFFER_BLOCK_SIZE;
    char *data = malloc(sizeof(char) * cap);
    if (!data)
    {
        return -1;
    }
    bzero(data, cap);

    ssize_t total_bytes = 0;
    ssize_t next_read_block_size = cap - 1;

    do
    {
        ssize_t bytes_read = buf_read(buf, data + total_bytes, (size_t)next_read_block_size);
        if (bytes_read <= 0)
        {
            break;
        }

        total_bytes += bytes_read;

        int delim_idx = index_of_delimeter(data, cap);
        if (delim_idx > -1)
        {
            bzero(data + delim_idx, cap - delim_idx);
            *out = data;
            return 0;
        }

        if (bytes_read == next_read_block_size)
        {
            next_read_block_size = (cap * 2) - cap;
            cap *= 2;
            char *grown = realloc(data, cap);
            if (!grown)
            {
                free(data);
                return -1;
            }
            data = grown;
        }
    } while (total_bytes < MAX_READ_SIZE);

    free(data);

    return -1;
}

void resp_node_free(RespNode_t *node)
{
    if (!node)
    {
        return;
    }

    if (node->value)
    {
        switch (node->type)
        {
        case SIMPLE_STRING:
        case BULK_STRING:
        case INTEGER:
            free(node->value);
            break;
        case ARRAY:
        {
            RespNode_t **nodes = (RespNode_t **)node->value;
            for (size_t i = 0; i < node->length; i++)
            {
                resp_node_free(nodes[i]);
            }
            free(node->value);
        }
        }
    }

    free(node);
}

int resp_parse(RespNode_t **target, buffer_t *buf)
{
    RespNode_t *node = malloc(sizeof(RespNode_t));
    if (!node)
    {
        return -1;
    }
    node->type = SIMPLE_STRING;
    node->value = NULL;

    if (read_node_type(buf, &node->type) != 0)
    {
        free(node);
        return -1;
    }

    switch (node->type)
    {
    case SIMPLE_STRING:
    {
        char *value;
        if (read_simple_string(buf, &value) != 0)
        {
            free(node);
            return -1;
        }
        node->value = (void *)value;
        node->length = strlen(value);
        break;
    }
    case BULK_STRING:
    {
        ssize_t length = 0;
        if (read_integer(buf, &length) != 0)
        {
            free(node);
            return -1;
        }
        char *value;
        if (read_string(buf, &value, (int)length) != 0)
        {
            free(node);
            return -1;
        }
        node->length = (size_t)length;
        node->value = (void *)value;
        break;
    }
    case INTEGER:
    {
        ssize_t *value = malloc(sizeof(ssize_t));
        if (!value)
        {
            free(node);
            return -1;
        }
        if (read_integer(buf, value) != 0)
        {
            free(value);
            free(node);
            return -1;
        }
        node->value = (void *)value;
        break;
    }
    case ARRAY:
    {
        ssize_t length = 0;
        if (read_integer(buf, &length) != 0 || length < 0)
        {
            free(node);
            return -1;
        }
        node->length = (size_t)length;
        RespNode_t **nodes = malloc(sizeof(RespNode_t *) * length);
        if (!nodes)
        {
            free(node);
            return -1;
        }
        for (ssize_t i = 0; i < length; i++)
        {
            if (resp_parse(&nodes[i], buf) != 0)
            {
                for (ssize_t j = 0; j < i; j++)
                {
                    resp_node_free(nodes[j]);
                }
                free(nodes);
                free(node);
                return -1;
            }
        }
        node->value = (void *)nodes;
        break;
    }
    default:
    {
        printf("ERROR: unable to parse unsupported resp data type\n");
        free(node);
        return -1;
    }
    }

    *target = node;
    return 0;
}

int try_resp_parse(RespNode_t **target, buffer_t *buf)
{
    int ec;
    while ((ec = resp_parse(target, buf)) != 0)
    {
        buf_seek(buf, 0);
    }
    return ec;
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