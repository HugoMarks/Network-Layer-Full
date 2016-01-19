/* Arquivo: common.h
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#ifndef COMMON_H
#define COMMON_H

#include <config.h>
#include <stdlib.h>
#include <stdio.h>

typedef unsigned char byte;
typedef const char* string;

typedef enum layer
{
	LINK,
	NETWORK,
	TRANSPORT,
	APPLICATION
} layer_t;

typedef enum {
	TRUE,
	FALSE
} bool;

#define OK 		0
#define ERROR	-1

#define MAX_BUFFER_SIZE 1024

#define PUBLIC
#define PRIVATE static

#define TRUE	1
#define FALSE 	0

#define ALLOC(t, s) (t *)malloc(s)

#endif