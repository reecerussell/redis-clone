#ifndef ENGINE_H
#define ENGINE_H

#include "resp.h"

typedef struct Engine Engine_t;

Engine_t *engine_create();

void engine_process(Engine_t *engine, char *client_id, RespNode_t *node, RespNode_t **out);

void engine_forget_client(Engine_t *engine, char *client_id);

void engine_free(Engine_t *engine);

#endif