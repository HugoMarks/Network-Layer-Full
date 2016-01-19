/* Arquivo: test_layer.h
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#ifndef TEST_LAYER_H
#define TEST_LAYER_H

#include <inttypes.h>
#include <config.h>
#include <sys/types.h>
#include <link.h>

//Camada de teste;
//falta a parte de enviar para baiixo.
int test_init(uint16_t num, pthread_mutex_t, pthread_mutex_t, frame_t *link_recv_frame);
void recv_from_below(frame_t data);
void to_above_f_below(frame_t data);

#endif
