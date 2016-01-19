/* Arquivo: application_layer.c
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */

#include <config.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <common.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include <transport_layer.h> 
#include <net_layer.h>
#include <application_layer.h>
#include <link.h>


#define LOG(s, ...) logdebug(stdout, APPLICATION, s, ##__VA_ARGS__)

PRIVATE void send_ips (uint16_t s, char **buf, size_t size);
PRIVATE void recv_ips (uint16_t s, char **buf);

/* função para fazer o download de um arquivo */
PRIVATE void download (uint16_t ips, char file_name[])
{
	struct timeval start_time, stop_time;
	float drift;
	FILE *file;
	char sendbuf[MAX_BUF_LENGTH], recvbuf[MAX_BUF_LENGTH];
	char *sendbufbptr, *recvbufbptr;
	int buflen, file_size, fz;
	uint8_t command = 2; //código para requisição de baixar um arquivo
	
	buflen = sizeof(int) + sizeof(uint8_t) + strlen(file_name) + 1;
	memcpy(sendbuf, &buflen, sizeof(int));
	memcpy(sendbuf + sizeof(int), &command, sizeof(uint8_t));
	memcpy(sendbuf + sizeof(int) + sizeof(uint8_t), file_name, strlen(file_name) + 1);
	sendbufbptr = sendbuf;
	//envia requisição para baixar o arquivo
	printf("enviando pedido\n");
	send_ips(ips, &sendbufbptr, (size_t)buflen);
	
	recvbufbptr = recvbuf;
	//recebe mensagem com o tamanho do arquivo
	printf("esperando tamanho\n");
	recv_ips(ips, &recvbufbptr);
	memcpy(&file_size, recvbuf + sizeof(int), sizeof(int));
	if(file_size == 0)
	{
		//arquivo nao existe então termina
		printf("tamanho foi 0\n");
		return;
	}
	fz = file_size;
	/*
	buflen = sizeof(uint32_t) + sizeof(uint8_t);
	memcpy(sendbuf, &buflen, sizeof(uint32_t));
	command = 1; //codigo de OK
	memcpy(sendbuf + sizeof(uint32_t), &command, sizeof(uint8_t));
	//envia OK para começar a receber o arquivo
	sendbufbptr = sendbuf;
	//printf("enviando OK\n");
	send_ips(ips, &sendbufbptr, (size_t)buflen);
	*/
	
	if((file = fopen(file_name,"w")) == NULL)
	{
		//não foi possivel criar o arquivo
		return;
	}
	
	//recebimento do arquivo
	printf("recebendo arquivo com tamanho %u\n", file_size);
	gettimeofday(&start_time, NULL);
	while(file_size > 0)
	{
		recvbufbptr = recvbuf;
		recv_ips(ips, &recvbufbptr);
		memcpy(&buflen, recvbuf, sizeof(int));
		buflen -= sizeof(int);
		file_size -= buflen;
		fwrite(recvbuf + sizeof(int), sizeof(char), buflen, file);
		printf("recebendo: %u restante: %u\n", buflen, file_size);
		
	}
	gettimeofday(&stop_time, NULL);
	drift = (float)(stop_time.tv_sec  - start_time.tv_sec);
	drift += (stop_time.tv_usec - start_time.tv_usec)/(float)1000000;
	printf("Duracao da transferencia: %.6f segundos\n", drift);
	printf("Vazao media da transferencia: %.6f bytes/segundos\n", fz/drift);
	fclose(file);
}

/* função para fazer o upload de um arquivo */
PRIVATE void upload (uint16_t ips)
{
	FILE *file;
	char sendbuf[MAX_BUF_LENGTH], recvbuf[MAX_BUF_LENGTH];
	char *sendbufbptr, *recvbufbptr;
	int buflen, file_size;
	uint8_t command;
	char file_name[50];
	struct stat file_stat; 
	printf("aguardando pedido\n");
	recvbufbptr = recvbuf;
	//recebe mensagem com a requisição
	recv_ips(ips, &recvbufbptr);
	
	printf("pedido recebido\n");
	memcpy(&buflen, recvbuf, sizeof(int));
	memcpy(file_name, recvbuf + sizeof(int) + sizeof(uint8_t), buflen - (sizeof(int) + sizeof(uint8_t)));
	//faz a abertura do arquivo
	if((file = fopen(file_name,"r")) == NULL)
	{
		printf("tamanho foi zero\n");
		file_size = 0;
	}
	else
	{
		//usa-se stat() para pegar as informações de um caminho(neste caso o caminho do arquivo)
		stat(file_name, &file_stat);
		file_size = file_stat.st_size;
	}
	printf("enviando arquivo com tamanho %u\n",file_size);
	
	//Incrementa o tamanho da mensagem a ser enviada.
	buflen = (sizeof(int)*2);
	//Copia o tamanho da mensagem para o buffer a ser enviado
	memcpy(sendbuf, &buflen, sizeof(int));	
	memcpy(sendbuf +  sizeof(int), &file_size, sizeof(int));
	sendbufbptr = sendbuf;
	//Envia a mensagem com tamanho do arquivo
	printf("enviando tamanho\n");
	send_ips(ips, &sendbufbptr, (size_t)buflen);
	
	if(file == NULL)
	{
		return;
	}
	/*
	//printf("esperando ok\n");
	//recebe um OK
	recvbufbptr = recvbuf;
	recv_ips(ips, &recvbufbptr);
	*/
	//enviando o arquivo
	while(file_size > 0)
	{		
		buflen = MAX_BUF_LENGTH - sizeof(int);
		buflen = fread(sendbuf + sizeof(int), sizeof(char), buflen, file);
		file_size -= buflen;
		buflen += sizeof(int);
		memcpy(sendbuf, &buflen, sizeof(int));
		sendbufbptr = sendbuf;
		//envia mensagem
		send_ips(ips, &sendbufbptr, (size_t)buflen);
		printf("enviando: %u restante: %u\n", buflen - sizeof(int), file_size);
		
	}
	fclose(file);
	printf("finalizando envio\n");
}

PUBLIC int application_layer_init(uint16_t num_host)
{
    int option =0, tam_window =0, ret_send=0, ret_recived=0, tam_buffer=0;
	uint16_t ips =0, dest=0, dest_ips=0, ret_connect =0, sp;
	char file_name[50];
	int l, c, d, mtu;
	uint16_t remote_host;
	//entra num loop at� o usuário solicitar o termino do programa
    while(option != 14)
   {
		LOG("BEM VINDO A CAMADA DE APLICACAO \n1 - Abrir ponto de servico(<void>)\n2 - Fechar ponto de servico(<IPS>)\n3 - conectar(<No de Destino>,<IPS>,<tamanho da janela>)\n4 - Fechar conexão do ponto de servico(<IPS>)\n5 - enviar(<IPS>)\n6 - receber(<IPS>)\n7 - baixar(<IPS>, <nome_arquivo>)\n8 - enviar arquivo(<IPS>)\n9 - Exibe tabela de rotas\n10 - Configurar o adulterador\n11 - Desabilitar enlace\n12 - Habilitar enlace\n13 - Depurar\n14 - Sair da camada de aplicacao\n Informe o numero do que deseja executar: ");
		scanf("%d",&option);
		__fpurge(stdin);
		switch(option)
		{
			case 1: //responsavel por abertura do ponto de servico
				LOG("Solicitacao de ponto de servico realizada!");
				
				ips = alloc_service_point();
				LOG ("Numero do ponto de servico: %u\n", ips);

				break;
			case 2: //responsavel por fechamento do ponto de servico
				LOG("Informe o ips:");
				scanf("%hd",&sp);
				__fpurge(stdin);
				dalloc_service_point(sp);
				LOG ("Ponto de servico %u fechado com sucesso!\n", sp);
				break;
			case 3: //responsavel por conectar
				LOG("Solicitacao para conectar!");
				
				LOG("Informe o ips origem:");
				scanf("%hd",&sp);
				__fpurge(stdin);
				
				LOG("Informe o destino (no):");
				scanf("%hd",&dest);
				__fpurge(stdin);
				
				LOG("Informe o destino (ips):");
				scanf("%hd",&dest_ips);
				__fpurge(stdin);
				
				LOG("Informe o tamanho da janela:");
				scanf("%d",&tam_window);
				__fpurge(stdin);
				
				ret_connect = connect_point(sp, dest, dest_ips,tam_window);
				LOG ("Retorno do conectar = %d\n", ret_connect);
				break;
			case 4: //responsavel por fechamento de conexão
				LOG("Informe o ips:");
				scanf("%hd",&sp);
				__fpurge(stdin);
				close_conn(sp);
				LOG ("Ponto de servico %u desconectado com sucesso!\n", sp);
				break;
			case 5: //responsavel por funcao de enviar
				LOG("Solicitacao para enviar!");
				
				LOG("Informe o ips:");
				scanf("%hd",&sp);
				__fpurge(stdin);
				
				LOG("Informe o tamanho (bytes) da mensagem que deseja enviar:");
				scanf("%d",&tam_buffer);
				__fpurge(stdin);
				
				char *str = (char*) malloc(tam_buffer * sizeof(char));
				 
				LOG("Infome a msg que deseja enviar:");
				scanf("%[^\n]",str);
				__fpurge(stdin);
				LOG ("MSG recebida: %s\n", str);
				ret_send = send_point(sp, str, (tam_buffer * sizeof(char)));
				LOG ("Tamanho da msg: %u",(tam_buffer * sizeof(char)));
				LOG ("Quantidade de bytes enviados = %d\n", ret_send);
				tam_buffer =0;
				free(str);
				break;
			case 6: //responsavel por funcao de receber
				LOG("Solicitacao para receber!");
				
				LOG("Informe o ips:");
				scanf("%hd",&sp);
				__fpurge(stdin);
				
				LOG("Informe o tamanho (bytes) da mensagem que deseja receber:");
				scanf("%d",&tam_buffer);
				__fpurge(stdin);
				
				char *str2 = (char*) malloc(tam_buffer * sizeof(char));
				
				ret_recived = recv_point(sp, str2, (tam_buffer * sizeof(char)));
				
				LOG ("Quantidade de bytes recebidos = %d", ret_recived);
				LOG ("MSG recebida: %s\n", str2);
				
				tam_buffer =0;
				free(str2);
				break;
			case 7: //responsavel por fazer o download de um arquivo
				LOG("Informe o ips:");
				scanf("%hd",&sp);
				__fpurge(stdin);
				LOG("Infome o nome do arquivo que deseja baixar:");
				scanf("%[^\n]",file_name);
				__fpurge(stdin);
				download(sp, file_name);
				break;
			case 8: //responsavel por fazer o upload de um arquivo
				LOG("Informe o ips:");
				scanf("%hd",&sp);
				__fpurge(stdin);
				upload(sp);
				break;
			case 9:
				/* Exibe a tabela de rotas*/
				dislpay_routes();
				break;
			case 10: //configura o adulterador
				//conf_adulterador(Perda, Corrupção, Duplicação), ca(...)
				LOG("Configuracao do adulterador:");
				printf("Informe a porcentagem de perda:");
				scanf("%d",&l);
				__fpurge(stdin);
				printf("Informe a porcentagem de corrupcao:");
				scanf("%d",&c);
				__fpurge(stdin);
				printf("Informe a porcentagem de duplicacao:");
				scanf("%d",&d);
				__fpurge(stdin);
				config_garbler(l, c, d);
				break;
			case 11: //desabilita um enlace
				//desabilitar_enlace(NóRem), de(...)
				printf("Informe o no remoto para desabilitar:");
				scanf("%hu", &remote_host);
				__fpurge(stdin);
				disable_route(remote_host);
				config_link(num_host, remote_host, -1);
				break;
			case 12: //habilita um enlace
				//habilitar_enlace(NóRem), he(...)
				printf("Informe o no remoto para habilitar:");
				scanf("%hu", &remote_host);
				__fpurge(stdin);
				printf("Informe mtu para habilitar:");
				scanf("%d",&mtu);
				__fpurge(stdin);
				enable_route(remote_host);
				config_link(num_host, remote_host, mtu);
				break;
			case 13: //depurar
				//depurar, dp
				depurar();
				break;
			case 14: //Fecha a camada de Aplicação
				LOG("Camada de APLICACAO terminou com sucesso.\n");
				break;
			default:
				LOG("Opcao invalida! Escolha Novamente.\n");
				break;
		}
   }
   return OK;

}


PRIVATE void send_ips (uint16_t s, char **buf, size_t size)
{
  uint32_t bytes; //n�mero de bytes enviados
  char *sendbufbptr = *buf;

  //Envia os dados
  while ((bytes = send_point(s, sendbufbptr, size))>=0)
  {
    //Avan�a o n�mero de bytes enviados
    sendbufbptr += bytes;
    //Subtrai o n�mero de bytes enviados
    size -= bytes;
    //Se n�o h� mais bytes para enviar, sai do while
    if(size == 0)
      break;
  }
}

PRIVATE void recv_ips (uint16_t s, char **buf)
{
  uint32_t bytes; //n�mero de bytes recebidos
  uint32_t buflen = sizeof(uint32_t); //tamanho do buffer
  char *recvbufbptr = *buf; //ponteiro para o buffer
  
  //Recebe o tamanho do pacote
  while ((bytes = recv_point(s, recvbufbptr, buflen))>=0)
  {
    //Avan�a o n�mero de bytes lidos
    recvbufbptr += bytes;
    buflen -= bytes;
    //Se n�o h� mais bytes para ler, sai do while
    if(buflen == 0)
      break;
  }

  //Copia os bytes lidos para o buflen
  memcpy(&buflen, *buf, sizeof(uint32_t));
  buflen-=sizeof(uint32_t);
  //Recebe o resto do pacote baseado no tamanho recebido
  while ((bytes = recv_point(s, recvbufbptr, buflen))>=0)
  {
    //Avan�a o n�mero de bytes lidos
    recvbufbptr += bytes;
    buflen -= bytes;
    //Se n�o h� mais bytes para ler, sai do while
    if(buflen == 0)
      break;
  }
}