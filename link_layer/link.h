/* Arquivo: link.h
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#ifndef LINK_H
#define LINK_H



#include <inttypes.h>
#include <config.h>
#include <sys/types.h>
#include <common.h>

#define LINK_HEADER_SIZE sizeof(size_t) + sizeof(uint16_t)

typedef struct frame
{
	uint16_t dest;
	size_t size;
	byte *data;
	uint16_t checksum;
} frame_t;

int link_init(uint16_t num, pthread_mutex_t link, pthread_mutex_t net, frame_t **link_recv_frame,int max_mtu);
int link_send(uint16_t dest, byte *data, size_t size);
host_t *findhost(uint16_t num);
int getmtu(uint16_t first, uint16_t second);
//recebe da camada de cima.
void recv_f_above(frame_t);
//void send_t_above(frame_t);
PUBLIC void config_garbler(int l, int c, int d);

#endif
