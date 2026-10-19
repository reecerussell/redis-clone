#ifndef BUFFER_H
#define BUFFER_H

#include <stdlib.h>

typedef struct buffer buffer_t;

buffer_t *buf_create(int fd);

ssize_t buf_read(buffer_t *buf, char *out, size_t n);

ssize_t buf_position(buffer_t *buf);

int buf_seek(buffer_t *buf, size_t position);

void buf_free(buffer_t *buf);

#endif