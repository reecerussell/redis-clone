#include <stdlib.h>
#include <string.h>
#include "../include/resp.h"

static int read_long(const char *src, size_t src_length, int offset, long *result)
{
    char nbuf[20];
    nbuf[19] = '\0';

    int i = 0;
    for (; i < 19 && (size_t)(offset + i) < src_length - 1; i++)
    {
        char c = src[offset + i];

        // Handle end of section
        if (c == '\r')
        {
            nbuf[i] = '\0';
            break;
        }

        // Handle non-numerics
        if (c < '0' || c > '9')
        {
            *result = -1;
            return i;
        }

        nbuf[i] = c;
    }

    *result = strtol(nbuf, NULL, 10);
    return i;
}

static char *read_string(const char *src, size_t src_length, int offset, int length)
{
    char *value = malloc(sizeof(char) * length + 1);
    if (!value)
    {
        return NULL;
    }
    value[length] = '\0';

    int i = 0;
    for (; (size_t)(offset + i) < src_length - 1 && i < length; i++)
    {
        value[i] = src[offset + i];
    }

    // String read must at specified length.
    if (i != length)
    {
        free(value);
        return NULL;
    }

    // Payload is more than specified limit
    if ((size_t)(offset + length + 1) <= src_length - 1 &&
        (src[offset + length] != '\r' ||
         src[offset + length + 1] != '\n'))
    {
        free(value);
        return NULL;
    }

    return value;
}

static int read_bulk_strings(const char *src, size_t src_length, unsigned long *value_idx, int argc, char **argv, int *arg_idx)
{
    long arg_len = 0;
    long chars_read = read_long(src, src_length, *value_idx, &arg_len);
    if (arg_len < 0)
    {
        return 1;
    }

    *value_idx += chars_read + 2;

    char *arg;
    if (arg_len > 0)
    {
        arg = read_string(src, src_length, *value_idx, arg_len);
        if (!arg)
        {
            return 1;
        }
    }
    else
    {
        arg = strdup("");
    }

    *value_idx += arg_len + 2;

    if (*arg_idx >= argc)
    {
        free(arg);
        return 1;
    }

    argv[(*arg_idx)++] = arg;

    return 0;
}

RespArgs *parse_command(const char *value)
{
    if (!value)
    {
        return NULL;
    }

    // Look out for the start of a command: *3 (3 being the number of args)
    size_t value_len = strlen(value); // TODO: handle empty strings, i.e. not terminated.
    if (value_len < 2 || value[0] != '*')
    {
        return NULL;
    }

    long argc = 0;
    int pos_inc = read_long(value, value_len, 1, &argc);
    if (argc < 1)
    {
        return NULL;
    }

    char **argv = calloc(argc, sizeof(char *));
    if (!argv)
    {
        return NULL;
    }

    unsigned long value_idx = 3 + pos_inc;
    int argi = 0;
    for (; value_idx < value_len;)
    {
        if (value[value_idx++] == '$')
        {
            if (read_bulk_strings(value, value_len, &value_idx, argc, argv, &argi) != 0)
            {
                for (long i = 0; i < argi; i++)
                {
                    free(argv[i]);
                }
                free(argv);
                return NULL;
            }
        }

        // TODO: handle malformed inputs.
    }

    // Return null as invalid payload, due to arg count mismatch
    if (argc != argi)
    {
        for (long i = 0; i < argi; i++)
        {
            free(argv[i]);
        }
        free(argv);
        return NULL;
    }

    RespArgs *args = malloc(sizeof(RespArgs));
    if (!args)
    {
        for (long i = 0; i < argc; i++)
        {
            free(argv[i]);
        }
        free(argv);
        return NULL;
    }

    args->argc = argc;
    args->argv = argv;

    return args;
}

void resp_args_free(RespArgs *args)
{
    if (!args)
    {
        return;
    }

    for (int i = 0; i < args->argc; i++)
    {
        free(args->argv[i]);
    }
    free(args->argv);
    free(args);
}