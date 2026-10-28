#include <stdlib.h>
#include <uuid/uuid.h>
#include <ctype.h>
#include <time.h>
#include "utils.h"

char *generate_uuid()
{
    char *out = malloc(sizeof(char) * UUID_STR_LEN);
    if (!out)
    {
        return NULL;
    }

    uuid_t b;
    uuid_generate(b);
    uuid_unparse_lower(b, out);

    return out;
}

char *to_uppercase(char *value)
{
    for (size_t i = 0;; i++)
    {
        if (value[i] == '\0')
        {
            break;
        }

        value[i] = toupper(value[i]);
    }

    return value;
}

struct stopwatch
{
    struct timespec start;
};

stopwatch_t *stopwatch_start()
{
    stopwatch_t *stopwatch = malloc(sizeof(stopwatch_t));
    if (!stopwatch)
    {
        return NULL;
    }
    clock_gettime(CLOCK_MONOTONIC, &stopwatch->start);
    return stopwatch;
}

long long stopwatch_stop_us(stopwatch_t *stopwatch)
{
    if (!stopwatch)
    {
        return -1;
    }

    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);

    long long elapsed_us = (end.tv_sec - stopwatch->start.tv_sec) * 1000000LL + (end.tv_nsec - stopwatch->start.tv_nsec) / 1000;
    free(stopwatch);
    return elapsed_us;
}