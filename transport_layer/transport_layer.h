/* Arquivo: transport_layer.h
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
 
#ifndef TRANSPORT_LAYER_H
#define TRANSPORT_LAYER_H

#include <inttypes.h>
#include <config.h>
#include <sys/types.h>
#include <link.h>
#include <net_layer.h>

#define MAX_SERVICE_POINTS 16
#define MAX_SEQ_NUM 4294967295
#define MAX_SEGMENT_SIZE 512
#define TRANSPORT_HEADER_SIZE ((sizeof(uint16_t) * 4) + (sizeof(uint32_t) * 2) + sizeof(uint8_t))
#define TIMEOUT	3 //*****

typedef struct segment {
	uint16_t src;
	uint16_t dest;
	uint32_t seq_num;
	uint32_t ack_num;
	uint8_t	 flags; //0 = dados, 1 = sync, 2 = sync+ack, 3 = ack, 4 = fin
	uint16_t window_size;
	byte *data;
	uint16_t checksum;
} segment_t;

typedef struct ack_lst {
    uint32_t seq_num;
    uint8_t flag;
    size_t size;
    struct ack_lst_t *next;
} ack_lst_t;

typedef struct send_segment {
	segment_t *segment;
    size_t size;
} send_segment_t;

typedef struct seglst {
	segment_t *segm;
	size_t size;
	struct seglst_t *next;
} seglst_t;

typedef struct recv_segment {
	segment_t *segment;
	size_t size;
    uint16_t src;
} recv_segment_t;

typedef struct servicepoint {
	//no de destino (depois do conectar)
	uint16_t dest;
	//pontoServico destino (depois do conectar)
	uint16_t dest_sp;
	//tamanho da janela
	uint16_t window_size;
	
	//flag para sinalizar conexao
	uint8_t flag_con; //0 = sem conexao, 1 = estabelecendo conexao, 2 = com conexao, 3 = encerrando conexao
	//mutex conexao
	pthread_mutex_t mutex_con;
	
	//numero de sequencia envio(depois do conectar e atualizado com os envios)
	uint32_t seq_num_s;
    //numero de sequencia ant envio(num_seq - tamanho da janela)
	uint32_t prev_seq_num_s;
    //numero de sequencia fim envio(num_seq + tamanho da janela)
	uint32_t end_seq_num_s;
	//Mutex num_seq_envio
	pthread_mutex_t mutex_s;
	
	//numero de sequencia recebimento (depois de receber um sync e com os envios)
	uint32_t seq_num_r;
    //numero de sequencia ant recebimento
	uint32_t prev_seq_num_r;
    //numero de sequencia fim recebimento
	uint32_t end_seq_num_r;
	//Mutex num_seq_recebimento
	pthread_mutex_t mutex_r;
	
	//lista de ACKs
	ack_lst_t *acks;
	//Mutex para lista de ACKs
	pthread_mutex_t mutex_ack;
	pthread_mutex_t mutex_recv_ack;
	
	//lista de DADOS
	seglst_t *segs;
	seglst_t *segs_aux;
    //Mutex para lista de DADOS
	pthread_mutex_t mutex_seg;
	
	//Buffer de envio
	byte *send_buff;
	size_t send_size;
    //Mutex de produz envio
	pthread_mutex_t mutex_prod;
    //Mutex de consome envio (inicia LOCK)
	pthread_mutex_t mutex_cons;
    
	pthread_t thread_recv;
	pthread_t thread_send;
	
    //buffer de pacote nao tratado (1 posicao)
	recv_segment_t wait_buff;
    //Mutex de consome_pacote_nao_tratado (inicia LOCK)
	pthread_mutex_t mutex_pack_cons;
    //Mutex de produz_pacote_nao_tratado
	pthread_mutex_t mutex_pack_prod;
} servicepoint_t;

//Aloca ponto de servico
PUBLIC uint16_t alloc_service_point();

//Desaloca ponto de servico
PUBLIC void dalloc_service_point(uint16_t point);

//Conectar a um ponto
PUBLIC uint16_t connect_point(uint16_t point, uint16_t dest, uint16_t dest_point, size_t window_size);

//Fecha uma conexão
PUBLIC void close_conn(uint16_t point);

//Envia para um ponto de serviço
PUBLIC size_t send_point(uint16_t point, byte *buff, size_t size);

//Recebe de um ponto de serviço
PUBLIC size_t recv_point(uint16_t point, byte *buff, size_t size);

PUBLIC int transport_init(uint16_t num, pthread_mutex_t transp, pthread_mutex_t net2, netdata_t *net_recv_datagram);

PUBLIC void depurar();

#endif