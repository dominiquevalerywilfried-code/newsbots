/* log.c - Logs horodatés dans un fichier + console */

#include "newsbot.h"
#include <stdarg.h>
#include <time.h>

static void log_write(const char *level, const char *fmt, va_list args)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);

    /* Console */
    printf("[%s] [%s] ", ts, level);
    va_list args2;
    va_copy(args2, args);
    vprintf(fmt, args2);
    va_end(args2);
    printf("\n");
    fflush(stdout);

    /* Fichier */
    FILE *f = fopen(LOG_FILE, "a");
    if (f) {
        fprintf(f, "[%s] [%s] ", ts, level);
        vfprintf(f, fmt, args);
        fprintf(f, "\n");
        fclose(f);
    }
}

void log_info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write("INFO ", fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write("ERROR", fmt, args);
    va_end(args);
}
