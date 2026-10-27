#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "../src/listener.h"
#include "../src/buffer.h"
#include "../src/logger.h"
#include "../src/resp.h"

static Listener *lis;

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

void handler(RespNode_t *node, RespNode_t **reply)
{
    log_infoc("test.handler", "Received request!");

    print_node(node);

    RespNode_t *reply_node = malloc(sizeof(RespNode_t));
    if (!reply_node)
    {
        log_errorc("test.handler", "Failed to allocate reply node");
        return;
    }

    reply_node->type = SIMPLE_STRING;
    reply_node->length = 2;
    reply_node->value = strdup("OK");
    *reply = reply_node;
}

int main(void)
{
    if (watch_shutdown_signals() != 0)
    {
        printf("[main]: ERROR - failed to watch shutdown signals");
        exit(1);
    }

    int port = 8573;

    printf("[main]: Creating listener...\n");

    lis = listener_create(port, handler);

    printf("[main]: Starting listener on port %d...\n", port);
    if (listener_start(lis) != 0)
    {
        printf("[main]: ERROR - Failed to start listener\n");
        exit(1);
    }

    printf("[main]: Listener stopped, freeing resources...\n");

    listener_free(lis);

    printf("[main]: Complete\n");
}