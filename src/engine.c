#include <string.h>
#include "engine.h"
#include "hash_table.h"
#include "utils.h"
#include "logger.h"

#define INITIAL_DATA_SIZE 16

// Commands
#define GET_KEY "GET"
#define SET_KEY "SET"
#define DEL_KEY "DEL"
#define HELLO_KEY "HELLO"
#define AUTH_KEY "AUTH"
#define CLIENT_KEY "CLIENT"

enum Command
{
    CMDGET,
    CMDSET,
    CMDDEL,
    CMDHELLO,
    CMDAUTH,
    CMDCLIENT
};

struct Engine
{
    HashTable *data;
    HashTable *clients;
};

Engine_t *engine_create()
{
    Engine_t *engine = malloc(sizeof(Engine_t));
    if (!engine)
    {
        return NULL;
    }

    HashTable *data = ht_create(INITIAL_DATA_SIZE);
    if (!data)
    {
        free(engine);
        return NULL;
    }
    HashTable *clients = ht_create(INITIAL_DATA_SIZE);
    if (!clients)
    {
        ht_free(data);
        free(engine);
        return NULL;
    }

    engine->data = data;
    engine->clients = clients;
    return engine;
}

void engine_free(Engine_t *engine)
{
    if (!engine)
    {
        return;
    }

    ht_free(engine->data);
    ht_free(engine->clients);
    free(engine);
}

static bool is_command_node(RespNode_t *node, enum Command *cmd)
{
    if (!node)
    {
        return false;
    }

    if (node->type != ARRAY || node->length < 2)
    {
        log_infoc("engine", "node is not array with at least 2 items");
        return false;
    }

    RespNode_t *first_node = resp_child_node_at(node, 0);
    if (!first_node || (first_node->type != SIMPLE_STRING && first_node->type != BULK_STRING))
    {
        return false;
    }

    RespNode_t *key_node = resp_child_node_at(node, 1);
    if (!key_node || (key_node->type != SIMPLE_STRING && key_node->type != BULK_STRING))
    {
        return false;
    }

    char *key = to_uppercase((char *)first_node->value);
    if (strcmp(key, GET_KEY) == 0)
    {
        *cmd = CMDGET;
        return node->length == 2;
    }

    if (strcmp(key, SET_KEY) == 0)
    {
        *cmd = CMDSET;
        if (node->length < 3)
        {
            return false;
        }

        RespNode_t *value_node = resp_child_node_at(node, 2);
        if (!value_node || (value_node->type != SIMPLE_STRING && value_node->type != BULK_STRING))
        {
            return false;
        }

        return true;
    }

    if (strcmp(key, DEL_KEY) == 0)
    {
        *cmd = CMDDEL;
        return node->length == 2;
    }

    if (strcmp(key, HELLO_KEY) == 0)
    {
        *cmd = CMDHELLO;
        if (node->length != 2)
        {
            return false;
        }

        return true;
    }

    if (strcmp(key, AUTH_KEY) == 0)
    {
        *cmd = CMDAUTH;
        return true; // TODO: handle this
    }

    if (strcmp(key, CLIENT_KEY) == 0)
    {
        *cmd = CMDCLIENT;
        return true; // TODO: handle this
    }

    return false;
}

static void ack_client(Engine_t *engine, char *client_id)
{
    if (!engine || !client_id)
    {
        return;
    }

    log_debugc("engine", "acknowleging client '%s'", client_id);
    ht_set(engine->clients, client_id, "1");
}

void engine_process(Engine_t *engine, char *client_id, RespNode_t *node, RespNode_t **out)
{
    if (!engine || !node || !client_id || !out)
    {
        return;
    }

    enum Command cmd;
    if (!is_command_node(node, &cmd))
    {
        log_debugc("engine", "received an invalid command");
        *out = resp_create_error("not a valid command");
        return;
    }

    RespNode_t *key_node = resp_child_node_at(node, 1);
    if (!key_node)
    {
        return;
    }
    char *key = (char *)key_node->value;

    switch (cmd)
    {
    case CMDHELLO:
    {
        RespNode_t *value_node = resp_child_node_at(node, 1);
        char *value = (char *)value_node->value;
        if (strcmp(value, "2") != 0)
        {
            log_infoc("engine", "client requested unsupported protocol: %lld", value);
            *out = resp_create_error("Only RESP2 is supported");
        }
        else
        {
            log_infoc("engine", "ack'ing client");
            *out = resp_create_array(2, resp_create_string("HELLO"), resp_create_integer(2));
            ack_client(engine, client_id);
        }
        break;
    }
    case CMDGET:
    {
        log_debugc("engine", "looking up key '%s'", key);
        char *value = ht_get(engine->data, key);
        if (!value)
        {
            *out = resp_create_string("-1");
        }
        else
        {
            *out = resp_create_string(value);
        }
        break;
    }
    case CMDDEL:
    {
        log_debugc("engine", "deleting key '%s'", key);
        ht_del(engine->data, key);
        *out = resp_create_integer(1);
        break;
    }
    case CMDSET:
    {
        RespNode_t *value_node = resp_child_node_at(node, 2);
        if (value_node)
        {
            log_debugc("engine", "setting key '%s'", key);
            ht_set(engine->data, key, (char *)value_node->value);
        }
        *out = resp_create_string("-1");
        break;
    }
    case CMDAUTH:
    {
        *out = resp_create_error("not supported");
        break;
    }
    default:
        *out = resp_create_string("OK");
        break;
    }
}

void engine_forget_client(Engine_t *engine, char *client_id)
{
    if (!engine || !client_id)
    {
        return;
    }
    ht_del(engine->clients, client_id);
}