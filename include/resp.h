#ifndef RESP_H
#define RESP_H

#include <stdlib.h>
#include "buffer.h"

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
    size_t length;
    void *value;
} RespNode_t;

int resp_parse(RespNode_t **target, buffer_t *buf);

void resp_node_free(RespNode_t *node);

int try_resp_parse(RespNode_t **target, buffer_t *buf);

const char *resp_node_type_name(RespNodeType type);

#endif
