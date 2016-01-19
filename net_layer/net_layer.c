/* Arquivo: net_layer.c
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#include <link.h>
#include <net_layer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common.h>
#include <pthread.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <checksum.h>
#include <frag.h>
#include <net_list.h>
#include <sys/sem.h>            /* for semget(), semop(), semctl() */
#include <unistd.h>

 /*2 chaves para o semaforo*/
#define SEM_KEY_INDICE_PROD     0x3000      /*chave para semaforo do produtor*/
#define SEM_KEY_INDICE_CONS     0x3100      /*chave para semaforo do consumidor*/
int g_sem_prod;
int g_sem_cons;


#ifdef DEBUG
#define LOG(s, ...) logdebug(stdout, NETWORK, s, ##__VA_ARGS__)
#else
#define LOG(s, ...)
#endif

PRIVATE void *internalsend(void *param);
PRIVATE void *internalrecv(void *param);

PRIVATE void *internalsendroute(void *param);
PRIVATE void *internalhandleroute(void *param);
PRIVATE void fillallroutes();
PRIVATE void neighbors_init(int way[MAX_HOSTS]);
PRIVATE uint16_t hostreachable(uint16_t dest);

uint16_t num_global;

//vetor de vizinhos utilizado pela tabela de rotas
PRIVATE uint8_t neighbors[MAX_HOSTS];

PRIVATE pthread_mutex_t send_mutex;
PRIVATE pthread_mutex_t intersend_mutex;

PRIVATE pthread_mutex_t route_table_mutex;

PRIVATE routetable_t routes[MAX_HOSTS];

PRIVATE pthread_mutex_t prod_insert;
int prod_vector = MAX_SEND_WAITING - 1;
int cons_vector = 0;
PUBLIC wait_list_item_t wait_list[MAX_SEND_WAITING],wait_item;

//mutex para sincronismo entre as camadas de rede e enlace
PUBLIC pthread_mutex_t net_recv_mutex;
PUBLIC pthread_mutex_t link_recv_mutex;
//mutex para sincronismo entre as camadas de rede e transporte
PUBLIC pthread_mutex_t net2_recv_mutex;
PUBLIC pthread_mutex_t transp_recv_mutex;

PUBLIC pthread_mutex_t net_send_mutex;
PUBLIC pthread_mutex_t counter_list_mutex;
PUBLIC pthread_mutex_t list_prod_mutex;
PUBLIC pthread_mutex_t list_cons_mutex;
PUBLIC pthread_mutex_t max_list_mutex;
PUBLIC pthread_mutex_t empty_list_mutex;

/*Serao usadas em PrintAlphabet(). Os filhos receberam elas em herança , *nao necessitando de mudanças.*/
struct sembuf   g_sem_bloquea[1];           //bloque a regiao critica
struct sembuf   g_sem_libera[1];            //libera a regiao critica

//frame recebido da camada de enlace
frame_t recv_frame;
//dados para passar para camada de transporte
netdata_t recv_datagram;

/*função responsável inicializar a camada de rede*/
PUBLIC int net_init(uint16_t num, pthread_mutex_t link, pthread_mutex_t net, pthread_mutex_t net2, pthread_mutex_t transp, frame_t *link_recv_frame, netdata_t **net_recv_datagram, int way[MAX_HOSTS])
{
    //criação das threads de envio, recebimento e envio de rotas
	pthread_t send_thread;
    pthread_t recv_thread;
    pthread_t send_route_thread;
    //recebimento dos mutex de sincronismo entre as camadas
    link_recv_mutex = link;
    net_recv_mutex = net;
    net2_recv_mutex = net2;
    transp_recv_mutex = transp;
    //numero do nó
    num_global = num;
    //frame recebido da camada de enlace
    recv_frame = *link_recv_frame;
    //dados para passar para camada de transporte
    (*net_recv_datagram) = &recv_datagram;

    /*
     * Construindo a estrutura de controle do semaforo
     */
    g_sem_libera[0].sem_num   =  0;
    g_sem_libera[0].sem_op    = MAX_SEND_WAITING;
    g_sem_libera[0].sem_flg   =  0;
    
    g_sem_bloquea[0].sem_num =  0;
    g_sem_bloquea[0].sem_op  =  -1;
    g_sem_bloquea[0].sem_flg =  0;

    /*criando o semaforo produtor*/
    if ((g_sem_prod = semget(SEM_KEY_INDICE_PROD + num_global,1,IPC_CREAT | 0666)) == -1){
        fprintf(stderr,"chamada semget() falhou! Impossivel criar conjunto de semaforos");
        exit(1);
    }
    /*inicializando o semaforo produtor*/
    if (semop (g_sem_prod, g_sem_libera,1) == -1){//
        fprintf(stderr,"chamada semop() falhou , impossivel inicializar o semaforo");
        exit(1);
    }
    g_sem_libera[0].sem_op = 1;
    
    /*criando o semaforo consumidor*/
    if ((g_sem_cons = semget(SEM_KEY_INDICE_CONS + num_global,1,IPC_CREAT | 0666)) == -1){
        fprintf(stderr,"chamada semget() falhou! Impossivel criar conjunto de semaforos");
        exit(1);
    }
    
    //inicialização dos mutex
    pthread_mutex_init(&route_table_mutex, NULL);

    //preenche o vetor de vizinhos oriundos do caminho
    neighbors_init(way);

    //inicialização da tabela de rotas 
    fillallroutes();
    
    //criação da thread de envio
    pthread_create(&send_thread, NULL, internalsend, NULL);
    pthread_detach(send_thread);
    LOG("thread de envio criada");
    
    //criação da thread de recebimento
    pthread_create(&recv_thread, NULL, internalrecv, NULL);
    pthread_detach(recv_thread);
    LOG("thread de recebimento criada");

    //criação da thread de envio de rotas
    pthread_create(&send_route_thread, NULL, internalsendroute, NULL);
    pthread_detach(send_route_thread);
    LOG("thread de rotas criada");

    return OK;
}

/*criação do pacote de envio*/
PUBLIC void buildsenddatagram(byte *buf, datagram_t *datagram)
{
    size_t count = 0;
	memcpy(buf, &datagram->size, sizeof(datagram->size));
    count += sizeof(datagram->size);
    memcpy(buf + count, &datagram->id, sizeof(datagram->id));
    count += sizeof(datagram->id);
    memcpy(buf + count, &datagram->frag, sizeof(datagram->frag));
    count += sizeof(datagram->frag);
    memcpy(buf + count, &datagram->protocol, sizeof(datagram->protocol));
    count += sizeof(datagram->protocol);
    memcpy(buf + count, &datagram->ttl, sizeof(datagram->ttl));
    count += sizeof(datagram->ttl);
    memcpy(buf + count, &datagram->dest, sizeof(datagram->dest));
    count += sizeof(datagram->dest);
    memcpy(buf + count, &datagram->src, sizeof(datagram->src));
    count += sizeof(datagram->src);
	memcpy(buf + count, datagram->data, datagram->size);
    count += datagram->size;
	datagram->checksum = checksum(buf, count);
	memcpy(buf + count, &datagram->checksum, CHECKSUM_SIZE);
}

/*criação do pacote de recebimento*/
PUBLIC datagram_t *buildrecvdatagram(byte *buf, size_t size)
{
    datagram_t *recv_new = ALLOC(datagram_t, sizeof(datagram_t));
    size_t count = 0;
	memcpy(&recv_new->size, buf, sizeof(recv_new->size));
    count += sizeof(recv_new->size);
    memcpy(&recv_new->id, buf + count, sizeof(recv_new->id));
    count += sizeof(recv_new->id);
    memcpy(&recv_new->frag, buf + count, sizeof(recv_new->frag));
    count += sizeof(recv_new->frag);
    memcpy(&recv_new->protocol, buf + count, sizeof(recv_new->protocol));
    count += sizeof(recv_new->protocol);
    memcpy(&recv_new->ttl, buf + count, sizeof(recv_new->ttl));
    count += sizeof(recv_new->ttl);
    memcpy(&recv_new->dest, buf + count, sizeof(recv_new->dest));
    count += sizeof(recv_new->dest);
    memcpy(&recv_new->src, buf + count, sizeof(recv_new->src));
    count += sizeof(recv_new->src);
    recv_new->data = ALLOC(byte, recv_new->size);
	memcpy(recv_new->data, buf + count, recv_new->size);
    count += recv_new->size;
	memcpy(&recv_new->checksum, buf + count, CHECKSUM_SIZE);
    return recv_new;
}

/*função responsável produzir um pacote na fila de envio*/
PRIVATE void wait_list_insert(wait_list_item_t head)
{
    //verifica se tem espaço para produzir
    if(semop(g_sem_prod,g_sem_bloquea,1) == -1){
        fprintf(stderr,"Impossivel fechar o semaforo! \n");
        exit(1);
    }
    pthread_mutex_lock(&prod_insert);
    prod_vector = (prod_vector + 1) % MAX_SEND_WAITING;
    wait_list[prod_vector].dest = head.dest;
    wait_list[prod_vector].src = head.src;
    wait_list[prod_vector].data = head.data;
    wait_list[prod_vector].size = head.size;
    wait_list[prod_vector].protocol = head.protocol;
    wait_list[prod_vector].first_dest_link = head.first_dest_link;
    wait_list[prod_vector].offset = head.offset;
    wait_list[prod_vector].ttl = head.ttl;
    wait_list[prod_vector].id = head.id;
    pthread_mutex_unlock(&prod_insert);
    
    //semaforo desbloqueia um espaço para consumir
    if(semop(g_sem_cons,g_sem_libera,1) == -1){
        fprintf(stderr,"chamada semop(),impossivel liberar recurso");
        exit(1);
    }
}

/*função responsável consumir um pacote disponível na fila de envio*/
PRIVATE wait_list_item_t wait_list_remove()
{
    wait_list_item_t tmp;
    
    //verifica se pode consumir
    if(semop(g_sem_cons,g_sem_bloquea,1) == -1){
        fprintf(stderr,"Impossivel fechar o semaforo! \n");
        exit(1);
    }
    
    tmp = wait_list[cons_vector];
    cons_vector = (cons_vector + 1) % MAX_SEND_WAITING;
    
    //semaforo desbloqueia um espaço para produzir
    if(semop(g_sem_prod,g_sem_libera,1) == -1){
        fprintf(stderr,"Impossivel liberar o semaforo! \n");
        exit(1);
    }
    
    return tmp;
}
/*função responsável por colocar o pacote na fila de envio, utilizado pelo transporte e thread de envio de rotas*/
PUBLIC int net_send(uint16_t dest, byte *data, size_t size, uint8_t protocol)
{
    int node;
    if(protocol == 0)
    {
        node = dest;
    }
    else
    {
        node = hostreachable(dest);
    }
    if(node != 0)
    {
        wait_item.dest = dest;
        wait_item.src = num_global;
        wait_item.data = data;
        wait_item.size = size;
        wait_item.protocol = protocol;
        wait_item.first_dest_link = node;
        wait_item.offset = 0;
        wait_item.ttl = TTL;
        wait_list_insert(wait_item);
        return OK;
    }
    return ERROR;
}
/*função responsável por colocar o pacote na fila de envio, utilizado recebimento de rotas para repasse*/
PUBLIC void net_resend(datagram_t *datagram)
{
    int node;
    wait_list_item_t resend;
    if((node = hostreachable(datagram->dest))!=0)
    {     
        resend.size = datagram->size;
        resend.id = datagram->id;
        resend.offset = datagram->frag;
        resend.protocol = datagram->protocol;
        resend.ttl = datagram->ttl;
        resend.dest = datagram->dest;
        resend.src = datagram->src;
        resend.data = datagram->data;
        resend.first_dest_link = node;
        wait_list_insert(resend);
    }
}

/*thread que recebe da camada de baixo*/
PRIVATE void *internalsend(void *param)
{
    int isfragmentable;
    int mtu;
    uint16_t count_id = 0;
    wait_list_item_t intern_list_cons;
    offsetlst_t *head = NULL,*aux;
    datagram_t *data_send = ALLOC(datagram_t, sizeof(datagram_t));
    byte *buffer;
    while(TRUE)
    {
        intern_list_cons = wait_list_remove();
        //getmtu do destino
        mtu = getmtu(num_global, intern_list_cons.first_dest_link);
        //protocolo = 1 utilizado pelo transp ou protocolo = 0 utilizado pela tab de rotas
        if(intern_list_cons.protocol == 1 || intern_list_cons.protocol ==  0)
        {
            isfragmentable = fragpack(&head,intern_list_cons.data,intern_list_cons.size,mtu,intern_list_cons.offset);
            LOG("dest1 %d", intern_list_cons.dest);
            //sabe que é apenas dados
            if(isfragmentable == 0)
            {
                //nao precisou fragmentar
                data_send->size = intern_list_cons.size;
                data_send->id = count_id;
                data_send->frag = intern_list_cons.offset;
                data_send->protocol = intern_list_cons.protocol;
                data_send->ttl = intern_list_cons.ttl;
                data_send->dest = intern_list_cons.dest;
                data_send->src = intern_list_cons.src;
                data_send->data = ALLOC(byte, data_send->size);
                memcpy(data_send->data, intern_list_cons.data, data_send->size);
                buffer = ALLOC(byte, data_send->size + NET_HEADER_SIZE);
                buildsenddatagram(buffer,data_send);
                free(data_send->data);

                if(count_id <= 65535)
                    count_id++;
                else
                    count_id = 0;

                LOG("enviando para enlace");
                link_send(intern_list_cons.first_dest_link, buffer, data_send->size + NET_HEADER_SIZE);
                //free(data_send); ???

            }
            else
            {LOG("frag para enlace");
                while(head != NULL)
                {LOG("head para enlace");
                    //precisa fragmentar
                    data_send->size = head->datagram->size;
                    data_send->id = count_id;
                    data_send->frag = head->datagram->frag;
                    data_send->protocol = intern_list_cons.protocol;
                    data_send->ttl = intern_list_cons.ttl;
                    data_send->dest = intern_list_cons.dest;
                    LOG("dest %d", data_send->dest);
                    data_send->src = intern_list_cons.src;
                    data_send->data = ALLOC(byte, head->datagram->size);
                    memcpy(data_send->data, head->datagram->data, head->datagram->size);
                    LOG("enviando data %s", data_send->data);
                    buffer = ALLOC(byte, data_send->size + NET_HEADER_SIZE);
                    buildsenddatagram(buffer,data_send);
                    free(data_send->data);
                    link_send(intern_list_cons.first_dest_link, buffer, data_send->size + NET_HEADER_SIZE);
                    //anda com a lista
                    aux = head;

                    head = head->next;

                    free(aux);
                }
                LOG("fim para enlace");
                if(count_id <= 65535)
                    count_id++;
                else
                    count_id = 0;
            }
        }
        //protocolo = 2 , utilizado pelo repasse
        else
        {
            if(intern_list_cons.protocol == 2)
            {
                isfragmentable = fragpack2(&head,intern_list_cons.data,intern_list_cons.size,mtu,intern_list_cons.offset);
                
                if(isfragmentable == 0)
                {
                    //nao precisou fragmentar
                    data_send->size = intern_list_cons.size;
                    data_send->id = intern_list_cons.id;
                    data_send->frag = intern_list_cons.offset;
                    data_send->protocol = intern_list_cons.protocol;
                    data_send->ttl = intern_list_cons.ttl;
                    data_send->dest = intern_list_cons.dest;
                    data_send->src = intern_list_cons.src;
                    data_send->data = ALLOC(byte, data_send->size);
                    memcpy(data_send->data, intern_list_cons.data, data_send->size);
                    buffer = ALLOC(byte, data_send->size + NET_HEADER_SIZE);
                    buildsenddatagram(buffer,data_send);
                    free(data_send->data);
                    LOG("enviando para enlace");
                    link_send(intern_list_cons.first_dest_link, buffer, data_send->size + NET_HEADER_SIZE);
                    //free(data_send); ???
                }
                else
                {
                    while(head != NULL)
                    {
                        //precisa fragmentar
                        data_send->size = head->datagram->size;
                        data_send->id = intern_list_cons.id;
                        data_send->frag = head->datagram->frag;
                        data_send->protocol = intern_list_cons.protocol;
                        data_send->ttl = intern_list_cons.ttl;
                        data_send->dest = intern_list_cons.dest;
                        data_send->src = intern_list_cons.src;
                        data_send->data = ALLOC(byte, head->datagram->size);
                        memcpy(data_send->data, head->datagram->data, head->datagram->size);
                        
                        buffer = ALLOC(byte, data_send->size + NET_HEADER_SIZE);
                        buildsenddatagram(buffer,data_send);
                        free(data_send->data);
                        link_send(intern_list_cons.first_dest_link, buffer, data_send->size + NET_HEADER_SIZE);
                        //anda com a lista
                        aux = head;

                        head = head->next;

                        free(aux);
                    }
                    
                }
            }
        } 
    }
}

/*thread que recebe da camada de baixo*/
PRIVATE void *internalrecv(void *param)
{
    datagram_t *recv;
    pthread_t recv_route_thread;
    packlst_t *list = NULL;
    uint16_t rcv_checksum;
    byte *buf;
    size_t size;
    size_t data_size;
    while(TRUE)
    {
        rcv_checksum = 1;
        //espera por liberação da camada de enlace para o próximo recebimento        
        LOG("mutex travado para receber");
        pthread_mutex_lock(&net_recv_mutex);
        
        //copia dados recebidos pelo enlace
        buf = recv_frame.data;
        size = recv_frame.size;
        
        //libera camada de enlace para o proximo recebimento
        LOG("mutex liberado para camada de enlace");
        pthread_mutex_unlock(&link_recv_mutex);
        
        //criação do datagram_t
        recv = buildrecvdatagram(buf, size);
        LOG("recebendo data %s", recv->data);
        //recv = ALLOC(datagram_t, sizeof(datagram_t));
        //recv->data = ALLOC(byte, size - NET_HEADER_SIZE);
        //memcpy(recv, buf, NET_HEADER_SIZE - CHECKSUM_SIZE);
        //memcpy(recv->data, buf + NET_HEADER_SIZE - CHECKSUM_SIZE, size - NET_HEADER_SIZE);
        //memcpy(&recv->checksum, buf + size - CHECKSUM_SIZE, CHECKSUM_SIZE);
        
        //calcula o checksum
        //LOG("check: %u",recv->checksum);
        rcv_checksum = checksum(buf, size - CHECKSUM_SIZE);
        //LOG("checksum calculado: %#04x", rcv_checksum);
        rcv_checksum = rcv_checksum - recv->checksum;
        //LOG("resultado do checksum: %#04x", rcv_checksum);
        
        //desaloca buf
        free(buf);
        buf = NULL;
        
        //se o checksum estiver correto
        if (!rcv_checksum)
        {LOG("dest %d %d ", recv->dest, num_global);
            //se o destino é para mim
            if(recv->dest == num_global)
            {LOG("pack para mim");
                //adiciona na lista e se tiver completo trata o pacote
                if(insertoffset(&list, recv) == TRUE)
                {
                    if(list == NULL)
			         LOG("Esta 222 NULL ---------");
                    LOG("insertoffset ok!");
                    //tratar pacote
                    recv->data = getpack(&list, recv->id, recv->src, &data_size);
                    LOG("get pack");
                    LOG("recv->data %s", recv->data);
                    switch(recv->protocol)
                    {
                        case 0: //mensagem de rotas
                            pthread_create(&recv_route_thread, NULL, internalhandleroute, recv->data);
                            pthread_detach(recv_route_thread);
                            free(recv);
                            break;
                        case 1: //PTC
                        case 2:
                            //repassa o pacote para camada de transporte
                            
                            //espera por liberação da camada de transporte para o próximo recebimento
                            pthread_mutex_lock(&net2_recv_mutex);
                            LOG("mutex travado para repassar a camada de transporte");
                            recv_datagram.data = recv->data;
                            recv_datagram.size = data_size;
                            recv_datagram.src = recv->src;
                            LOG("data: %s", recv_datagram.data);
                            LOG("data: %zu", recv_datagram.size);
                            free(recv);
                            
                            //libera camada de transporte para pegar o pacote
                            LOG("mutex liberado para camada de transporte");
                            pthread_mutex_unlock(&transp_recv_mutex);
                            break;
                    }
                }
            }
            else //senão reenvia pacote
            {LOG("pack nao e para mim");
                //reenviar pacote
                recv->protocol = 2;
                recv->ttl--;
                net_resend(recv);
            }
        }
    }
}

/*thread destinada a receber rota e atualizar sua tabela de rotas*/
PRIVATE void *internalhandleroute(void *param)
{
    //obtem a rota recebida 'por parametro da thread'
    routetable_t routereceived = *(routetable_t *)param;
    //LOG("rota recebida: d:%d g:%d c:%d",routereceived.dest,routereceived.gateway,routereceived.cost);

    pthread_mutex_lock(&route_table_mutex);
    //verifica se o destino da tabela de rotas não existe ainda ou se o novo custo é menor que o custo atual
    if((routes[routereceived.dest-1].cost == INFINITE_ROUTE) || (routereceived.cost < routes[routereceived.dest-1].cost))
    {   
        //atualiza o custo da tabela de rotas
        routes[routereceived.dest-1].cost = routereceived.cost;
        
        //preenche o gateway da tabela de rotas com o host que envio essa mensagem
        routes[routereceived.dest-1].gateway = routereceived.gateway; 

        //LOG("rota atualizada dest: %d, gateway: %d, cost: %d",routes[routereceived.dest-1].dest,routes[routereceived.dest-1].gateway,routes[routereceived.dest-1].cost ); 

    }
    pthread_mutex_unlock(&route_table_mutex);
    //LOG("fim do recebimento");
    free (param);
}

/*thread destinada a enviar a sua tabela de routas para todos os vizinhos a cada 30 segundos */
PRIVATE void *internalsendroute(void *param)
{
    //preenche a tabela com os destino dos vizinhos e dele mesmo
    uint16_t current;

    pthread_mutex_lock(&route_table_mutex);
    //percorre os vizinhos em busca de alguem, caso encontre, adiciona na tabela de rotas
    for (current =0; current<MAX_HOSTS; current++)
    {
        //verifica se é o vizinho ou ele mesmo
        if ((neighbors[current] == 1) || (current == (num_global-1)))
        {
            routes[current].gateway = num_global;
            routes[current].cost = 1;
        }

       // imprime tabela de rotas
       //LOG("rota - %d --dest: %d, gateway: %d, cost: %d",current,routes[current].dest,routes[current].gateway,routes[current].cost ); 
    }
    pthread_mutex_unlock(&route_table_mutex);

    //estrutura usada na comunicação
    routetable_t packcomunication;
    uint16_t numberhost;
    byte *aux;

    //neste momento, divulga as rotas conhecidas e aguarda a cada 30 segundos.
    while (TRUE)
    {
        //divulga rota para seus vizinhos
        for (current =0; current<MAX_HOSTS; current++)
        {
            //encontra o vizinho a ser mandado as rotas
            if ((neighbors[current] == 1))
            {
                 //envia todas as rotas conhecidas para o vizinho
                 for (numberhost =0; numberhost<MAX_HOSTS; numberhost++)
                 {
                    pthread_mutex_lock(&route_table_mutex);
                    if(routes[numberhost].cost != 0)
                    {

                        packcomunication.dest = routes[numberhost].dest;
                        packcomunication.cost = routes[numberhost].cost;
                        packcomunication.cost++; 
                        packcomunication.gateway = num_global; //gatway utilizado para saber quem mandou a msg de comunicacao
                        //LOG("rota enviada -- dest:%d gateway:%d cost:%d",packcomunication.dest,packcomunication.gateway,packcomunication.cost);
                        aux = ALLOC(byte, sizeof(packcomunication));
                        memcpy(aux, &packcomunication, sizeof(packcomunication));
                        pthread_mutex_unlock(&route_table_mutex);
                        //envia rota;
                        net_send(current+1, aux, sizeof(packcomunication), 0);
                    }
                    else
                    {
                        pthread_mutex_unlock(&route_table_mutex);
                    }
                    
                 }
            }
        }
        sleep(30); //temporizador de 30 segundos
    }
}

/*função destinada a inicializar(preencher) a tabela de routas, (coloca -1 nos destinos e 0 nos custos) */
PRIVATE void fillallroutes()
{
    //LOG("inicializa vetor de rotas"); 
    int currentHost;

    pthread_mutex_lock(&route_table_mutex);

    for (currentHost = 0; currentHost < MAX_HOSTS; currentHost++)
    {
        //cria a rota para cada item da tabela
        
        //preeenche os destinos e custos (default 0)
        routes[currentHost].dest = currentHost+1;
        routes[currentHost].cost = INFINITE_ROUTE;

        //preenche a porta de entrada (0 = a não existe)
        routes[currentHost].gateway = INFINITE_ROUTE;

        //LOG("inicialização dest: %d, gateway: %d, cost: %d",routes[currentHost].dest,routes[currentHost].gateway,routes[currentHost].cost ); 
    }
    pthread_mutex_unlock(&route_table_mutex);
}

PUBLIC void dislpay_routes()
{
    uint16_t currentHost;

    pthread_mutex_lock(&route_table_mutex);

    for (currentHost =0; currentHost<MAX_HOSTS; currentHost++)
    {
       // imprime tabela de rotas
       printf("\nRota  para destino: %d, gateway: %d, cost: %d",routes[currentHost].dest,routes[currentHost].gateway,routes[currentHost].cost );
    }
    printf("\n\n");
    
    pthread_mutex_unlock(&route_table_mutex);
    
}

PUBLIC void disable_route(uint16_t host)
{
    uint16_t currentHost;

    pthread_mutex_lock(&route_table_mutex);
    
    //remove vizinho
    neighbors[host-1] = 0;

    //remove rotas da tabela de rotas
    routes[host-1].cost = INFINITE_ROUTE;
    routes[host-1].gateway = INFINITE_ROUTE;
    
    for (currentHost =0; currentHost<MAX_HOSTS; currentHost++)
    {
       if (routes[currentHost].gateway == host) 
        {
             routes[currentHost].cost = INFINITE_ROUTE;
             routes[currentHost].gateway = INFINITE_ROUTE;
        }
        
        //printf("\n\nvizinho%d -- %d\n",currentHost+1, neighbors[currentHost]);
       // imprime tabela de rotas
      // printf("\nRota  para destino: %d, gateway: %d, cost: %d",routes[currentHost].dest,routes[currentHost].gateway,routes[currentHost].cost );
    }
  
    pthread_mutex_unlock(&route_table_mutex);
    
}

PUBLIC void enable_route(uint16_t host)
{
    uint16_t currentHost;

    pthread_mutex_lock(&route_table_mutex);
    
    neighbors[host-1] = 1;
    
    routes[host-1].cost = 1;
    routes[host-1].gateway = host;
  
    pthread_mutex_unlock(&route_table_mutex);
    
}

/*função destinada a inicializar os vizinhos do host (1 = tem vizinho, 0 não tem vizinho)*/
PRIVATE void neighbors_init(int way[MAX_HOSTS])
{
    int i;
    for (i = 0; i < MAX_HOSTS; i++)
    {
        if (way[i] != -1)
            neighbors[i] = 1;
        else
            neighbors[i] = 0;
        //LOG("fonte%d -- %d",i+1, way[i]);
        //LOG("vizinho%d -- %d",i+1, neighbors[i]);
    }
}
/*função destinada a verificar o destino é alnçavel */
PRIVATE uint16_t hostreachable(uint16_t dest)
{
    //checa se é um destino inválido
    if (dest == 0 || (dest-1)>= MAX_HOSTS)
        return 0;

    int retorno;
    pthread_mutex_lock(&route_table_mutex);

    //verifica se o destino é alcançavel
    if(routes[dest-1].gateway != INFINITE_ROUTE)
    {
        //verifico se o gateway é o proprio host de origem
         if(routes[dest-1].gateway == num_global)
         {
            retorno = dest;
         }
         else
            retorno = routes[dest-1].gateway; // se não for, retorna o host vizinho que consegue alcançar o destino
    }
    else
        retorno = 0;
    pthread_mutex_unlock(&route_table_mutex);

    return retorno;
}

PRIVATE void *clean(void *param)
{
	
}