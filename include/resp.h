#ifndef RESP_H
#define RESP_H

typedef enum
{
    ARRAY,
    SIMPLE_STRING,
    BULK_STRING,
    INTEGER
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

void resp_node_free(RespNode_t *node);

const char *resp_node_type_name(RespNodeType type);

#endif
