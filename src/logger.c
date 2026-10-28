#include <stdio.h>
#include <stdarg.h>

// error = 0, info = 1, debug = 2;
static int max_level = 2;

void log_info(const char *format, ...)
{
    if (max_level < 1)
    {
        return;
    }

    va_list args;

    printf("[INF]: ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void log_infoc(const char *category, const char *format, ...)
{
    if (max_level < 1)
    {
        return;
    }

    va_list args;

    printf("[INF][%s]: ", category);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void log_debug(const char *format, ...)
{
    if (max_level < 2)
    {
        return;
    }

    va_list args;

    printf("[DBG]: ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void log_debugc(const char *category, const char *format, ...)
{
    if (max_level < 2)
    {
        return;
    }

    va_list args;

    printf("[DBG][%s]: ", category);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void log_error(const char *format, ...)
{
    if (max_level < 0)
    {
        return;
    }

    va_list args;

    printf("[ERR]: ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void log_errorc(const char *category, const char *format, ...)
{
    if (max_level < 0)
    {
        return;
    }

    va_list args;

    printf("[ERR][%s]: ", category);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}