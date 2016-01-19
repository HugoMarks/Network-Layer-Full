/* Arquivo: net_layer_test.h
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#ifndef NET_LAYER_TEST_H
#define NET_LAYER_TEST_H

#include <inttypes.h>
#include <config.h>
#include <sys/types.h>
#include <link.h>
#include <net_layer.h>

//Camada de teste;
//falta a parte de enviar para baiixo.
int net_layer_test_init(uint16_t num, pthread_mutex_t transp, pthread_mutex_t net2, netdata_t *net_recv_datagram);

#endif
