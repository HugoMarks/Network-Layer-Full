/* Arquivo: transport_layer.c
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
#include <checksum.h>
#include <net_layer.h>
#include <transport_layer.h>
#include <sys/time.h>
#include <ack_lst.h>
#include <seg_lst.h>
#include <unistd.h>

#ifdef DEBUG
#define LOG(s, ...) logdebug(stdout, TRANSPORT, s, ##__VA_ARGS__)
#else
#define LOG(s, ...)
#endif

PRIVATE int getfreeslot();
PRIVATE void *send_thread(void *param);
PRIVATE void *recv_thread(void *param);
PRIVATE void *internalrecv(void *param);
PRIVATE void *seg_send_thread(void *param);
PRIVATE void close_conn_aux(uint16_t point, uint8_t flag, recv_segment_t recv);

PRIVATE servicepoint_t *service_points[MAX_SERVICE_POINTS];

PUBLIC segment_t *segmento;

PRIVATE time_t seconds;

PRIVATE uint16_t num_global;
PUBLIC pthread_mutex_t transp_recv_mutex;
PUBLIC pthread_mutex_t net2_recv_mutex;
PUBLIC netdata_t recv_datagram;

/* função destinada inicializar a camada de transporte*/
PUBLIC int transport_init(uint16_t num, pthread_mutex_t transp, pthread_mutex_t net2, netdata_t *net_recv_datagram)
{
	int i;
    pthread_t recv_thread;
    transp_recv_mutex = transp;
    net2_recv_mutex = net2;
    num_global = num;
    recv_datagram = (*net_recv_datagram);
    
	for (i = 0; i < MAX_SERVICE_POINTS; i++)
	{
		service_points[i] = NULL;
	}
    
    //criação da thread de recebimento
    pthread_create(&recv_thread, NULL, internalrecv, NULL);
    pthread_detach(recv_thread);
    
    return OK;
}
/* função para camada de aplicação liberar seu ponto de serviço*/
PUBLIC void dalloc_service_point(uint16_t point)
{
    if(service_points[point] != NULL)
    {
        rm_ack_lst(&service_points[point]->acks);
        rm_seg_lst(&service_points[point]->segs);
        rm_seg_lst(&service_points[point]->segs_aux);
        pthread_cancel(service_points[point]->thread_recv);
        pthread_cancel(service_points[point]->thread_send);
        pthread_mutex_destroy(&service_points[point]->mutex_con);
        pthread_mutex_destroy(&service_points[point]->mutex_s);
        pthread_mutex_destroy(&service_points[point]->mutex_r);
        pthread_mutex_destroy(&service_points[point]->mutex_ack);
        pthread_mutex_destroy(&service_points[point]->mutex_recv_ack);
        pthread_mutex_destroy(&service_points[point]->mutex_seg);
        pthread_mutex_destroy(&service_points[point]->mutex_prod);
        pthread_mutex_destroy(&service_points[point]->mutex_cons);
        pthread_mutex_destroy(&service_points[point]->mutex_pack_cons);
        pthread_mutex_destroy(&service_points[point]->mutex_pack_prod);
        free(service_points[point]);
        service_points[point] = NULL;
    }
}
/* função para camada de aplicação alocar um ponto de serviço */
PUBLIC uint16_t alloc_service_point()
{
	int slot;
	servicepoint_t *sp;
    uint16_t *sp_num;
	pthread_t r_thread;
	//Procura por um slot livre
	slot = getfreeslot();
	
	if (slot == ERROR)
	{
		return ERROR;
	}
	
	//Aloca um ponto de serviço
	sp = ALLOC(servicepoint_t, sizeof(servicepoint_t));
	
	/*
	 * Inicializando os campos
	 */
    sp->dest = 0;
    sp->dest_sp = 0;
    sp->flag_con = 0;
	//Buffers
	sp->wait_buff.segment = NULL;
	sp->send_buff = NULL;
	//Listas
	sp->acks = NULL;
	sp->segs = NULL;
    sp->segs_aux = NULL;
	//Mutex
    pthread_mutex_init(&sp->mutex_con, NULL);
	pthread_mutex_init(&sp->mutex_s, NULL);
	pthread_mutex_init(&sp->mutex_r, NULL);
	pthread_mutex_init(&sp->mutex_ack, NULL);
    pthread_mutex_init(&sp->mutex_recv_ack, NULL);
	pthread_mutex_init(&sp->mutex_seg, NULL);
	pthread_mutex_init(&sp->mutex_prod, NULL);
	pthread_mutex_init(&sp->mutex_cons, NULL);
	pthread_mutex_init(&sp->mutex_pack_cons, NULL);
	pthread_mutex_init(&sp->mutex_pack_prod, NULL);
	pthread_mutex_lock(&sp->mutex_recv_ack);
	//Inicializa o mutex do consumidor como travado
	pthread_mutex_lock(&sp->mutex_cons);
	//Inicializa o mutex do consumidor (pacotes nao consumidos) como travado	
	pthread_mutex_lock(&sp->mutex_pack_cons);
	
	service_points[slot] = sp;
    
	
    sp_num = ALLOC(uint16_t, sizeof(uint16_t));
    memcpy(sp_num, &slot, sizeof(uint16_t));
	//criação da thread de recebimento
    pthread_create(&service_points[slot]->thread_recv, NULL, recv_thread, sp_num);
    pthread_detach(service_points[slot]->thread_recv);
    //criação da thread de envio
    pthread_create(&service_points[slot]->thread_send, NULL, send_thread, sp_num);
    pthread_detach(service_points[slot]->thread_send);
    
	return (uint16_t)slot;
}
/* função para camada de aplicação estabeler uma conexão */
PUBLIC uint16_t connect_point(uint16_t point, uint16_t dest, uint16_t dest_point, size_t window_size)
{
	struct timeval semente;
    uint16_t *sp_num;
	pthread_t s_thread, thread;
    send_segment_t *pack;
    char s = 's';
    byte *buff;
    ack_lst_t *new_ack;
	//verificar ponto de serviço
	if(service_points[point] == NULL)
	{
        return 0;
	}
	else
	{
        //verifica se existe conexao nesse ponto de serviço
        pthread_mutex_lock(&service_points[point]->mutex_con);
        if(service_points[point]->flag_con == 0)
        {
            
            int i=0;
            uint32_t num_seq;
            uint32_t aux;
    
            //montar o segmento
    
            //escolhe numero de seq de envio
            num_seq = rand() % MAX_SEQ_NUM;
            num_seq = (num_seq + window_size) % MAX_SEQ_NUM;
            service_points[point]->dest = dest;
            service_points[point]->dest_sp = dest_point;
            service_points[point]->window_size = window_size;
            service_points[point]->seq_num_s = num_seq;
            
            //insert_ack	
            new_ack = ALLOC(ack_lst_t, sizeof(ack_lst_t));
            new_ack->seq_num = (service_points[point]->seq_num_s + 1) % MAX_SEQ_NUM;
            new_ack->flag = 0;
            new_ack->size = 0;
            new_ack->next = NULL;
            
            pthread_mutex_lock(&service_points[point]->mutex_ack);
            insert_ack(&service_points[point]->acks, new_ack);
            pthread_mutex_unlock(&service_points[point]->mutex_ack);
            
            //envia SYNC
            pack = ALLOC(send_segment_t, sizeof(send_segment_t));
            pack->segment = ALLOC(segment_t, sizeof(segment_t));
            pack->segment->src = point;
            pack->segment->dest = service_points[point]->dest_sp;
            pack->segment->seq_num = service_points[point]->seq_num_s;
            
            service_points[point]->seq_num_s = (service_points[point]->seq_num_s + 1) % MAX_SEQ_NUM;
            service_points[point]->prev_seq_num_s = (service_points[point]->seq_num_s + (MAX_SEQ_NUM - (uint32_t)window_size)) % MAX_SEQ_NUM;
            service_points[point]->end_seq_num_s = (service_points[point]->seq_num_s + (uint32_t)window_size) % MAX_SEQ_NUM;
            
            pack->segment->flags = 1;
            pack->segment->window_size = service_points[point]->window_size;
            pack->segment->data = ALLOC(byte, sizeof(byte));
            memcpy(pack->segment->data, &s, sizeof(byte));
            pack->size = 1;
            
            //cria a thread de envio (ela fara reenvio se nao chegar ack)
            pthread_create(&thread, NULL, seg_send_thread, pack);
            pthread_detach(thread);
            
            service_points[point]->flag_con = 1;
            pthread_mutex_unlock(&service_points[point]->mutex_con);
            
            //sp_num = ALLOC(uint16_t, sizeof(uint16_t));
            //memcpy(sp_num, &point, sizeof(uint16_t));
            //criar thread de envio
            //pthread_create(&s_thread, NULL, send_thread, sp_num);
            //pthread_detach(s_thread);
            
            return 1;
        }
        else
        {
            //esse ponto de serviço já possui conexão
            pthread_mutex_unlock(&service_points[point]->mutex_con);
            //retornar erro
            return 0;
        }
	}
}
/* função para camada de aplicação fechar conexão */
PUBLIC void close_conn(uint16_t point)
{
    segment_t *segment;
    ack_lst_t *new_ack;
    send_segment_t *send_seg;
    byte *buf;
    char fin = 'f';
    pthread_t thread;
    if(service_points[point] == NULL)
    {
        return;
    }
    pthread_mutex_lock(&service_points[point]->mutex_con);
    if(service_points[point]->flag_con == 2)
    {
        //cria segmento para envio
        segment = ALLOC(segment_t, sizeof(segment_t));
        segment->src = point;
        segment->dest = service_points[point]->dest_sp;
        pthread_mutex_lock(&service_points[point]->mutex_s);
        segment->seq_num = service_points[point]->seq_num_s;
        segment->flags = 4;
        segment->window_size = service_points[point]->window_size;
        segment->data = ALLOC(byte, 1);
        memcpy(segment->data, &fin, 1);
        
        //cria ack esperado na lista de acks
        new_ack = ALLOC(ack_lst_t, sizeof(ack_lst_t));
        service_points[point]->seq_num_s = (service_points[point]->seq_num_s + 1) % MAX_SEQ_NUM;
        new_ack->seq_num = service_points[point]->seq_num_s;
        new_ack->flag = 0;
        new_ack->size = 1;
        new_ack->next = NULL;
        pthread_mutex_lock(&service_points[point]->mutex_ack);
        insert_ack(&service_points[point]->acks, new_ack);
        pthread_mutex_unlock(&service_points[point]->mutex_ack);
        
        pthread_mutex_unlock(&service_points[point]->mutex_s);
        
        send_seg = ALLOC(send_segment_t, sizeof(send_segment_t));
        send_seg->segment = segment;
        send_seg->size = 1;
        //cria a thread de envio (ela fara reenvio se nao chegar ack)
        pthread_create(&thread, NULL, seg_send_thread, send_seg);
        pthread_detach(thread);
        
        service_points[point]->flag_con = 3;
        pthread_mutex_unlock(&service_points[point]->mutex_con);
    }
    else
    {
        pthread_mutex_unlock(&service_points[point]->mutex_con);
    }
}


PRIVATE int getfreeslot()
{
	int i;
	
	for (i = 0; i < MAX_SERVICE_POINTS; i++)
	{
		if (service_points[i] == NULL)
		{
			return i;
		}
	}
	
	return ERROR;
}

/* função para camada de aplicação enviar dados */
PUBLIC size_t send_point(uint16_t sp, byte *buffer, size_t tamanho)
{
    //aloca um buffer com o tamanho (max = (32768 - cabecalho_transporte))
    size_t tam;
    if(service_points[sp] != NULL)
    {
        pthread_mutex_lock(&service_points[sp]->mutex_con);
        if(service_points[sp]->flag_con != 2)
        {
            pthread_mutex_unlock(&service_points[sp]->mutex_con);
            return 0; //********
        }
        pthread_mutex_unlock(&service_points[sp]->mutex_con);
        if(tamanho > (32768 - TRANSPORT_HEADER_SIZE))
        {
            tam = (32768 - TRANSPORT_HEADER_SIZE);
        }
        else
        {
            tam = tamanho;
        }
        byte *buf = ALLOC(byte, tam);
        if(buf != NULL)
        {
            //faz memcpy do buffer
            memcpy(buf, buffer, tam);
            //LOCK mutex_produz_envio
            pthread_mutex_lock(&service_points[sp]->mutex_prod);
            //coloca o buffer no ponto de servico
            service_points[sp]->send_buff = buf;
            service_points[sp]->send_size = tam;
            //UNLOCK mutex_consome_envio
            pthread_mutex_unlock(&service_points[sp]->mutex_cons);
            //retorna bytes copiados
            return tam;
        }
        else
        {
            //Nao foi possivel alocar buffer para copiar
            return 0;
        }
    }
    return 0;
}

/* função para camada de aplicação receber dados */
PUBLIC size_t recv_point(uint16_t point, byte *buff, size_t size)
{
    size_t s;
    if(service_points[point] != NULL)
    {
        pthread_mutex_lock(&service_points[point]->mutex_seg);
        s = recv_seg(&service_points[point]->segs, buff, size);
        pthread_mutex_unlock(&service_points[point]->mutex_seg);
        return s;
    }
    else
    {
        return 0;
    }
}

/*criação do pacote de envio*/
PRIVATE void buildsendsegment(byte *buf, send_segment_t *pack)
{
    size_t count = 0;
	memcpy(buf, &pack->segment->src, sizeof(pack->segment->src));
    count += sizeof(pack->segment->src);
    memcpy(buf + count, &pack->segment->dest, sizeof(pack->segment->dest));
    count += sizeof(pack->segment->dest);
    memcpy(buf + count, &pack->segment->seq_num, sizeof(pack->segment->seq_num));
    count += sizeof(pack->segment->seq_num);
    memcpy(buf + count, &pack->segment->ack_num, sizeof(pack->segment->ack_num));
    count += sizeof(pack->segment->ack_num);
    memcpy(buf + count, &pack->segment->flags, sizeof(pack->segment->flags));
    count += sizeof(pack->segment->flags);
    memcpy(buf + count, &pack->segment->window_size, sizeof(pack->segment->window_size));
    count += sizeof(pack->segment->window_size);
	if(pack->size != 0)
    {
        memcpy(buf + count, pack->segment->data, pack->size);
        count += pack->size;
    }
    pack->segment->checksum = checksum(buf, count);
	memcpy(buf + count, &pack->segment->checksum, CHECKSUM_SIZE);
}

/* thread de envio para um pedaco da janela de transmissao */
PRIVATE void *seg_send_thread(void *param) 
{
    byte *buffer;
    //obtem pacote da threadEnvio (thread Pai)
    send_segment_t *pack = (send_segment_t *)param;
    buffer = ALLOC(byte, pack->size + TRANSPORT_HEADER_SIZE);
    buildsendsegment(buffer, pack);    
    //chama funcao enviar da camada de rede:
    //printf("net_Send %d -- size %zu --- seg %u\n",net_send(service_points[pack->segment->src]->dest, buffer, pack->size + TRANSPORT_HEADER_SIZE, 1), pack->size, pack->segment->seq_num);
    if(service_points[pack->segment->src] != NULL)
    {
        net_send(service_points[pack->segment->src]->dest, buffer, pack->size + TRANSPORT_HEADER_SIZE, 1);
    }
    while (TRUE) //**** Deixa tentar infinitamente ou numero de tentativas limitado? ****
    {
        sleep(TIMEOUT);
        if(service_points[pack->segment->src] != NULL)
        {
            //LOCK ACK
            pthread_mutex_lock(&service_points[pack->segment->src]->mutex_ack);
            //meu ACK chegou?
            if(is_set(service_points[pack->segment->src]->acks, (pack->segment->seq_num + pack->size)) == 1)
            {
                pthread_mutex_unlock(&service_points[pack->segment->src]->mutex_ack);
                break;
            }
            else
            {
                pthread_mutex_unlock(&service_points[pack->segment->src]->mutex_ack);
                //printf("net_resend %d --- size %zu --- seg %u\n",net_send(service_points[pack->segment->src]->dest, buffer, pack->size + TRANSPORT_HEADER_SIZE, 1), pack->size, pack->segment->seq_num);
                net_send(service_points[pack->segment->src]->dest, buffer, pack->size + TRANSPORT_HEADER_SIZE, 1);
            }
        }
        else
        {
            break;
        }
    }
    //free(buffer); **********
    free(pack->segment->data);
    free(pack->segment);
    free(pack);
}

/* thread de envio para cada ponto de servico */
PRIVATE void *send_thread(void *param) 
{
	uint16_t sp = *(uint16_t *)param;
    segment_t *segment;
    uint16_t window_size = service_points[sp]->window_size;
    uint16_t window_ptr;
    size_t send_ptr, send_size;
    uint16_t segment_size;
    uint16_t segment_send_size;
    size_t size_move;
    ack_lst_t *new_ack;
	send_segment_t *send_seg;
	pthread_t thread;
    int done = 0;
    
    window_ptr = 0;
    
    while(TRUE)
    {
        //LOCK mutex_consome_envio
        pthread_mutex_lock(&service_points[sp]->mutex_cons);
        if(done == 0)
        {
            done = 1;
            if(service_points[sp]->window_size < MAX_SEGMENT_SIZE)
            {
                //entao a janela só terá uma posição
                segment_size = service_points[sp]->window_size;
            }
            else
            {
                //senao a janela terá mais de uma posição e a primeira será do tamanho MAX_SEGMENT_SIZE
                segment_size = MAX_SEGMENT_SIZE;
            }
            segment_send_size = segment_size;
        }
        send_ptr = 0;
        send_size = service_points[sp]->send_size;
        
        while(send_size != 0)
        {
            //cria segmento para envio
            segment = ALLOC(segment_t, sizeof(segment_t));
            segment->src = sp;
            segment->dest = service_points[sp]->dest_sp;
            pthread_mutex_lock(&service_points[sp]->mutex_s);
            segment->seq_num = service_points[sp]->seq_num_s;
            segment->flags = 0;
            segment->window_size = service_points[sp]->window_size;
            if(send_size < segment_send_size)
            {
                segment_send_size = send_size;
                send_size = 0;
            }
            else
            {
                send_size -= segment_send_size;
            }
            segment->data = ALLOC(byte, segment_send_size);
            memcpy(segment->data, service_points[sp]->send_buff + send_ptr, segment_send_size);
            send_ptr += segment_send_size;
            //cria ack esperado na lista de acks
            new_ack = ALLOC(ack_lst_t, sizeof(ack_lst_t));
            service_points[sp]->seq_num_s = (service_points[sp]->seq_num_s + segment_send_size) % MAX_SEQ_NUM;
            new_ack->seq_num = service_points[sp]->seq_num_s;
            new_ack->flag = 0;
            new_ack->size = segment_send_size;
            new_ack->next = NULL;
            pthread_mutex_lock(&service_points[sp]->mutex_ack);
            insert_ack(&service_points[sp]->acks, new_ack);
            pthread_mutex_unlock(&service_points[sp]->mutex_ack);
            pthread_mutex_unlock(&service_points[sp]->mutex_s);
            
            send_seg = ALLOC(send_segment_t, sizeof(send_segment_t));
			send_seg->segment = segment;
    		send_seg->size = segment_send_size;
            
            //cria a thread de envio (ela fara reenvio se nao chegar ack)
            pthread_create(&thread, NULL, seg_send_thread, send_seg);
            pthread_detach(thread);
            
            //avança a o ponteiro da janela
            window_ptr = (window_ptr + segment_send_size) % service_points[sp]->window_size;
            if(window_ptr == 0)
            {
                size_move = 0;
                while(size_move == 0)
                {
                    //se nao recebeu nenhum ack novo fica bloqueado
                    //pthread_mutex_lock(&service_points[sp]->mutex_recv_ack);
                    //verifica acks para poder andar com a janela
                    
                    pthread_mutex_lock(&service_points[sp]->mutex_ack);
                    size_move = in_seq(&service_points[sp]->acks);
                    pthread_mutex_unlock(&service_points[sp]->mutex_ack);
                }
                window_ptr = service_points[sp]->window_size - size_move;
                if(size_move > segment_size)
                {
                    segment_send_size = segment_size;
                }
                else
                {
                    segment_send_size = size_move;
                }
            }
            else
            {
                if((window_ptr + segment_size) > service_points[sp]->window_size)
                {
                    segment_send_size = (service_points[sp]->window_size - window_ptr);
                }
                else
                {
                    segment_send_size = segment_size;
                }
            }
        }
        //UNLOCK mutex_produz_envio
        pthread_mutex_unlock(&service_points[sp]->mutex_prod);
    }
}

/* criação do pacote de recebimento */
PRIVATE segment_t *buildrecvsegment(byte *buf, size_t size)
{
    segment_t *recv_new = ALLOC(segment_t, sizeof(segment_t));
    size_t count = 0;
	memcpy(&recv_new->src, buf, sizeof(recv_new->src));
    count += sizeof(recv_new->src);
    memcpy(&recv_new->dest, buf + count, sizeof(recv_new->dest));
    count += sizeof(recv_new->dest);
    memcpy(&recv_new->seq_num, buf + count, sizeof(recv_new->seq_num));
    count += sizeof(recv_new->seq_num);
    memcpy(&recv_new->ack_num, buf + count, sizeof(recv_new->ack_num));
    count += sizeof(recv_new->ack_num);
    memcpy(&recv_new->flags, buf + count, sizeof(recv_new->flags));
    count += sizeof(recv_new->flags);
    memcpy(&recv_new->window_size, buf + count, sizeof(recv_new->window_size));
    count += sizeof(recv_new->window_size);
    if(size > TRANSPORT_HEADER_SIZE)
    {
        recv_new->data = ALLOC(byte, size - TRANSPORT_HEADER_SIZE);
	    memcpy(recv_new->data, buf + count, size - TRANSPORT_HEADER_SIZE);
        count += (size - TRANSPORT_HEADER_SIZE);
    }
    memcpy(&recv_new->checksum, buf + count, CHECKSUM_SIZE);
    return recv_new;
}

/* thread que recebe pacotes da camada de rede */
PRIVATE void *internalrecv(void *param)
{
    netdata_t recv;
    segment_t *new_seg;
    uint16_t rcv_checksum;
    recv_segment_t new_recv;
    
    while(TRUE)
    {
        rcv_checksum = 1;
        //espera por liberação da camada de enlace para o próximo recebimento
        LOG("mutex travado para receber");
        pthread_mutex_lock(&transp_recv_mutex);
        //recebe pacote da camada de rede
        recv.data = recv_datagram.data;
        recv.size = recv_datagram.size;
        recv.src = recv_datagram.src;
        //libera a camada de rede para novo recebimento
        pthread_mutex_unlock(&net2_recv_mutex);
        
        //montagem do cabeçalho do segmento recebido
        new_seg = buildrecvsegment(recv.data, recv.size);
        //calcula o checksum do segmento recebido
        rcv_checksum = checksum(recv.data, recv.size - CHECKSUM_SIZE);
        rcv_checksum = rcv_checksum - new_seg->checksum;
        //se o checksum estiver correto
        if (!rcv_checksum)
        {
            new_recv.segment = new_seg;
            new_recv.src = recv.src;
            new_recv.size = recv.size;
            if(service_points[new_seg->dest] != NULL)
            {
                //espera por liberação para colocar o segmento recebido no ponto de serviço de destino
                pthread_mutex_lock(&service_points[new_seg->dest]->mutex_pack_prod);
                //coloca segmento recebido no ponto de serviço de destino
                service_points[new_seg->dest]->wait_buff = new_recv;
                //libera ponto de serviço para consumir segmento recebido
                pthread_mutex_unlock(&service_points[new_seg->dest]->mutex_pack_cons);
            }
            else
            {
                free(new_recv.segment);
            }
        }
        LOG("recebimento terminado");
    }
}

/* thread para estabelecer sincronismo (quando receber um SYNC) */
PRIVATE void *sync_thread(void *param) 
{
	recv_segment_t syn = *(recv_segment_t *)param;
    ack_lst_t *new_ack;
    send_segment_t *send_seg;
    segment_t *segment;
    char sync = 's';
    pthread_t thread, s_thread;
    uint16_t *sp_num;
    uint32_t sq;
    //tem destino setado no ponto de servico?
    pthread_mutex_lock(&service_points[syn.segment->dest]->mutex_con);
    if(service_points[syn.segment->dest]->flag_con == 0)
    {
        //estabelece conexão
        service_points[syn.segment->dest]->dest = syn.src;
        service_points[syn.segment->dest]->dest_sp = syn.segment->src;
		service_points[syn.segment->dest]->window_size = syn.segment->window_size;
		service_points[syn.segment->dest]->seq_num_r = (syn.segment->seq_num + 1) % MAX_SEQ_NUM;
        service_points[syn.segment->dest]->prev_seq_num_r = syn.segment->seq_num + (MAX_SEQ_NUM - (uint32_t)syn.segment->window_size) % MAX_SEQ_NUM;
		service_points[syn.segment->dest]->end_seq_num_r = (syn.segment->seq_num + (uint32_t)syn.segment->window_size) % MAX_SEQ_NUM;
        
        //escolhe um número de sequência de envio
        sq = rand() % MAX_SEQ_NUM;
        sq = (sq + syn.segment->window_size) % MAX_SEQ_NUM;
        service_points[syn.segment->dest]->seq_num_s = sq; //********
        service_points[syn.segment->dest]->prev_seq_num_s = (sq + (MAX_SEQ_NUM - (uint32_t)syn.segment->window_size)) % MAX_SEQ_NUM;
        service_points[syn.segment->dest]->end_seq_num_s = (sq + (uint32_t)syn.segment->window_size) % MAX_SEQ_NUM;
        
        //cria ack esperado na lista de acks
        new_ack = ALLOC(ack_lst_t, sizeof(ack_lst_t));
        new_ack->seq_num = (service_points[syn.segment->dest]->seq_num_s + 1) % MAX_SEQ_NUM;
        new_ack->flag = 0;
        new_ack->size = 0;
        new_ack->next = NULL;
        pthread_mutex_lock(&service_points[syn.segment->dest]->mutex_ack);
        insert_ack(&service_points[syn.segment->dest]->acks, new_ack);
        pthread_mutex_unlock(&service_points[syn.segment->dest]->mutex_ack);
        
        //enviar ACK+SYNC
        //cria segmento para envio
        segment = ALLOC(segment_t, sizeof(segment_t));
        segment->src = syn.segment->dest;
        segment->dest = syn.segment->src;
        
        pthread_mutex_lock(&service_points[syn.segment->dest]->mutex_s);
        segment->seq_num = service_points[syn.segment->dest]->seq_num_s;
        service_points[syn.segment->dest]->seq_num_s = (service_points[syn.segment->dest]->seq_num_s + 1) % MAX_SEQ_NUM;
        pthread_mutex_unlock(&service_points[syn.segment->dest]->mutex_s);
        
        segment->ack_num = service_points[syn.segment->dest]->seq_num_r;
        segment->flags = 2;
        segment->window_size = service_points[syn.segment->dest]->window_size;
        segment->data = ALLOC(byte, 1);
        memcpy(segment->data, &sync, 1);
        
        service_points[syn.segment->dest]->flag_con = 2; //*********************
        pthread_mutex_unlock(&service_points[syn.segment->dest]->mutex_con);
        
        send_seg = ALLOC(send_segment_t, sizeof(send_segment_t));
		send_seg->segment = segment;
    	send_seg->size = 1;
        
        //cria a thread de envio (ela fara reenvio se nao chegar ack)
        pthread_create(&thread, NULL, seg_send_thread, send_seg);
        pthread_detach(thread);
        
        //sp_num = ALLOC(uint16_t, sizeof(uint16_t));
        //memcpy(sp_num, &syn.segment->dest, sizeof(uint16_t));
        //criar thread de envio
        //pthread_create(&s_thread, NULL, send_thread, sp_num);
        //pthread_detach(s_thread);
    }
    else
    {
        pthread_mutex_unlock(&service_points[syn.segment->dest]->mutex_con);
    }
    //senao: *** duvida: enviar uma recusa de conexao ou fazer nada ***
    //termina thread
}

/* thread para estabelecer sincronismo (quando receber SYNC + ACK) */
PRIVATE void *sync_ack_thread(void *param) 
{
	recv_segment_t syn = *(recv_segment_t *)param;
    ack_lst_t *new_ack;
    send_segment_t *pack;
    segment_t *segment;
    byte *buffer;
    pthread_t thread;
    //tem destino setado no ponto de servico?
    pthread_mutex_lock(&service_points[syn.segment->dest]->mutex_con);
    if(service_points[syn.segment->dest]->flag_con == 1)
    {
        //set no ack recebido
        set_ack(service_points[syn.segment->dest]->acks, syn.segment->ack_num);
        //set seq_num_r
		service_points[syn.segment->dest]->seq_num_r = (syn.segment->seq_num + 1) % MAX_SEQ_NUM;
        service_points[syn.segment->dest]->prev_seq_num_r = (syn.segment->seq_num + (MAX_SEQ_NUM - (uint32_t)syn.segment->window_size)) % MAX_SEQ_NUM;
		service_points[syn.segment->dest]->end_seq_num_r = (syn.segment->seq_num + (uint32_t)syn.segment->window_size) % MAX_SEQ_NUM;
        
        //envia ACK
        segment = ALLOC(segment_t, sizeof(segment_t));
        segment->src = syn.segment->dest;
        segment->dest = syn.segment->src;
        segment->ack_num = service_points[syn.segment->dest]->seq_num_r;
        segment->flags = 3;
        segment->window_size = service_points[syn.segment->dest]->window_size;
        
        pack = ALLOC(send_segment_t, sizeof(send_segment_t));
        pack->segment = segment;
        pack->size = 0;
        buffer = ALLOC(byte, TRANSPORT_HEADER_SIZE);
        buildsendsegment(buffer, pack);    
        //chama funcao enviar da camada de rede:
        net_send(service_points[syn.segment->dest]->dest, buffer, TRANSPORT_HEADER_SIZE, 1);
        
        service_points[syn.segment->dest]->flag_con = 2;
        pthread_mutex_unlock(&service_points[syn.segment->dest]->mutex_con);
    }
    else
    {
        pthread_mutex_unlock(&service_points[syn.segment->dest]->mutex_con);
    }
}

/* thread de recebimento para cada ponto de servico */
void *recv_thread(void *param) 
{
    uint16_t sp = *(uint16_t *)param;
    uint32_t offset;
    recv_segment_t new_recv, *recv;
    pthread_t thread;
    byte *buff;
    send_segment_t *pack;
    while(TRUE)
    {
        pthread_mutex_lock(&service_points[sp]->mutex_pack_cons);
        new_recv = service_points[sp]->wait_buff;
        pthread_mutex_unlock(&service_points[sp]->mutex_pack_prod);
        switch(new_recv.segment->flags)
        {
            case 0: //DADOS
                pthread_mutex_lock(&service_points[sp]->mutex_con);
                if(service_points[sp]->flag_con != 2)
                {
                    free(new_recv.segment);
                    break;
                }
                pthread_mutex_unlock(&service_points[sp]->mutex_con);
                //LOCK num_seq_recebimento
                pthread_mutex_lock(&service_points[sp]->mutex_r);
                //se num_seq_ant_recebimento < num_seq_fim_recebimento
                
                if(service_points[sp]->prev_seq_num_r < service_points[sp]->end_seq_num_r)
                {
                    //entao: - se pacote.num_seq >= num_seq_ant_recebimento && pacote.num_seq =< num_seq_fim_recebimento
                    if(new_recv.segment->seq_num >= service_points[sp]->prev_seq_num_r && new_recv.segment->seq_num <= service_points[sp]->end_seq_num_r)
                    {
                        //entao: - se pacote.num_seq < num_seq_recebimento
                        if(new_recv.segment->seq_num < service_points[sp]->seq_num_r)
                        {
                            //entao: - UNLOCK num_seq_recebimento
                            pthread_mutex_unlock(&service_points[sp]->mutex_r);
                            //- reenvia ACK
                            pack = ALLOC(send_segment_t, sizeof(send_segment_t));
                            pack->segment = ALLOC(segment_t, sizeof(segment_t));
                            pack->segment->src = sp;
                            pack->segment->dest = service_points[sp]->dest_sp;
                            pack->segment->ack_num = (new_recv.segment->seq_num + (new_recv.size - TRANSPORT_HEADER_SIZE)) % MAX_SEQ_NUM;
                            
                            pack->segment->flags = 3;
                            pack->segment->window_size = service_points[sp]->window_size;
                            pack->size = 0;
                            buff = ALLOC(byte, TRANSPORT_HEADER_SIZE);
                            buildsendsegment(buff, pack);
                            net_send(service_points[sp]->dest, buff, pack->size + TRANSPORT_HEADER_SIZE, 1);
                            free(pack->segment);
                            free(pack);
                        }
                        else
                        {
                            //senao:
                            //- LOCK mutex_lista_dados
                            pthread_mutex_lock(&service_points[sp]->mutex_seg);
                            //- add dados
                            offset = insert_seg(&service_points[sp]->segs, &service_points[sp]->segs_aux, new_recv.segment, (new_recv.size - TRANSPORT_HEADER_SIZE), service_points[sp]->seq_num_r);
                            //- UNLOCK mutex_lista_dados
                            pthread_mutex_unlock(&service_points[sp]->mutex_seg);
                            //- atualiza número de sequência
                            service_points[sp]->seq_num_r = (service_points[sp]->seq_num_r + offset) % MAX_SEQ_NUM;
                            service_points[sp]->end_seq_num_r = (service_points[sp]->seq_num_r + service_points[sp]->window_size) % MAX_SEQ_NUM;
                            service_points[sp]->prev_seq_num_r = (service_points[sp]->seq_num_r + (MAX_SEQ_NUM - service_points[sp]->window_size)) % MAX_SEQ_NUM;
                            //- UNLOCK num_seq_recebimento
                            pthread_mutex_unlock(&service_points[sp]->mutex_r);
                            //- envia ACK
                            pack = ALLOC(send_segment_t, sizeof(send_segment_t));
                            pack->segment = ALLOC(segment_t, sizeof(segment_t));
                            pack->segment->src = sp;
                            pack->segment->dest = service_points[sp]->dest_sp;
                            pack->segment->ack_num = (new_recv.segment->seq_num + (new_recv.size - TRANSPORT_HEADER_SIZE)) % MAX_SEQ_NUM;
                            pack->segment->flags = 3;
                            pack->segment->window_size = service_points[sp]->window_size;
                            pack->size = 0;
                            buff = ALLOC(byte, TRANSPORT_HEADER_SIZE);
                            buildsendsegment(buff, pack);
                            net_send(service_points[sp]->dest, buff, pack->size + TRANSPORT_HEADER_SIZE, 1);
                            free(pack->segment);
                            free(pack);
                        }
                    }
                    else
                    {
                        //senao: - UNLOCK num_seq_recebimento
                        pthread_mutex_unlock(&service_points[sp]->mutex_r);
                        //- descarta
                        free(new_recv.segment);
                    }
                }
                else
                {
                    //senao: - se pacote.num_seq < num_seq_ant_recebimento && pacote.num_seq > num_seq_fim_recebimento
                    if(new_recv.segment->seq_num < service_points[sp]->prev_seq_num_r && new_recv.segment->seq_num > service_points[sp]->end_seq_num_r)
                    {
                        //entao: - UNLOCK num_seq_recebimento
                        pthread_mutex_unlock(&service_points[sp]->mutex_r);
                        //- descarta
                        free(new_recv.segment);
                    }
                    else
                    {
                        //senao: - se num_seq_ant_recebimento < num_seq_recebimento
                        if(service_points[sp]->prev_seq_num_r < service_points[sp]->seq_num_r)
                        {
                            //entao: - se pacote.num_seq < num_seq_recebimento
                            if(new_recv.segment->seq_num < service_points[sp]->seq_num_r)
                            {
                                //entao: - UNLOCK num_seq_recebimento
                                pthread_mutex_unlock(&service_points[sp]->mutex_r);
                                //- reenvia ACK
                                pack = ALLOC(send_segment_t, sizeof(send_segment_t));
                                pack->segment = ALLOC(segment_t, sizeof(segment_t));
                                pack->segment->src = sp;
                                pack->segment->dest = service_points[sp]->dest_sp;
                                pack->segment->ack_num = (new_recv.segment->seq_num + (new_recv.size - TRANSPORT_HEADER_SIZE)) % MAX_SEQ_NUM;
                                pack->segment->flags = 3;
                                pack->segment->window_size = service_points[sp]->window_size;
                                pack->size = 0;
                                buff = ALLOC(byte, TRANSPORT_HEADER_SIZE);
                                buildsendsegment(buff, pack);
                                net_send(service_points[sp]->dest, buff, pack->size + TRANSPORT_HEADER_SIZE, 1);
                                free(pack->segment);
                                free(pack);
                            }
                            else
                            {
                                //senao: 
                                //- LOCK mutex_lista_dados
                                pthread_mutex_lock(&service_points[sp]->mutex_seg);
                                //- add dados
                                offset = insert_seg(&service_points[sp]->segs, &service_points[sp]->segs_aux, new_recv.segment, (new_recv.size - TRANSPORT_HEADER_SIZE), service_points[sp]->seq_num_r);
                                //- UNLOCK mutex_lista_dados
                                pthread_mutex_unlock(&service_points[sp]->mutex_seg);
                                //- atualiza número de sequência
                                service_points[sp]->seq_num_r = (service_points[sp]->seq_num_r + offset) % MAX_SEQ_NUM;
                                service_points[sp]->end_seq_num_r = (service_points[sp]->seq_num_r + service_points[sp]->window_size) % MAX_SEQ_NUM;
                                service_points[sp]->prev_seq_num_r = (service_points[sp]->seq_num_r + (MAX_SEQ_NUM - service_points[sp]->window_size)) % MAX_SEQ_NUM;
                                //- UNLOCK num_seq_recebimento
                                pthread_mutex_unlock(&service_points[sp]->mutex_r);
                                //- envia ACK
                                pack = ALLOC(send_segment_t, sizeof(send_segment_t));
                                pack->segment = ALLOC(segment_t, sizeof(segment_t));
                                pack->segment->src = sp;
                                pack->segment->dest = service_points[sp]->dest_sp;
                                pack->segment->ack_num = (new_recv.segment->seq_num + (new_recv.size - TRANSPORT_HEADER_SIZE)) % MAX_SEQ_NUM;
                                pack->segment->flags = 3;
                                pack->segment->window_size = service_points[sp]->window_size;
                                pack->size = 0;
                                buff = ALLOC(byte, TRANSPORT_HEADER_SIZE);
                                buildsendsegment(buff, pack);
                                net_send(service_points[sp]->dest, buff, pack->size + TRANSPORT_HEADER_SIZE, 1);
                                free(pack->segment);
                                free(pack);
                            }
                        }
                        else
                        {
                            //senao: - se pacote.num_seq >= num_seq_recebimento
                            if(new_recv.segment->seq_num >= service_points[sp]->seq_num_r)
                            {
                                //entao: 
                                //- LOCK mutex_lista_dados
                                pthread_mutex_lock(&service_points[sp]->mutex_seg);
                                //- add dados
                                offset = insert_seg(&service_points[sp]->segs, &service_points[sp]->segs_aux, new_recv.segment, (new_recv.size - TRANSPORT_HEADER_SIZE), service_points[sp]->seq_num_r);
                                //- UNLOCK mutex_lista_dados
                                pthread_mutex_unlock(&service_points[sp]->mutex_seg);
                                //- atualiza número de sequência
                                service_points[sp]->seq_num_r = (service_points[sp]->seq_num_r + offset) % MAX_SEQ_NUM;
                                service_points[sp]->end_seq_num_r = (service_points[sp]->seq_num_r + service_points[sp]->window_size) % MAX_SEQ_NUM;
                                service_points[sp]->prev_seq_num_r = (service_points[sp]->seq_num_r + (MAX_SEQ_NUM - service_points[sp]->window_size)) % MAX_SEQ_NUM;
                                //- UNLOCK num_seq_recebimento
                                pthread_mutex_unlock(&service_points[sp]->mutex_r);
                                //- envia ACK
                                pack = ALLOC(send_segment_t, sizeof(send_segment_t));
                                pack->segment = ALLOC(segment_t, sizeof(segment_t));
                                pack->segment->src = sp;
                                pack->segment->dest = service_points[sp]->dest_sp;
                                pack->segment->ack_num = (new_recv.segment->seq_num + (new_recv.size - TRANSPORT_HEADER_SIZE)) % MAX_SEQ_NUM;
                                pack->segment->flags = 3;
                                pack->segment->window_size = service_points[sp]->window_size;
                                pack->size = 0;
                                buff = ALLOC(byte, TRANSPORT_HEADER_SIZE);
                                buildsendsegment(buff, pack);
                                net_send(service_points[sp]->dest, buff, pack->size + TRANSPORT_HEADER_SIZE, 1);
                                free(pack->segment);
                                free(pack);
                            }
                            else
                            {
                                //senao: - UNLOCK num_seq_recebimento
                                pthread_mutex_unlock(&service_points[sp]->mutex_r);
                                //- reenvia ACK
                                pack = ALLOC(send_segment_t, sizeof(send_segment_t));
                                pack->segment = ALLOC(segment_t, sizeof(segment_t));
                                pack->segment->src = sp;
                                pack->segment->dest = service_points[sp]->dest_sp;
                                pack->segment->ack_num = (new_recv.segment->seq_num + (new_recv.size - TRANSPORT_HEADER_SIZE)) % MAX_SEQ_NUM;
                                pack->segment->flags = 3;
                                pack->segment->window_size = service_points[sp]->window_size;
                                pack->size = 0;
                                buff = ALLOC(byte, TRANSPORT_HEADER_SIZE);
                                buildsendsegment(buff, pack);
                                net_send(service_points[sp]->dest, buff, pack->size + TRANSPORT_HEADER_SIZE, 1);
                                free(pack->segment);
                                free(pack);
                            }
                        }
                    }
                }
                break;
            case 1: //SYNC
                //cria a thread para tratar sincronismo
                recv = ALLOC(recv_segment_t, sizeof(recv_segment_t));
                recv->segment = new_recv.segment;
                recv->src = new_recv.src;
                pthread_create(&thread, NULL, sync_thread, recv);
                pthread_detach(thread);
                break;
            case 2: //SYNC + ACK
                //cria a thread para tratar sincronismo
                recv = ALLOC(recv_segment_t, sizeof(recv_segment_t));
                recv->segment = new_recv.segment;
                recv->src = new_recv.src;
                pthread_create(&thread, NULL, sync_ack_thread, recv);
                pthread_detach(thread);
                break;
            case 3: //ACK
                pthread_mutex_lock(&service_points[sp]->mutex_ack);
                //list_ack(service_points[sp]->acks);
                set_ack(service_points[sp]->acks, new_recv.segment->ack_num);
                //list_ack(service_points[sp]->acks);
                pthread_mutex_unlock(&service_points[sp]->mutex_ack);
                //pthread_mutex_unlock(&service_points[sp]->mutex_recv_ack);
                free(new_recv.segment);
                break;
            case 4: //FIN
                pthread_mutex_lock(&service_points[sp]->mutex_con);
                close_conn_aux(sp, service_points[sp]->flag_con, new_recv);
                pthread_mutex_unlock(&service_points[sp]->mutex_con);
                break;
        }
    }
}
/* função responsável tratar as mensagens = fin */
PRIVATE void close_conn_aux(uint16_t point, uint8_t flag, recv_segment_t recv)
{
    segment_t *segment;
    ack_lst_t *new_ack;
    send_segment_t *send_seg;
    send_segment_t *pack;
    byte *buf, *buff;
    char fin = 'f';
    pthread_t thread;
    if(service_points[point]->flag_con == 2)
    {
        //envia ack
        pack = ALLOC(send_segment_t, sizeof(send_segment_t));
        pack->segment = ALLOC(segment_t, sizeof(segment_t));
        pack->segment->src = point;
        pack->segment->dest = service_points[point]->dest_sp;
        pack->segment->ack_num = (recv.segment->seq_num + (recv.size - TRANSPORT_HEADER_SIZE)) % MAX_SEQ_NUM;
        pack->segment->flags = 3;
        pack->segment->window_size = service_points[point]->window_size;
        pack->size = 0;
        buff = ALLOC(byte, TRANSPORT_HEADER_SIZE);
        buildsendsegment(buff, pack);
        net_send(service_points[point]->dest, buff, pack->size + TRANSPORT_HEADER_SIZE, 1);
        free(pack->segment);
        free(pack);
        
        //cria segmento de fin
        segment = ALLOC(segment_t, sizeof(segment_t));
        segment->src = point;
        segment->dest = service_points[point]->dest_sp;
        pthread_mutex_lock(&service_points[point]->mutex_s);
        segment->seq_num = service_points[point]->seq_num_s;
        segment->flags = 4;
        segment->window_size = service_points[point]->window_size;
        segment->data = ALLOC(byte, 1);
        memcpy(segment->data, &fin, 1);
        
        //cria ack esperado na lista de acks
        new_ack = ALLOC(ack_lst_t, sizeof(ack_lst_t));
        service_points[point]->seq_num_s = (service_points[point]->seq_num_s + 1) % MAX_SEQ_NUM;
        new_ack->seq_num = service_points[point]->seq_num_s;
        new_ack->flag = 0;
        new_ack->size = 1;
        new_ack->next = NULL;
        pthread_mutex_lock(&service_points[point]->mutex_ack);
        insert_ack(&service_points[point]->acks, new_ack);
        pthread_mutex_unlock(&service_points[point]->mutex_ack);
        
        pthread_mutex_unlock(&service_points[point]->mutex_s);
        
        send_seg = ALLOC(send_segment_t, sizeof(send_segment_t));
        send_seg->segment = segment;
        send_seg->size = 1;
        //cria a thread de envio (ela fara reenvio se nao chegar ack)
        pthread_create(&thread, NULL, seg_send_thread, send_seg);
        pthread_detach(thread);
        
        service_points[point]->flag_con = 0;
    }
    if(service_points[point]->flag_con == 3)
    {
        pack = ALLOC(send_segment_t, sizeof(send_segment_t));
        pack->segment = ALLOC(segment_t, sizeof(segment_t));
        pack->segment->src = point;
        pack->segment->dest = service_points[point]->dest_sp;
        pack->segment->ack_num = (recv.segment->seq_num + (recv.size - TRANSPORT_HEADER_SIZE)) % MAX_SEQ_NUM;
        pack->segment->flags = 3;
        pack->segment->window_size = service_points[point]->window_size;
        pack->size = 0;
        buff = ALLOC(byte, TRANSPORT_HEADER_SIZE);
        buildsendsegment(buff, pack);
        net_send(service_points[point]->dest, buff, pack->size + TRANSPORT_HEADER_SIZE, 1);
        free(pack->segment);
        free(pack);
        service_points[point]->flag_con = 0;
    }
}

/* função para a depuração */
PUBLIC void depurar()
{
    int i;
    printf("Pontos de serviços alocados:\n");
    for (i = 0; i < MAX_SERVICE_POINTS; i++)
	{
		if(service_points[i] != NULL)
        {
            
            switch(service_points[i]->flag_con)
            {
                case 0:
                    printf("%d - Sem conexao\n", i);
                    break;
                case 1:
                    printf("%d - Estabelecendo conexao\n", i);
                    break;
                case 2:
                    printf("%d - Conexao com: no %u - ponto de servico %u ", i, service_points[i]->dest, service_points[i]->dest_sp);
                    printf("- Tamanho da janela %u bytes\n",service_points[i]->window_size);
                    break;
                case 3:
                    printf("%d - Encerrando conexao\n", i);
                    break;
            }
        }
	}
}