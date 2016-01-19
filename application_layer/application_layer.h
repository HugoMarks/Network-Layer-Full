/* Arquivo: application_layer.h
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#ifndef APPLICATION_LAYER_H
#define APPLICATION_LAYER_H

#include <inttypes.h>
#include <config.h>
#include <sys/types.h>

#define MAX_BUF_LENGTH	1024

//Camada de aplicação;
int application_layer_init(uint16_t num_host);
void config_link(int source, int dest, int mtu);

#endif
