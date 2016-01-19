/* Arquivo: logging.h
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#ifndef LOGGING_H
#define LOGGING_H

#include <common.h>

void logdebug(FILE *out, layer_t level, string fmt, ...);

#endif