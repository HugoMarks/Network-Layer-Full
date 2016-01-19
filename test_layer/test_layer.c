/* Arquivo: test_layer.c
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#include <link.h>
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common.h>
#include <pthread.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <checksum.h>
#include <test_layer.h> 

#ifdef DEBUG
#define LOG(s, ...) logdebug(stdout, NETWORK, s, ##__VA_ARGS__)
#else
#define LOG(s, ...)
#endif


PRIVATE void *internalsend(void *param);
PRIVATE void *internalrecv(void *param);


frame_t test_recv_frame;
uint16_t num_global;


PUBLIC pthread_mutex_t link_recv_mutex;
PUBLIC pthread_mutex_t net_recv_mutex;

frame_t recv_frame;

PUBLIC void to_above_f_below(frame_t data)
{
    recv_from_below(data);
}

PUBLIC void recv_from_below(frame_t data)
{
    test_recv_frame = data; 
}

PUBLIC int test_init(uint16_t num, pthread_mutex_t link, pthread_mutex_t net, frame_t *link_recv_frame)
{

    pthread_t send_thread;
    pthread_t recv_thread;

    link_recv_mutex = link;
    net_recv_mutex = net;
    num_global = num;
    recv_frame = *link_recv_frame;
    
    //criação da thread de envio
    pthread_create(&send_thread, NULL, internalsend, NULL);
    pthread_detach(send_thread);
    
    //criação da thread de recebimento
    pthread_create(&recv_thread, NULL, internalrecv, NULL);
    pthread_detach(recv_thread);
    
    return OK;

}

/*thread que recebe da camada de baixo*/
PRIVATE void *internalsend(void *param)
{
    while(TRUE)
    {
        //sleep(7);
        if(num_global == 1)
            link_send(2,"teste",6);
        
    }
}

/*thread que recebe da camada de baixo*/
PRIVATE void *internalrecv(void *param)
{

    int i = 0;
    while(num_global != 2)
    {

    }
    while(TRUE)
    {
        //espera por liberação da camada de enlace para o próximo recebimento
        // nao sei se vai funcionar
        
        
        LOG("teste: mutex travado para receber");
        pthread_mutex_lock(&net_recv_mutex);
        LOG("\nteste -> recebeu: numero: %i",i);
     
        //LOG("teste -> recebeu: origem: %d\n", recv_frame.orig);
        //LOG("teste -> recebeu: destino: %d\n", recv_frame.dest);
        LOG("teste -> recebeu: data: %s", recv_frame.data);
      
        
        i++;

        free(recv_frame.data);
        recv_frame.data = NULL;
        LOG("teste: mutex liberado para camada de enlace");
       
        pthread_mutex_unlock(&link_recv_mutex);

        LOG("teste: recebimento terminado");
      
    }

}
