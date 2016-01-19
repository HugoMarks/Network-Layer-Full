/* Arquivo: project.c
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <common.h>
#include <errno.h>
#include <link.h>
#include <net_layer.h>
#include <checksum.h>
#include <test_layer.h>
#include <net_layer_test.h>
#include <transport_layer.h>
#include <transport_layer_test.h>
#include <application_layer.h>
#include <unistd.h>
#include <pthread.h>

/**/
int links[MAX_HOSTS][MAX_HOSTS];
host_t hosts[MAX_HOSTS];

//mutex para sincronismo entre enlace e rede
pthread_mutex_t link_recv_mutex;
pthread_mutex_t net_recv_mutex;
pthread_mutex_t net2_recv_mutex;
pthread_mutex_t transp_recv_mutex;

/**/
frame_t *link_recv_frame;
netdata_t *net_recv_datagram;

PRIVATE void initlinks(void);
PUBLIC int get_max_mtu();

PUBLIC int main(int argc, char *argv[])
{
	int num;

	if (argc != 3)
	{
		fprintf(stderr, "Usage: project <host_num> <config_file>\n");
		exit(-1);
	}

	num = strtol(argv[1], NULL, 10);     
	//inicialização dos mutex
	pthread_mutex_init(&link_recv_mutex, NULL);
	pthread_mutex_init(&net_recv_mutex, NULL);
	pthread_mutex_init(&net2_recv_mutex, NULL);
	pthread_mutex_init(&transp_recv_mutex, NULL);
	pthread_mutex_lock(&net_recv_mutex);
	pthread_mutex_lock(&transp_recv_mutex); 
	/*Inicialização da camada de enlace*/
	initlinks();
	readconfig(argv[2], links, hosts);
	int max_mtu;
	max_mtu = get_max_mtu();
	link_init(num, link_recv_mutex, net_recv_mutex, &link_recv_frame, max_mtu);

	/*Inicialização da camada de teste*/
	//test_init(num, link_recv_mutex, net_recv_mutex, link_recv_frame);
	//sleep(2);
	net_init(num, link_recv_mutex, net_recv_mutex, net2_recv_mutex, transp_recv_mutex, link_recv_frame, &net_recv_datagram, links[num-1]);
	//sleep(2);
	/*Inicialização da camada de teste*/
	//net_layer_test_init(num, transp_recv_mutex, net2_recv_mutex, net_recv_datagram);
	
	transport_init(num, transp_recv_mutex, net2_recv_mutex, net_recv_datagram);
	
	/*Inicialização da camada de aplicação teste*/
	//transport_layer_test_init(num);
	
	/*Inicialização da camada de aplicação */
	application_layer_init(num);
		
	
	//while(1<2);
    return OK;
}
/*Função para procurar host */
PUBLIC host_t *findhost(uint16_t num)
{
	return &hosts[num - 1];
}
/*Função para pegar a MTU*/
PUBLIC int getmtu(uint16_t first, uint16_t second)
{
	return links[first - 1][second - 1];
}
/*Função para inicializar a topologia.*/
PRIVATE void initlinks(void)
{
	int i, j;

	for (i = 0; i < MAX_HOSTS; i++)
	{
		for (j = 0; j < MAX_HOSTS; j++)
		{
			links[i][j] = -1;
		}
	}
}

PUBLIC void config_link(int source, int dest, int mtu)
{
	links[source][dest] = mtu;
	links[dest][source] = mtu;	
}

PUBLIC int get_max_mtu()
{
	int i, j;
	int max = 0;
	for (i = 0; i < MAX_HOSTS; i++)
	{
		for (j = 0; j < MAX_HOSTS; j++)
		{
			if(max < links[i][j])
			{
				max = links[i][j];
			}
		}
	}
	printf("max : %d",max);
	return max;
}

