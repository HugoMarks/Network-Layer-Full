/* Arquivo: net_list.h
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */

#ifndef LIST_H
#define LIST_H

#include <frag.h>

PUBLIC bool insertoffset(packlst_t **packlst, datagram_t *datagram);

packlst_t *getid(packlst_t **list, uint16_t id, uint16_t src);

byte *getpack(packlst_t **list, uint16_t id, uint16_t src, size_t *size);


#endif