/* Arquivo: seg_lst.c
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */

#include <seg_lst.h>
#include <string.h>

void list_seg(seglst_t *list)
{
    while(list != NULL)
    {
        printf("num_seq: %u , size: %zu\n",list->segm->seq_num, list->size);
        list = (seglst_t *)list->next;
    }
}

PRIVATE seglst_t *is_next(seglst_t **lista, uint32_t base)
{
    seglst_t *prev = NULL, *aux = (*lista), *ret = NULL;
    while(aux != NULL)
    {
        if(aux->segm->seq_num == base)
        {
            break;
        }
        prev = aux;
        aux = (seglst_t *)aux->next;
    }
    if(aux != NULL)
    {
        if(aux->segm->seq_num == base)
        {
            if(prev == NULL)
            {
                (*lista) = (seglst_t *)aux->next;
                ret = aux;
            }
            else
            {
                prev->next = aux->next;
                ret = aux;
            }
        }
    }
    return ret;
}

PRIVATE void insert_seg_lst(seglst_t **lst, segment_t *seg, size_t size)
{
    seglst_t *aux, *novo;
    novo = (seglst_t *)malloc(sizeof(seglst_t));
    novo->segm = seg;
    novo->size = size;
    novo->next = NULL;
    if((*lst) == NULL)
    {
        (*lst) = novo;
    }
    else
    {
        aux = (*lst);
        while(aux->next != NULL)
        {
            if(aux->segm->seq_num == seg->seq_num)
            {
                free(seg);
                return;
            }
            aux = (seglst_t *)aux->next;
        }
        if(aux->segm->seq_num == seg->seq_num)
        {
            free(seg);
            return;
        }
        aux->next = (struct seglst_t *)novo;
    }
}

PUBLIC uint32_t insert_seg(seglst_t **lst, seglst_t **lst_aux, segment_t *seg, size_t size, uint32_t base)
{
    seglst_t *aux, *aux2;
    uint32_t count = base, ret = 0;
    if((*lst) == NULL)
    {
        if(seg->seq_num == base)
        {
            (*lst) = (seglst_t *)malloc(sizeof(seglst_t));
            (*lst)->segm = seg;
            (*lst)->size = size;
            (*lst)->next = NULL;
            
            ret = size;
            aux = (*lst);
            count = (count + size) % MAX_SEQ_NUM;
            while(1==1)
            {
                aux2 = is_next(lst_aux, count);
                if(aux2 == NULL)
                {
                    break;
                }
                count = (count + aux2->size) % MAX_SEQ_NUM;
                aux2->next = NULL;
                aux->next = (struct seglst_t *)aux2;
                aux = aux2;
                ret += aux2->size;
            }
            return ret;
        }
        else
        {
            insert_seg_lst(lst_aux, seg, size);
            return 0;
        }
    }
    else
    {
        aux = (*lst);
        while(aux->next != NULL)
        {
            aux = (seglst_t *)aux->next;
        }
        if(seg->seq_num == base)
        {
            aux->next = (struct seglst_t *)malloc(sizeof(seglst_t));
            aux = (seglst_t *)aux->next;
            aux->segm = seg;
            aux->size = size;
            aux->next = NULL;
            
            ret = size;
            count = (count + size) % MAX_SEQ_NUM;
            while(1==1)
            {
                aux2 = is_next(lst_aux, count);
                if(aux2 == NULL)
                {
                    break;
                }
                count = (count + aux2->size) % MAX_SEQ_NUM;
                aux2->next = NULL;
                aux->next = (struct seglst_t *)aux2;
                aux = aux2;
                ret += aux2->size;
            }
            return ret;
        }
        else
        {
            insert_seg_lst(lst_aux, seg, size);
            return 0;
        }
    }    
}

PUBLIC size_t recv_seg(seglst_t **list, byte *buffer, size_t size)
{
    size_t recv_size = size;
    size_t offset = 0;
    seglst_t *aux;
    byte *buf_aux;
    
    while(recv_size != 0)
    {
        if((*list) != NULL)
        {
            if(recv_size >= (*list)->size)
            {
                memcpy(buffer + offset, (*list)->segm->data, (*list)->size);
                offset += (*list)->size;
                recv_size -= (*list)->size;
                free((*list)->segm->data);
                free((*list)->segm);
                aux = (*list);
                (*list) = (seglst_t *)(*list)->next;
                free(aux);
            }
            else
            {
                memcpy(buffer + offset, (*list)->segm->data, recv_size);
                buf_aux = (byte *)malloc(sizeof(byte) * ((*list)->size - recv_size));
                memcpy(buf_aux, (*list)->segm->data + recv_size, ((*list)->size - recv_size));
                free((*list)->segm->data);
                (*list)->segm->data = buf_aux;
                (*list)->size -= recv_size;
                recv_size -= recv_size;
            }            
        }
        else
        {
            break;
        }
    }
    return (size - recv_size);
}

PUBLIC void rm_seg_lst(seglst_t **lst)
{
    seglst_t *aux = (*lst);
    while((*lst) != NULL)
    {
        aux = (*lst);
        (*lst) = (seglst_t *)(*lst)->next;
        free(aux->segm->data);
        free(aux->segm);
        free(aux);
    }
}