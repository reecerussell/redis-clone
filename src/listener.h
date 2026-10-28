#include <netinet/in.h>
#include <stdatomic.h>
#include "resp.h"

typedef void (*MessageHandler)(char *client_id, RespNode_t *node, RespNode_t **reply);

typedef void (*DisconnectHandler)(char *client_id);

typedef struct
{
    struct sockaddr_in *addr;
    MessageHandler handler;
    DisconnectHandler on_disconnect;
    atomic_bool is_stopping;
} Listener;

Listener *listener_create(int port, MessageHandler handler, DisconnectHandler on_disconnect);

int listener_start(Listener *lis);

void listener_stop(Listener *lis);

void listener_free(Listener *lis);
