#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common.h>
#include <net_layer.h>
#include <checksum.h>
#include <frag.h>

#define DATA_SEGMENT(res, data, off, len) memcpy(res, data + off, len)

#ifdef DEBUG
#define LOG(s, ...) logdebug(stdout, NETWORK, s, ##__VA_ARGS__)
#else
#define LOG(s, ...)
#endif

PRIVATE void addheader(datagram_t **frag, byte *data, size_t size, uint16_t flag_offset);

/**
 * Fragmenta os dados e adiciona cada fragmento para uma lista especificada.
 * frags: a lista de fragmentos
 * data: os dados a serem fragmetados
 * size: o tamanho dos dados, sem incluir o cabeçalho IP
 * mtu: a MTU do enlace
 * offset: o offset a ser levado em consideração quando calcular novos offsets
*/
int fragpack(offsetlst_t **frags, byte *data, size_t size, uint16_t mtu, uint16_t offset)
{
	size_t frag_size;
	offsetlst_t *frags_ptr;
	byte *data_seg;
	size_t off;
	uint16_t pack_off;
	
	pack_off = offset & OFFSET_FIELD_MASK;
	off = 0;
	*frags = ALLOC(offsetlst_t, sizeof(offsetlst_t));
	frags_ptr = *frags;

	if (size + NET_HEADER_SIZE <= mtu)
	{
		//Não precisa de fragmentação. Vamos apenas adicionar o header...
		addheader(&frags_ptr->datagram, data, size, offset);
		return 0;
	}

	//Obtém o tamanho de cada fragmento
	frag_size = mtu - NET_HEADER_SIZE;
	//Cada fragmento precisa ser divisível por 8
	frag_size -= (frag_size % 8);
	data_seg = ALLOC(byte, frag_size);

	//Monta todos os fragmentos, menos o último, que vai ter a flag de mais fragmentos zerada
	while (TRUE)
	{
		//É o último fragmento?
		if ((size - off) <= frag_size)
		{
			LOG("Last one...");
			free(data_seg);
			frag_size = size - off;
			LOG("off: %zu", off);
			data_seg = ALLOC(byte, frag_size);
			DATA_SEGMENT(data_seg, data, off, frag_size);
			//Adiciona o cabeçalho IP para o último pacote com a flag de mais fragmentos zerada
			addheader(&frags_ptr->datagram, data_seg, frag_size, off + pack_off);
			//Fim da lista
			frags_ptr->next = NULL;
			free(data_seg);
			break;
		}
		else
		{
			LOG("off: %zu", off);
			DATA_SEGMENT(data_seg, data, off, frag_size);
			//Adiciona o cabeçalho IP com a flag de mais fragmentos setada
			addheader(&frags_ptr->datagram, data_seg, frag_size, (off + pack_off) | MORE_FRAGS);
			frags_ptr->next = ALLOC(offsetlst_t, sizeof(offsetlst_t));
			frags_ptr = frags_ptr->next;
		}
		
		off += frag_size;
	}

	return 1;
}

int fragpack2(offsetlst_t **frags, byte *data, size_t size, uint16_t mtu, uint16_t offset)
{
	size_t frag_size;
	offsetlst_t *frags_ptr;
	byte *data_seg;
	size_t off;
	uint16_t pack_off;
	uint16_t more;
	pack_off = offset >> OFFSET_FIELD_LENGTH;
	if(pack_off == 1)
	{
		more = MORE_FRAGS;
	}
	else
	{
		more = 0;
	}
	pack_off = offset & OFFSET_FIELD_MASK;
	off = 0;
	*frags = ALLOC(offsetlst_t, sizeof(offsetlst_t));
	frags_ptr = *frags;

	if (size + NET_HEADER_SIZE <= mtu)
	{
		//Não precisa de fragmentação. Vamos apenas adicionar o header...
		addheader(&frags_ptr->datagram, data, size, offset);
		return 0;
	}

	//Obtém o tamanho de cada fragmento
	frag_size = mtu - NET_HEADER_SIZE;
	//Cada fragmento precisa ser divisível por 8
	frag_size -= (frag_size % 8);
	data_seg = ALLOC(byte, frag_size);

	//Monta todos os fragmentos, menos o último, que vai ter a flag de mais fragmentos zerada
	while (TRUE)
	{
		//É o último fragmento?
		if ((size - off) <= frag_size)
		{
			LOG("Last one...");
			free(data_seg);
			frag_size = size - off;
			LOG("off: %zu", off);
			data_seg = ALLOC(byte, frag_size);
			DATA_SEGMENT(data_seg, data, off, frag_size);
			//Adiciona o cabeçalho IP para o último pacote com a flag de mais fragmentos zerada
			addheader(&frags_ptr->datagram, data_seg, frag_size, (off + pack_off) | more );
			//Fim da lista
			frags_ptr->next = NULL;
			free(data_seg);
			break;
		}
		else
		{
			LOG("off: %zu", off);
			DATA_SEGMENT(data_seg, data, off, frag_size);
			//Adiciona o cabeçalho IP com a flag de mais fragmentos setada
			addheader(&frags_ptr->datagram, data_seg, frag_size, (off + pack_off) | MORE_FRAGS);
			frags_ptr->next = ALLOC(offsetlst_t, sizeof(offsetlst_t));
			frags_ptr = frags_ptr->next;
		}
		
		off += frag_size;
	}

	return 1;
}

PRIVATE void addheader(datagram_t **frag, byte *data, size_t size, uint16_t flag_offset)
{
	datagram_t *aux;

	*(frag) = ALLOC(datagram_t, sizeof(datagram_t));
	aux = *frag;
	aux->size = size;
	aux->frag = flag_offset;
	aux->data = ALLOC(byte, size);
	memcpy(aux->data, data, size);
	aux->checksum = checksum(aux->data, size);
	LOG("size: %zu | offset: %u | flag: %u | data: %s | checksum: %u", size, flag_offset & OFFSET_FIELD_MASK, flag_offset >> 15, data, checksum);
}