#include <netinet/in.h>
#include <stdatomic.h>
#include "buffer.h"

typedef int (*MessageHandler)(buffer_t *buf, char **resp);

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
