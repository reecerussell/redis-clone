#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../include/buffer.h"

#define DEFAULT_BLOCK_SIZE 1024

struct buffer
{
    int fd;
    char *data;
    size_t len;
    size_t cap;
    size_t pos;
    size_t block_size;
};

buffer_t *buf_create(int fd)
{
    if (fd < 0)
    {
        return NULL;
    }

    buffer_t *buf = malloc(sizeof(buffer_t));
    if (!buf)
    {
        return NULL;
    }

    buf->fd = fd;
    buf->data = NULL;
    buf->cap = 0;
    buf->len = 0;
    buf->pos = 0;
    buf->block_size = DEFAULT_BLOCK_SIZE;

    return buf;
}

static int buf_ensure_space(buffer_t *buf, size_t extra)
{
    size_t requested = buf->len + extra;
    if (requested <= buf->cap)
    {
        return 0; // Short circuit if we already have the space.
    }

    size_t cap = buf->cap;
    while (cap < requested)
    {
        cap += buf->block_size;
    }

    char *data = realloc(buf->data, cap);
    if (!data)
    {
        return -1;
    }

    buf->data = data;
    buf->cap = cap;

    return 0;
}

ssize_t buf_read(buffer_t *buf, char *out, size_t n)
{
    if (!buf)
    {
        return -1;
    }

    if (n <= 0)
    {
        return 0;
    }

    if (buf_ensure_space(buf, n) < 0)
    {
        return -1;
    }

    if (buf->len - buf->pos < n)
    {
        ssize_t bytes_read;
        do
        {
            bytes_read = read(buf->fd, buf->data + buf->len, n);
        } while (bytes_read < 0 && errno == EINTR);

        if (bytes_read <= 0)
        {
            return bytes_read;
        }

        buf->len += (size_t)bytes_read;
    }

    size_t available = buf->len - buf->pos;
    size_t count = n < available ? n : available;

    if (out)
    {
        memcpy(out, buf->data + buf->pos, count);
    }

    buf->pos += count;
    return (ssize_t)count;
}

ssize_t buf_position(buffer_t *buf)
{
    if (!buf)
    {
        return -1;
    }

    return buf->pos;
}

int buf_seek(buffer_t *buf, size_t pos)
{
    if (!buf || pos > buf->len)
    {
        return -1;
    }

    buf->pos = pos;
    return 0;
}

char *buf_data(buffer_t *buf)
{
    if (!buf)
    {
        return NULL;
    }
    return buf->data;
}

void buf_free(buffer_t *buf)
{
    if (!buf)
    {
        return;
    }

    if (buf->data)
    {
        free(buf->data);
    }

    free(buf);
}