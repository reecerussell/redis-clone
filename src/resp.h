#ifndef RESP_H
#define RESP_H

typedef enum
{
    ARRAY,
    SIMPLE_STRING,
    BULK_STRING,
    INTEGER,
    RESP_ERROR
} RespNodeType;

typedef struct
{
    RespNodeType type;
    long long length;
    void *value;
} RespNode_t;

typedef enum
{
    OK,
    ERROR,
    INCOMPLETE
} RespParseResult;

RespParseResult try_parse(const char *data, size_t length, int depth, RespNode_t **out, size_t *bytes_consumed);

char *resp_node_to_string(RespNode_t *node);

void resp_node_free(RespNode_t *node);

const char *resp_node_type_name(RespNodeType type);

// Helpers

RespNode_t *resp_child_node_at(RespNode_t *node, size_t index);

RespNode_t *resp_create_string(char *value);

RespNode_t *resp_create_error(char *value);

RespNode_t *resp_create_integer(long long value);

RespNode_t *resp_create_array(size_t len, ...);

#endif
