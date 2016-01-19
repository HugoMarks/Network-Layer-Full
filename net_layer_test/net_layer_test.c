/* Arquivo: net_layer_test.c
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */

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
#include <net_layer.h>
#include <net_layer_test.h> 
#include <unistd.h>

#ifdef DEBUG
#define LOG(s, ...) logdebug(stdout, TRANSPORT, s, ##__VA_ARGS__)
#else
#define LOG(s, ...)
#endif


PRIVATE void *internalsend(void *param);
PRIVATE void *internalrecv(void *param);


frame_t test_recv_frame;
uint16_t num_global;


PUBLIC pthread_mutex_t transp_recv_mutex;
PUBLIC pthread_mutex_t net2_recv_mutex;

netdata_t recv_datagram;

PUBLIC int net_layer_test_init(uint16_t num, pthread_mutex_t transp, pthread_mutex_t net2, netdata_t *net_recv_datagram)
{

    pthread_t send_thread;
    pthread_t recv_thread;

    transp_recv_mutex = transp;
    net2_recv_mutex = net2;
    num_global = num;
    recv_datagram = (*net_recv_datagram);
    //criação da thread de envio
    pthread_create(&send_thread, NULL, internalsend, NULL);
    pthread_detach(send_thread);
    
    //criação da thread de recebimento
    pthread_create(&recv_thread, NULL, internalrecv, NULL);
    pthread_detach(recv_thread);
    
    LOG("test init");
    return OK;

}

/*thread que recebe da camada de baixo*/
PRIVATE void *internalsend(void *param)
{
    while(TRUE)
    {
        sleep(3);
        if(num_global == 1)
        { 
            LOG("enviando");
            LOG("retorno: %d",net_send(2,"testext1stextest2xtestext3stextest4xtest5xtest6xtest7x",55,1));
            LOG("envio terminado");
        }
            
        
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
        pthread_mutex_lock(&transp_recv_mutex);
        LOG("teste -> recebeu: numero: %i",i);
     
        LOG("teste -> recebeu: data: %s", recv_datagram.data);
        LOG("teste -> recebeu: size: %zu", recv_datagram.size);
      
        
        i++;

        //free(recv_frame.data);
        //recv_frame.data = NULL;
        LOG("teste: mutex liberado para camada de rede");
       
        pthread_mutex_unlock(&net2_recv_mutex);

        LOG("teste: recebimento terminado");
      
    }

}
