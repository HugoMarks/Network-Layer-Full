/* Arquivo: ack_lst.c
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
 
#include <ack_lst.h>

/* função para inserir acks no final da lista */
PUBLIC void insert_ack(ack_lst_t **lista, ack_lst_t *novo)
{
    ack_lst_t *aux, *prev = NULL;
    if((*lista) == NULL)
    {
        (*lista) = novo;
    }
    else
    {
        aux = (*lista);
        while(aux->next != NULL)
        {
            aux = (ack_lst_t *)aux->next;
        }
        aux->next = (struct ack_lst_t *)novo;
    }    
}

/* função para remover ack pelo número de sequência */
PUBLIC void rm_ack(ack_lst_t **lista, uint32_t num)
{
    ack_lst_t *prev, *aux = (*lista);
    while(aux != NULL && aux->seq_num != num)
    {
        prev = aux;
        aux = (ack_lst_t *)aux->next;
    }
    if(aux != NULL)
    {
        if(aux->seq_num == num)
        {
            if(prev == NULL)
            {
                (*lista) = (ack_lst_t *)aux->next;
            }
            else
            {
                prev->next = aux->next;
                free(aux);
            }
        }
    }
}

/* função para setar um ack pelo número de sequência */
PUBLIC void set_ack(ack_lst_t *list, uint32_t num)
{
    while(list != NULL)
    {
        if(list->seq_num == num)
        {
            list->flag = 1;
            break;
        }
        list = (ack_lst_t *)list->next;
    }
}

/* função para listar os acks da lista */
PUBLIC void list_ack(ack_lst_t *list)
{
    while(list != NULL)
    {
        printf("num_seq: %d, set: %d\n",list->seq_num, list->flag);
        list = (ack_lst_t *)list->next;
    }
}

/* função para verificar se um ack está setado */
PUBLIC uint8_t is_set(ack_lst_t *list, uint32_t num)
{
    while(list != NULL)
    {
        if(list->seq_num == num)
        {
            return list->flag;
        }
        list = (ack_lst_t *)list->next;
    }
    return 1;
}

/* função para retornar o tamanho que a janela irá avançar */
PUBLIC size_t in_seq(ack_lst_t **list)
{
    size_t size = 0;
    ack_lst_t *aux;
    while((*list) != NULL)
    {
        if((*list)->flag == 0)
        {
            break;
        }
        size += (*list)->size;
        aux = (*list);
        (*list) = (ack_lst_t *)(*list)->next;
        free(aux);
    }
    return size;
}

PUBLIC void rm_ack_lst(ack_lst_t **lst)
{
    ack_lst_t *aux = (*lst);
    while((*lst) != NULL)
    {
        aux = (*lst);
        (*lst) = (ack_lst_t *)(*lst)->next;
        free(aux);
    }
}