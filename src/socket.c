#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include "socket.h"
#include "buffer.h"
#include "logger.h"

Listener *listener_create(int port, MessageHandler handler)
{
    if (port < 1 || port > 65535 || !handler)
    {
        return NULL;
    }

    struct sockaddr_in *sock_addr = malloc(sizeof(struct sockaddr_in));
    if (!sock_addr)
    {
        return NULL;
    }
    bzero((char *)sock_addr, sizeof(*sock_addr));
    sock_addr->sin_family = AF_INET;
    sock_addr->sin_addr.s_addr = INADDR_ANY;
    sock_addr->sin_port = htons(port);

    Listener *lis = malloc(sizeof(Listener));
    if (!lis)
    {
        free(sock_addr);
        return NULL;
    }

    lis->addr = sock_addr;
    lis->handler = handler;
    atomic_init(&lis->is_stopping, false);
    return lis;
}

int listener_start(Listener *lis)
{
    if (!lis)
    {
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        return 1;
    }
    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    if (bind(sockfd, (struct sockaddr *)lis->addr, sizeof(*lis->addr)) < 0)
    {
        log_errorc("listener", "Failed to bind to port '%s'", (char *)lis->addr);
        close(sockfd);
        return 1;
    }
    listen(sockfd, 1); // handle one connection at a time for now.

    log_infoc("listener", "Started listening for connections on '%s'", (char *)lis->addr);

    while (atomic_load(&lis->is_stopping) != true)
    {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int clifd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len);
        if (clifd < 0)
        {
            continue;
        }

        log_infoc("listener", "Accepted new connection from '%s'", (char *)lis->addr);

        buffer_t *buf = buf_create(clifd);
        if (!buf)
        {
            close(clifd);
            continue;
        }

        char *resp;
        int resp_len = lis->handler(buf, &resp);

        // If no error or there is data, try to send the response. In future
        // I need to add logging here so we know what happens.
        if (resp_len > 0)
        {
            send(clifd, resp, resp_len, MSG_NOSIGNAL);
            free(resp);
        }

        buf_free(buf);
        close(clifd);
    }

    close(sockfd);
    return 0;
}

void listener_stop(Listener *lis)
{
    if (!lis)
    {
        return;
    }

    atomic_store(&lis->is_stopping, true);
}

void listener_free(Listener *lis)
{
    if (!lis)
    {
        return;
    }

    free(lis->addr);
    free(lis);
}