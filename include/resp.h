#ifndef RESP_H
#define RESP_H

typedef struct
{
    char **argv; // argc NUL-terminated strings
    int argc;
} RespArgs;

// Parses one complete RESP command: an array of bulk strings, e.g.
// "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n". Returns NULL if the input is malformed
// or incomplete. The result is owned by the caller: release with
// resp_args_free.
RespArgs *parse_command(const char *value);

void resp_args_free(RespArgs *args);

#endif
