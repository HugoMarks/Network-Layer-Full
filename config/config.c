/* Arquivo: config.c
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#include <config.h>
#include <ctype.h>
#include <string.h>
#include <arpa/inet.h>
#include <common.h>

#define MTU_STRING_SIZE			4
#define NODE_HEADER 			"Nos"
#define LINK_HEADER 			"Enlaces"
#define END_TEXT 				"Fim"
#define NEW_LINE 				"\n"
#define COMMA_CHAR				','
#define END_LINE_CHAR			';'

PRIVATE void readall(FILE *file, int links[MAX_HOSTS][MAX_HOSTS], host_t hosts[MAX_HOSTS]);
PRIVATE void readnodes(FILE *file, host_t *hosts);
PRIVATE void readlinks(FILE *file, int links[MAX_HOSTS][MAX_HOSTS]);
PRIVATE void processhostline(const char *line, host_t *ret);
PRIVATE int chartoint(char c);
PRIVATE void processlinkline(const char *line, int links[MAX_HOSTS][MAX_HOSTS]);

/*
 * Obtém a lista de hosts a partir de um arquivo e armazena o resultado na matriz especificada.
 * path: o arquivo a ser lido
 * hosts: a matriz (6x6 no máximo) que terá os hosts lidos.
*/
PUBLIC void readconfig(const char *path, int links[MAX_HOSTS][MAX_HOSTS], host_t hosts[MAX_HOSTS])
{
	FILE *file;

	if ((file = fopen(path, "r")) == NULL)
	{
		fprintf(stderr, "Cannot open the file \"%s\"\n.", path);
		return;
	}

	readall(file, links, hosts);
	fclose(file);
}

/*
 * Lê o arquivo até o fim e coloca os hosts no argumento hosts.
 * file: o arquivo a ser lido.
 * hosts: a matriz de hosts que é usada para guardar os hosts lidos.
 */
PRIVATE void readall(FILE *file, int links[MAX_HOSTS][MAX_HOSTS], host_t hosts[MAX_HOSTS])
{
	char *line = NULL;
	size_t len = 0;

	//Lê linha a linha
	while (getline(&line, &len, file) != -1)
	{
		//Se a linha conter apenas o text 'Fim', termina a leitura.
		if (strcmp(line, END_TEXT) == 0)
		{
			break;
		}

		//Ignora as linhas em branco.
		if (strcmp(line, NEW_LINE) == 0)
		{
			continue;
		}

		//Seção dos nós.
		if (strncmp(line, NODE_HEADER, strlen(NODE_HEADER)) == 0)
		{
			//Lê os nós (hosts).
			readnodes(file, hosts);
		}
		
		//Lê os enlaces (links).
		readlinks(file, links);
		
	}

	free(line);
}

PRIVATE void readnodes(FILE *file, host_t *hosts)
{
	char *line = NULL;
	size_t len = 0;
	int num = 0;

	//Lê linha a linha
	while (getline(&line, &len, file) != -1)
	{
		if (strncmp(line, LINK_HEADER, strlen(LINK_HEADER)) == 0)
		{
			break;
		}

		if(strcmp(line, NEW_LINE) == 0)
		{
			continue;
		}
	
		//processa a linha lida.
		processhostline(line, &hosts[num++]);
	}

	free(line);
}

PRIVATE void readlinks(FILE *file, int links[MAX_HOSTS][MAX_HOSTS])
{
	char *line = NULL;
	size_t len = 0;

	//Lê linha a linha
	while (getline(&line, &len, file) != -1)
	{
		if (strncmp(line, END_TEXT, strlen(END_TEXT)) == 0)
		{
			break;
		}

		if(strcmp(line, NEW_LINE) == 0)
		{
			continue;
		}

		processlinkline(line, links);

	}

	free(line);
}

/*
 * Processa uma linha, extraindo as informações de um link.
 * line: a linha a ser processada
 * ret: o endereço que será usado para armazenar o link lido na linha.
 */
PRIVATE void processlinkline(const char *line, int links[MAX_HOSTS][MAX_HOSTS])
{
	char *aux = (char *)line;
	int first = -1;
	int second = -1;
	int mtu;
	char mtu_string[MTU_STRING_SIZE+1];
	char *mtu_ptr = mtu_string;

	while (!isdigit(*aux))
	{
		aux++;
	}

	first += chartoint(*(aux++));

	while (!isdigit(*aux))
	{
		aux++;
	}

	second += chartoint(*(aux++));


	while (!isdigit(*aux))
	{
		aux++;
	}


	while (*aux != END_LINE_CHAR)
	{
		*(mtu_ptr++) = *(aux++);
	}

	mtu = strtol(mtu_string, NULL, 10);
	links[first][second] = links[second][first] = mtu;
}

/*
 * Processa uma linha, extraindo as informações de um host.
 * line: a linha a ser processada
 * ret: o endereço que será usado para armazenar o host lido na linha.
 */
PRIVATE void processhostline(const char *line, host_t *ret)
{
	char *aux = (char *)line;
	char ip[MAX_IP_STRING_SIZE + 1];
	char port[MAX_PORT_STRING_SIZE + 1];
	char *ip_ptr = ip;
	char *port_ptr = port;

	while (!isdigit(*aux))
	{
		aux++;
	}

	ret->num = chartoint(*(aux++));

	while (!isdigit(*aux))
	{
		aux++;
	}

	while (*aux != COMMA_CHAR)
	{
		*(ip_ptr++) = *(aux++);
	}

	*(ip_ptr) = '\0';
	inet_pton(AF_INET, ip, &ret->ip_addr);

	while (!isdigit(*aux))
	{
		aux++;
	}

	while (*aux != END_LINE_CHAR)
	{
		*(port_ptr++) = *(aux++);
	}

	ret->port = strtol(port, NULL, 10);
}


PRIVATE int chartoint(char c)
{
	return c - '0';
}


