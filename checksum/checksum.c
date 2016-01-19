/* Arquivo: checksum.c
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#include <common.h>
#include <sys/types.h>
#include <inttypes.h>

/*
 * Calcula o checksum dos dados especificados.
 * data: os dados usados para calcular o checksum.
 * size: o tamanho dos dados
 */
PUBLIC uint16_t checksum(byte *data, size_t size)
{
    uint16_t *data_ptr = (uint16_t *)data;
	uint32_t sum = 0;

	//Soma os dados em pacotes de 16 bits.
	while (size > 1)
	{
		sum += *data_ptr++;
		//Incrementa de 2 em dois, pois queremos andar de 2 em 2 bytes (16 bits).
		size = size - 2;
	}

	//Se o size for > 0, significa que ele é ímpar, ou seja, sobraram 4 bits sem serem somados.
	if (size > 0)
	{
		//Soma os bits que faltaram.
		sum += *((uint8_t *)data_ptr);
	}

	//Se houver caries na soma, some todos.
	while (sum >> 16) //Há carries?
	{
		//Soma os 16 últimos bits (parte baixa) com os 16 primeiros bits (parte alta).
		sum = (sum & 0xFFFF) + (sum >> 16);
	}

	return (uint16_t)~sum;
}