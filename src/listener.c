#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <string.h>
#include "listener.h"
#include "buffer.h"
#include "logger.h"
#include "utils.h"

Listener *listener_create(int port, MessageHandler handler, DisconnectHandler on_disconnect)
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
    lis->on_disconnect = on_disconnect;
    atomic_init(&lis->is_stopping, false);
    return lis;
}

static void process_reply(int fd, RespNode_t *reply)
{
    if (fd < 0 || !reply)
    {
        return;
    }

    char *reply_string = resp_node_to_string(reply);
    if (!reply_string)
    {
        resp_node_free(reply);
    }

    size_t reply_len = strlen(reply_string);
    size_t sent = 0;
    while (sent < reply_len)
    {
        ssize_t bytes_sent = send(fd, reply_string + sent, reply_len - sent, MSG_NOSIGNAL);
        if (bytes_sent <= 0)
        {
            break;
        }
        sent += bytes_sent;
    }

    free(reply_string);
    resp_node_free(reply);
}

static void handle_connection(Listener *lis, int fd)
{
    if (!lis || fd < 0)
    {
        return;
    }

    buffer_t *buf = buf_create(fd);
    if (!buf)
    {
        return;
    }

    char *client_id = generate_uuid();

    RespParseResult res = ERROR;
    size_t total_bytes = 0;
    size_t total_bytes_consumed = 0;
    stopwatch_t *stopwatch = NULL;
    do
    {
        if (!stopwatch)
        {
            stopwatch = stopwatch_start();
        }

        if (total_bytes_consumed >= total_bytes || res == INCOMPLETE)
        {
            ssize_t bytes_read = buf_read(buf, NULL, 1024);
            if (bytes_read <= 0)
            {
                break;
            }
            total_bytes += bytes_read;
        }

        RespNode_t *node;
        size_t bytes_consumed;
        res = try_parse(buf_data(buf) + total_bytes_consumed, total_bytes - total_bytes_consumed, 0, &node, &bytes_consumed);
        if (res != OK)
        {
            continue;
        }

        RespNode_t *reply = NULL;
        lis->handler(client_id, node, &reply);
        resp_node_free(node);
        total_bytes_consumed += bytes_consumed;
        process_reply(fd, reply);

        log_infoc("listener", "Read and processed request in %lldus", stopwatch_stop_us(stopwatch));
        stopwatch = NULL;
    } while (res != ERROR);

    if (lis->on_disconnect)
    {
        lis->on_disconnect(client_id);
    }

    if (stopwatch)
    {
        stopwatch_stop_us(stopwatch);
    }

    buf_free(buf);
    free(client_id);
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
        setsockopt(clifd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));

        log_infoc("listener", "Accepted new connection from '%s'", (char *)lis->addr);

        handle_connection(lis, clifd);
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