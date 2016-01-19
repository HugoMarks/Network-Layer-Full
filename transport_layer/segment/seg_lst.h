/* Arquivo: seg_lst.h
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
 
#ifndef SEG_LST_H
#define SEG_LST_H

#include <common.h>
#include <transport_layer.h>

void list_seg(seglst_t *list);
PUBLIC uint32_t insert_seg(seglst_t **lst, seglst_t **lst_aux, segment_t *seg, size_t size, uint32_t base);
PUBLIC size_t recv_seg(seglst_t **list, byte *buffer, size_t size);
PUBLIC void rm_seg_lst(seglst_t **lst);

#endif