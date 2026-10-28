#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "listener.h"
#include "buffer.h"
#include "logger.h"
#include "resp.h"
#include "engine.h"

static Listener *lis;
static Engine_t *engine;

static void handle_signal(int)
{
    if (lis)
    {
        printf("[main]: Interrupt received, stopping listener...\n");
        listener_stop(lis);
    }
}

int watch_shutdown_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        return -1;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        return -1;
    }
    return 0;
}

int print_node(RespNode_t *node)
{
    log_infoc("test.print_node", "Received %s node", resp_node_type_name(node->type));
    switch (node->type)
    {
    case SIMPLE_STRING:
    case BULK_STRING:
    case RESP_ERROR:
        log_infoc("test.print_node", "Value (len=%lld): %s", node->length, (char *)node->value);
        break;
    case INTEGER:
        log_infoc("test.print_node", "Value: %lld", *((long long *)node->value));
        break;
    case ARRAY:
    {
        RespNode_t **nodes = (RespNode_t **)node->value;
        for (long long i = 0; i < node->length; i++)
        {
            print_node(nodes[i]);
        }
        break;
    }
    }

    return 0;
}

void handler(char *client_id, RespNode_t *node, RespNode_t **reply)
{
    log_infoc("handler", "Received request from client '%s'!", client_id);

    print_node(node);

    engine_process(engine, client_id, node, reply);

    log_infoc("handler", "Finished request from client '%s'!", client_id);
}

void handle_disconnect(char *client_id)
{
    log_infoc("handle_disconnect", "client '%s' disconnected, removing from engine", client_id);
    engine_forget_client(engine, client_id);
}

int main(void)
{
    if (watch_shutdown_signals() != 0)
    {
        printf("[main]: ERROR - failed to watch shutdown signals");
        exit(1);
    }

    int port = 8573;

    printf("[main]: Creating engine and listener...\n");

    engine = engine_create();
    lis = listener_create(port, handler, handle_disconnect);

    printf("[main]: Starting listener on port %d...\n", port);
    if (listener_start(lis) != 0)
    {
        printf("[main]: ERROR - Failed to start listener\n");
        exit(1);
    }

    printf("[main]: Listener stopped, freeing resources...\n");

    listener_free(lis);
    engine_free(engine);

    printf("[main]: Complete\n");
}