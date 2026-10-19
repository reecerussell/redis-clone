#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "../include/socket.h"
#include "../include/buffer.h"
#include "../include/logger.h"
#include "../include/resp.h"

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

int handler(buffer_t *buf, char **result)
{
    log_infoc("test.handler", "Received request!");

    RespNode_t *node;
    if (try_resp_parse(&node, buf) != 0)
    {
        log_errorc("test.handler", "Failed to parse arguments");
    }
    else
    {
        log_infoc("test.handler", "Received %s node", resp_node_type_name(node->type));
        // for (int i = 0; i < ra->argc; i++)
        // {
        //     log_infoc("test.handler", "[%d]: %s", i, ra->argv[i]);
        // }
    }

    resp_node_free(node);

    *result = strdup("Bye bye");

    return 8;
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