/* Arquivo: config.h
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <inttypes.h>

#define MAX_HOSTS 				6
#define MAX_IP_STRING_SIZE		15
#define MAX_PORT_STRING_SIZE 	5

typedef struct host
{
	uint16_t num;
	uint16_t port;
	uint32_t ip_addr;
} host_t;

void readconfig(const char *path, int links[MAX_HOSTS][MAX_HOSTS], host_t hosts[MAX_HOSTS]);

#endif
