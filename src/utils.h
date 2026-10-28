#ifndef UTILS_H
#define UTILS_H

char *generate_uuid();

char *to_uppercase(char *value);

typedef struct stopwatch stopwatch_t;

stopwatch_t *stopwatch_start();

long long stopwatch_stop_us(stopwatch_t *stopwatch);

#endif