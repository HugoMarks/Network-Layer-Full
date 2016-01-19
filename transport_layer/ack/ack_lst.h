/* Arquivo: ack_lst.h
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */


#ifndef ACK_LST_H
#define ACK_LST_H

#include <common.h>
#include <transport_layer.h>

PUBLIC void insert_ack(ack_lst_t **lista, ack_lst_t *novo);
PUBLIC void rm_ack(ack_lst_t **lista, uint32_t num);
PUBLIC void set_ack(ack_lst_t *list, uint32_t num);
PUBLIC void list_ack(ack_lst_t *list);
PUBLIC uint8_t is_set(ack_lst_t *list, uint32_t num);
PUBLIC size_t in_seq(ack_lst_t **list);
PUBLIC void rm_ack_lst(ack_lst_t **lst);

#endif