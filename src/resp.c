#include <stdio.h>
#include <string.h>
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

// Line represents a line substring in a larger string, but as
// a "view" into it rather than a copy of the data.
typedef struct
{
    const char *data;
    size_t len;
} line_t;

void free_lines(line_t **lines, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (lines[i])
        {
            free(lines[i]);
        }
    }
    free(lines);
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

RespParseResult split_lines(const char *data, size_t length, line_t ***out, size_t *count)
{
    // Estimate the line count from how many delimeters are found.
    // This doesn't account for CRLFs found in bulk strings, so can
    // over estimate. The line index is used as the final count.
    size_t line_count = 0;
    for (size_t i = 0; i < length; i++)
    {
        if (is_delimeter(data, i, length))
        {
            line_count++;
            i++;
        }
    }

    // Assume if we haven't found the line count yet, the data is incomplete.
    if (line_count < 1)
    {
        return INCOMPLETE;
    }

    line_t **lines = calloc(line_count, sizeof(line_t *));
    if (!lines)
    {
        return ERROR;
    }

    size_t line_index = 0;
    size_t start_index = 0;
    bool extract_bulk_length = false;
    for (size_t i = 0; i < length; i++)
    {
        if (line_index >= line_count || (extract_bulk_length && line_index + 1 == line_count))
        {
            free_lines(lines, line_index);
            return ERROR;
        }

        if (data[i] == '$')
        {
            extract_bulk_length = true;
            continue;
        }

        if (is_delimeter(data, i, length))
        {
            continue;
        }

        line_t *line = calloc(1, sizeof(line_t));
        if (!line)
        {
            free_lines(lines, line_index);
            return ERROR;
        }

        line->data = data + start_index;
        line->len = i - start_index;
        start_index = i + 2;
        i = start_index;
        lines[line_index++] = line;

        if (extract_bulk_length)
        {
            line_t *payload = calloc(1, sizeof(line_t));
            if (!payload)
            {
                free_lines(lines, line_index);
                return ERROR;
            }
            lines[line_index++] = payload;

            long long bulk_length;
            if (parse_int(line->data + 1, line->len - 1, &bulk_length) != 0)
            {
                free_lines(lines, line_index);
                return ERROR;
            }

            if (bulk_length < 0)
            {
                free_lines(lines, line_index);
                return ERROR;
            }

            if (bulk_length + 1 > (long long)length)
            {
                free_lines(lines, line_index);
                return INCOMPLETE;
            }

            const char *bulk_data = data + start_index;
            if (bulk_data[bulk_length] != '\r' || bulk_data[bulk_length + 1] != '\n')
            {
                free_lines(lines, line_index);
                return ERROR;
            }

            payload->data = bulk_data;
            payload->len = bulk_length;
            start_index = bulk_length + 2;
            i += bulk_length + 2;
            extract_bulk_length = false;
        }
    }

    *out = lines;
    *count = line_index;

    return OK;
}

RespParseResult parse_lines(RespNode_t **out, line_t **lines, size_t len, size_t *lines_read)
{
    if (!lines || len < 1 || lines[0]->len < 1)
    {
        return ERROR;
    }

    RespNode_t *node = calloc(1, sizeof(RespNode_t));
    if (!node)
    {
        return ERROR;
    }

    switch (lines[0]->data[0])
    {
    case SIMPLE_STRING_KEY:
    {
        node->type = SIMPLE_STRING;
        node->value = strndup(lines[0]->data + 1, lines[0]->len - 1);
        node->length = lines[0]->len - 1;
        *lines_read = 1;
        break;
    }
    case INTEGER_KEY:
    {
        long long *value = malloc(sizeof(long long));
        if (!value || parse_int(lines[0]->data + 1, lines[0]->len - 1, value) != 0)
        {
            resp_node_free(node);
            return ERROR;
        }

        node->type = INTEGER;
        node->value = value;
        node->length = 0;
        *lines_read = 1;
        break;
    }
    case BULK_STRING_KEY:
    {
        if (len < 2)
        {
            resp_node_free(node);
            return INCOMPLETE;
        }

        node->type = BULK_STRING;
        node->value = strndup(lines[1]->data, lines[1]->len);
        node->length = lines[1]->len;
        *lines_read = 2;
        break;
    }
    case ARRAY_KEY:
    {
        long long array_length;
        if (parse_int(lines[0]->data + 1, lines[0]->len - 1, &array_length) != 0)
        {
            resp_node_free(node);
            return ERROR;
        }

        node->type = ARRAY;
        node->length = array_length;

        if (array_length < -1)
        {
            resp_node_free(node);
            printf("parse_lines: array length less than -1\n");
            return ERROR;
        }

        // -1 indicates NULL array, so break early here
        if (array_length == -1)
        {
            node->value = NULL;
            break;
        }

        if (array_length > (long long)len - 1)
        {
            resp_node_free(node);
            printf("parse_lines: array length longer than available lines\n");
            return ERROR;
        }

        RespNode_t **nodes = calloc(array_length, sizeof(RespNode_t *));
        if (!nodes)
        {
            resp_node_free(node);
            return ERROR;
        }
        node->value = nodes;

        size_t sub_lines_read = 0;
        for (long long i = 0; i < array_length; i++)
        {
            RespNode_t *sub_node;
            size_t offset = 1 + sub_lines_read;
            size_t lr = 0;
            RespParseResult res = parse_lines(&sub_node, lines + offset, len - offset, &lr);
            sub_lines_read += lr;
            if (res != OK)
            {
                resp_node_free(node);
                return res;
            }

            nodes[i] = sub_node;
        }

        *lines_read = 1 + sub_lines_read;

        break;
    }
    default:
    {
        resp_node_free(node);
        printf("parse_lines: unsupported input: %.*s\n", (int)lines[0]->len, lines[0]->data);
        return ERROR;
    }
    }

    *out = node;
    return OK;
}

RespParseResult resp_parse(RespNode_t **target, buffer_t *buf)
{
    line_t **lines = NULL;
    size_t line_count = 0;
    RespParseResult res = INCOMPLETE;
    size_t total_bytes = 0;

    do
    {
        ssize_t bytes_read = buf_read(buf, NULL, total_bytes + BUFFER_BLOCK_SIZE);
        if (bytes_read <= 0)
        {
            return ERROR;
        }
        total_bytes += bytes_read;

        if ((res = split_lines(buf_data(buf), total_bytes, &lines, &line_count)) == ERROR)
        {
            return res;
        }
    } while (total_bytes < MAX_READ_SIZE && res == INCOMPLETE);

    size_t lines_read = 0;
    res = parse_lines(target, lines, line_count, &lines_read);
    free_lines(lines, line_count);

    if (lines_read != line_count)
    {
        res = INCOMPLETE;
    }

    return res;
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