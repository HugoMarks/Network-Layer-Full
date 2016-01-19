/* Arquivo: net_layer.h
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#ifndef NET_H
#define NET_H

#include <inttypes.h>
#include <config.h>
#include <sys/types.h>
#include <common.h>
#include <link.h>

#define MAX_SEND_WAITING 100
#define NET_HEADER_SIZE (sizeof(size_t) + (sizeof(uint16_t) * 5) + (sizeof(uint8_t) * 2))
#define TTL 6
#define INFINITE_ROUTE 0

typedef struct netdata {
	size_t size;
	uint16_t src;
	byte *data;	
} netdata_t;

typedef struct datagram {
	size_t size;
	uint16_t id;
	uint16_t frag;
	uint8_t protocol;
	uint8_t ttl;
	uint16_t dest;
	uint16_t src;
	byte *data;
	uint16_t checksum;
} datagram_t;

typedef struct offsetlst {
	datagram_t *datagram;
	struct offsetlst *next;
} offsetlst_t;

typedef struct packlst {
	uint16_t id;
	uint16_t src;
	uint16_t time;
	size_t count_size;
	size_t total_size;
	offsetlst_t *offs;
	struct packlst_t *next;
} packlst_t;

/* Estrutura utilizada pelo protocolo de roteamento (usada para o controle das rotas)*/
typedef struct routetable {
	uint8_t dest;
	uint8_t cost;
	uint16_t gateway;
}routetable_t;

typedef struct wait_list_item {
	uint16_t dest;
	uint16_t src;
	byte *data;
	uint16_t offset;
	uint8_t ttl;
	size_t size;
	uint8_t protocol;
	uint16_t id;
	uint16_t first_dest_link;
	//struct wait_list_item *next;
} wait_list_item_t;

int net_init(uint16_t num, pthread_mutex_t link, pthread_mutex_t net, pthread_mutex_t net2, pthread_mutex_t transp, frame_t *link_recv_frame, netdata_t **net_recv_datagram, int way[MAX_HOSTS]);
int net_send(uint16_t dest, byte *data, size_t size, uint8_t protocol);
void dislpay_routes();
void disable_route(uint16_t host);
void enable_route(uint16_t host);

#endif