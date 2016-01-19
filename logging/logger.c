/* Arquivo: logger.c
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#include <common.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdio_ext.h>

#define DEFAULT_TIME_SIZE 16

PRIVATE int getformatedtime(char *ret);

PUBLIC void logdebug(FILE *out, layer_t level, string fmt, ...)
{
	va_list args;
	char fmt_time[DEFAULT_TIME_SIZE];

	__fpurge(stdout);

	switch (level)
	{
		case LINK:
			fprintf(out, "enlace: ");
			break;
		case NETWORK:
			fprintf(out, "rede: ");
			break;
		case TRANSPORT:
			fprintf(out, "transporte: ");
			break;
		case APPLICATION:
			fprintf(out, "app: ");
			break;
		default:
			fprintf(out, "?: ");
			break;
	}

	//__fpurge(stdout);
    getformatedtime(fmt_time);
    fprintf(out, "[");
    fputs(fmt_time, out);
    fprintf(out, "] ");
    va_start(args, fmt);
	vfprintf(out, fmt, args);
	fprintf(out, "\n");
	va_end(args);
	__fpurge(stdout);
}

PRIVATE int getformatedtime(char *ret)
{
	struct timeval tv;
	struct tm *tm;
	char fmt[DEFAULT_TIME_SIZE];

	gettimeofday(&tv, NULL);

    if((tm = localtime(&tv.tv_sec)) != NULL)
    {
		strftime(fmt, DEFAULT_TIME_SIZE, "%H:%M:%S", tm);
		snprintf(ret, DEFAULT_TIME_SIZE, fmt, tv.tv_usec);
		return OK;
    }

    return ERROR;
}