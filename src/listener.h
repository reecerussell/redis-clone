#include <netinet/in.h>
#include <stdatomic.h>
#include "resp.h"

typedef void (*MessageHandler)(RespNode_t *node, RespNode_t **reply);

typedef struct
{
    struct sockaddr_in *addr;
    MessageHandler handler;
    atomic_bool is_stopping;
} Listener;

Listener *listener_create(int port, MessageHandler handler);

int listener_start(Listener *lis);

void listener_stop(Listener *lis);

void listener_free(Listener *lis);
