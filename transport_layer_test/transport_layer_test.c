/* Arquivo: transport_layer_test.c
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
#include <common.h>
#include <pthread.h>
#include <errno.h>
#include <transport_layer.h> 
#include <transport_layer_test.h> 


#define LOG(s, ...) logdebug(stdout, APPLICATION, s, ##__VA_ARGS__)

PUBLIC int transport_layer_test_init(uint16_t num_host)
{
    int option =0, tam_window =0, ret_send=0, ret_recived=0, tam_buffer=0;
	uint16_t ips =0, dest=0, dest_ips=0, ret_connect =0;
	
	//entra num loop at� o usuário solicitar o termino do programa
    while(option != 7)
   {
		LOG("BEM VINDO A CAMADA DE TESTE - TRANSPORTE\n1 - Abrir ponto de servico(<void>)\n2 - Fechar ponto de servico(<IPS>)\n3 - conectar(<No de Destino>,<IPS>,<tamanho da janela>)\n4 - Fechar conexão do ponto de servico(<IPS>)\n5 - enviar(<IPS>)\n6 - receber(<IPS>)\n7- Sair da camada de teste\n Informe o numero do que deseja executar: ");
		scanf("%d",&option);
		__fpurge(stdin);
		switch(option)
		{
			case 1: //responsavel por testar a abertura do ponto de servico
				LOG("Solicitacao de ponto de servico realizada!");
				
				ips = alloc_service_point();
				LOG ("Numero do ponto de servico: %u\n", ips);

				break;
			case 2: //responsavel por testar o fechamento do ponto de servico
				dalloc_service_point(ips);
				LOG ("Ponto de servico %u fechado com sucesso!\n", ips);
				break;
			case 3: //responsavel por testar conectar
			    LOG("Solicitacao para conectar!");
				LOG("Informe o destino (no):");
				scanf("%hd",&dest);
				__fpurge(stdin);
				
				LOG("Informe o destino (ips):");
				scanf("%hd",&dest_ips);
				__fpurge(stdin);
				
				LOG("Informe o tamanho da janela:");
				scanf("%d",&tam_window);
				__fpurge(stdin);
				
				ret_connect = connect_point(ips, dest, dest_ips,tam_window);
				LOG ("Retorno do conectar = %d\n", ret_connect);
				break;
			case 4: //responsavel por testar o fechamento de conexão
				close_conn(ips);
				LOG ("Ponto de servico %u desconectado com sucesso!\n", ips);
				break;
			case 5: //responsavel por testar a funcao de enviar
				LOG("Solicitacao para enviar!");
				
				LOG("Informe o tamanho (bytes) da mensagem que deseja enviar:");
				scanf("%d",&tam_buffer);
				__fpurge(stdin);
				
				char *str = (char*) malloc(tam_buffer * sizeof(char));
				 
				LOG("Infome a msg que deseja enviar:");
				scanf("%[^\n]",str);
				__fpurge(stdin);
				LOG ("MSG recebida: %s\n", str);
				
				ret_send = send_point(ips, str, (tam_buffer * sizeof(char)));
				
				LOG ("Tamanho da msg: %u",(tam_buffer * sizeof(char)));
				LOG ("Quantidade de bytes enviados = %d\n", ret_send);
				tam_buffer =0;
				free(str);
				break;
			case 6: //responsavel por testar a funcao de receber
				LOG("Solicitacao para receber!");
				
				LOG("Informe o tamanho (bytes) da mensagem que deseja receber:");
				scanf("%d",&tam_buffer);
				__fpurge(stdin);
				
				char *str2 = (char*) malloc(tam_buffer * sizeof(char));
				
				ret_recived = recv_point(ips, str2, (tam_buffer * sizeof(char)));
				
				LOG ("Quantidade de bytes recebidos = %d", ret_recived);
				LOG ("MSG recebida: %s\n", str2);
				
				tam_buffer =0;
				free(str2);
				break;
			case 7:
				/* Fecha a camada de teste */
				LOG("Camada de TESTE terminou com sucesso.\n");
				break;
			default:
				LOG("Opcao invalida! Escolha Novamente.\n");
				break;
		}
   }
   return OK;

}


