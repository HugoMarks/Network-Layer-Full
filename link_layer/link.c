/* Arquivo: link.c
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#include <link.h>
#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <common.h>
#include <pthread.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <checksum.h>
#include <test_layer.h>
#include <garbler.h>

/*
*	Camada de enlace.
*	Responsavel por receber um dado do vizinho e enviar um
*	dado para um vizinho. 
*
**/

#ifdef DEBUG2
#define LOG(s, ...) logdebug(stdout, LINK, s, ##__VA_ARGS__)
#else
#define LOG(s, ...)
#endif

PRIVATE uint16_t msg_content;
PRIVATE int send_msqid;
PRIVATE int socket_h;
PRIVATE struct sockaddr_in socket_host, socket_client, socket_dest;
PRIVATE host_t *me;

byte *buf_rcv;

/*
*Buffers para enviar e receber
*/
PRIVATE frame_t send_frame;
frame_t recv_frame;

/*
*Mutex para enviar e receber
*/
PRIVATE pthread_mutex_t send_mutex;
PRIVATE pthread_mutex_t intersend_mutex;

PUBLIC pthread_mutex_t link_recv_mutex;
PUBLIC pthread_mutex_t net_recv_mutex;

PRIVATE void *internalsend(void *param);
PRIVATE void *internalrecv(void *param);
PRIVATE int cansend(uint16_t other);
PRIVATE void buildsendframe(byte *buf);
PRIVATE void getrecvframe(byte *buf,size_t size);

int max;

PUBLIC int link_init(uint16_t num, pthread_mutex_t link, pthread_mutex_t net, frame_t **link_recv_frame,int max_mtu)
{
	pthread_t send_thread;
	pthread_t recv_thread;
	LOG("MTU: %d",max_mtu);
	max = max_mtu;
	buf_rcv = ALLOC(byte,max_mtu);

	/*Inicialização dos mutex de envio*/
	pthread_mutex_init(&send_mutex, NULL);
	pthread_mutex_init(&intersend_mutex, NULL);
	/*recebimento dos mutex recebidos por parametro de projeto.c.
	*	estes mutex são utilizados para sincronismo entre a camada de
	*	enlace e de rede.
	*/
	link_recv_mutex = link;
	net_recv_mutex = net;
	/*copiando o endereço de estrutura de recebimento:
	*	para que a camada de rede possa ter acesso ao pacote.
	*/
	(*link_recv_frame) = &recv_frame;
	/*Pegar a estrutura host_t do proprio nó.*/
	me = findhost(num);	
	LOG("criando socket...");

	if ((socket_h = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		return ERROR;
	}

	socket_host.sin_family = AF_INET;
	socket_host.sin_port = htons(me->port);
	socket_host.sin_addr.s_addr = me->ip_addr;

	if (bind(socket_h, (struct sockaddr *)&socket_host, sizeof(socket_host)) < 0)
	{
		perror("bind");
		return ERROR;
	}
	
	set_garbler(0,0,0);
	#ifdef DILMA
	set_garbler(0,100,0);
	#endif
	#ifdef LOST
	set_garbler(100,0,0);
	#endif
	#ifdef DUBLE
	set_garbler(0,0,100);
	#endif
	#ifdef YOLO
	set_garbler(25,25,25);
	#endif
	
	/*
	 *	Iniciando o mutex de send travado e criação da thread send
	 */    
	pthread_mutex_lock(&send_mutex);
	pthread_create(&send_thread, NULL, internalsend, NULL);
	pthread_detach(send_thread);
        
	//criação da thread de recebimento
	pthread_create(&recv_thread, NULL, internalrecv, NULL);
	pthread_detach(recv_thread);
	LOG("init feito");
	return OK;
}

PUBLIC int link_send(uint16_t dest, byte *data, size_t size)
{
	host_t *send_host;
	int mtu;

	LOG("enviando para %u", dest);

	/*Verificação se o destino é vizinho.*/
	if (!cansend(dest))
	{
		LOG("o destino %d nao e alcancavel", dest);
		errno = ENETUNREACH;
		return ERROR;
	}

	LOG("o destino %u e conhecido", dest);
	/*Verificação de MTU*/
	mtu = getmtu(me->num, dest);

	if (mtu <= size)
	{
		LOG("o pacote de tamanho %d bytes e maior que a MTU (%d bytes)", size, mtu);
		errno = EMSGSIZE;
		return ERROR;
	}

	LOG("mutext travado para enviar");
	pthread_mutex_lock(&intersend_mutex);
	send_frame.dest = dest;
	send_frame.size = size;
	send_frame.data = ALLOC(byte, size );
	memcpy(send_frame.data, data, size);
	LOG("dados a enviar: %s (%zu bytes)", send_frame.data, send_frame.size);
	LOG("checksum dos dados: %#04x", send_frame.checksum);
	send_host = findhost(dest);
	socket_dest.sin_family = AF_INET;
	socket_dest.sin_port = htons(send_host->port);
	socket_dest.sin_addr.s_addr = send_host->ip_addr;
	LOG("enviar para %s:%u", inet_ntoa(socket_dest.sin_addr),send_host->port);
	pthread_mutex_unlock(&send_mutex);
	return OK;
}

PRIVATE void *internalsend(void *param)
{
	byte *buf;

	LOG("thread para enviar iniciada...");

	while (TRUE)
	{
		pthread_mutex_lock(&send_mutex);
		LOG("enviando: origem: %d", me->num);
		LOG("enviando: destino: %d", send_frame.dest);
		LOG("enviando: data: %s", send_frame.data);
		LOG("enviando: tamanho: %zu bytes", send_frame.size + (2 * sizeof(uint16_t)) + sizeof(size_t));
		buf = ALLOC(byte,send_frame.size + LINK_HEADER_SIZE + CHECKSUM_SIZE);
		buildsendframe(buf);
		
		
		
		sendto_garbled(socket_h, buf, send_frame.size + LINK_HEADER_SIZE + CHECKSUM_SIZE, 0, (struct sockaddr *)&socket_dest, sizeof(socket_dest));
		free(buf);
		free(send_frame.data);
		pthread_mutex_unlock(&intersend_mutex);
	}
}

PUBLIC void config_garbler(int l, int c, int d)
{
	set_garbler(l,c,d);
}

PRIVATE void *internalrecv(void *param)
{
	int sizeclient;
	size_t size;
	sizeclient = sizeof(socket_client);
	LOG("thread para receber iniciada...");

	while(TRUE)
	{
		uint16_t rcv_checksum = 1;
		
		//espera por liberação da camada de rede para o próximo recebimento
		pthread_mutex_lock(&link_recv_mutex);
		LOG("mutex travado para receber");

		while (rcv_checksum)
		{
			size = recvfrom(socket_h, buf_rcv, max, 0, (struct sockaddr *)&socket_client, &sizeclient);
			LOG("\n");
			LOG("checksum recebido : %#04x", recv_frame.checksum);
			recv_frame.checksum = *((uint16_t *)(buf_rcv + size - CHECKSUM_SIZE));
			LOG("check: %u",recv_frame.checksum);
			rcv_checksum = checksum(buf_rcv, size - CHECKSUM_SIZE);
			LOG("checksum calculado: %#04x", rcv_checksum);
			rcv_checksum = rcv_checksum - recv_frame.checksum;
			LOG("resultado do checksum: %#04x", rcv_checksum);
			
			if (!rcv_checksum)
			{
				
				getrecvframe(buf_rcv,size);
				LOG("checksum correto!");
				LOG("recebeu: destino: %d", recv_frame.dest);
				LOG("recebeu: tamanho: %zu", recv_frame.size);
				LOG("recebeu: data: %s", recv_frame.data);
				break;
			}


			LOG("checksum incorreto!");
			LOG("o pacote sera descartado");
			to_above_f_below(recv_frame);
			
		}

		LOG("mutex liberado para camada de rede");

		//libera camada de rede para pegar o pacote
		pthread_mutex_unlock(&net_recv_mutex);
		LOG("recebimento terminado");
	}
}
/*criação do pacote de envio*/
PRIVATE void buildsendframe(byte *buf)
{
	memcpy(buf, &send_frame.dest, sizeof(uint16_t));
	memcpy(buf + sizeof(uint16_t), &send_frame.size, sizeof(size_t));
	memcpy(buf + LINK_HEADER_SIZE, send_frame.data, send_frame.size);
	send_frame.checksum = checksum(buf, send_frame.size + LINK_HEADER_SIZE);
	memcpy(buf + LINK_HEADER_SIZE + send_frame.size, &send_frame.checksum, CHECKSUM_SIZE);
}
/*criação do pacote de recebimento*/
PRIVATE void getrecvframe(byte *buf,size_t size)
{
	recv_frame.dest = *((uint16_t *)buf);
	
	recv_frame.size = *((size_t *)(buf + sizeof(uint16_t)));
	
	recv_frame.data = ALLOC(byte,size - LINK_HEADER_SIZE - CHECKSUM_SIZE);
	memcpy(recv_frame.data, buf + LINK_HEADER_SIZE, recv_frame.size);
	
}
/*função para verificação se é vizinho*/
PRIVATE int cansend(uint16_t other)
{
	return getmtu(me->num, other) == -1 ? FALSE : TRUE;
}

