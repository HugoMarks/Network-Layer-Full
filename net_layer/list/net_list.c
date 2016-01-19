/* Arquivo: net_list.c
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */

#include <string.h>
#include <common.h>
#include <net_layer.h>
#include <net_list.h>

#ifdef DEBUG2
#define LOG(s, ...) logdebug(stdout, NETWORK, s, ##__VA_ARGS__)
#else
#define LOG(s, ...)
#endif

PRIVATE bool addoffset(offsetlst_t **offs, datagram_t *datagram);
PRIVATE void initpacklst(packlst_t **packlst, uint16_t id, uint16_t src, size_t size);

PUBLIC packlst_t *getid(packlst_t **list, uint16_t id, uint16_t src)
{
	datagram_t pack;
	packlst_t *list_ptr;
	list_ptr = *list;
	
	while(list_ptr)
	{
		if(list_ptr->id == id && list_ptr->src == src)
		{
			return list_ptr;
		}
		list_ptr = (packlst_t *)list_ptr->next;
	}
	return NULL;
}

PRIVATE void removeid(packlst_t **packlst, uint16_t id, uint16_t src)
{
	packlst_t *aux;
	packlst_t *prev;

	if ((*packlst) == NULL)
	{
		return;
	}

	aux = *packlst;
	prev = NULL;

	while (aux)
	{
		if (aux->id == id && aux->src == src)
		{
			break;
		}

		prev = aux;
		aux = (packlst_t *)aux->next;
	}

	if (aux == NULL)
	{
		return;
	}

	if (prev == NULL)
	{
		(*packlst) = (packlst_t *)aux->next;
		free(aux);
		return;
	}

	prev->next = aux->next;
	free(aux);
}

/*
 * Função para retornar todos os fragmentos 
 * em formato de datagram_t
 */
PUBLIC byte *getpack(packlst_t **list, uint16_t id, uint16_t src, size_t *size)
{
	byte *data = NULL;
	packlst_t *pack_ptr;
	offsetlst_t *off_ptr;
	offsetlst_t *aux;
	size_t count = 0;
	LOG("get pack 0");
	pack_ptr = getid(list, id, src);
	LOG("get pack 1");
	if(pack_ptr)
	{
		data = ALLOC(byte, pack_ptr->total_size);
		*size = pack_ptr->total_size; 
		off_ptr = pack_ptr->offs;
		LOG("get pack 2");
		while(off_ptr)
		{LOG("get pack 3 count %d  size %d",count, off_ptr->datagram->size);
			memcpy(data + count, off_ptr->datagram->data, off_ptr->datagram->size);
			LOG("get pack 4");
			count += off_ptr->datagram->size;
			aux = off_ptr;
			off_ptr = off_ptr->next;
			//LOG("get pack 5 %s", off_ptr->datagram->data);
			//free(&off_ptr->datagram->data);
			LOG("get pack 5 1");
			//free(&off_ptr->datagram);
			LOG("get pack 6");
			//free(aux);
		}
		LOG("get pack 7 data %s", data);
		removeid(list, pack_ptr->id, pack_ptr->src);
	}
	return data;
}

PRIVATE bool hasmorefrags(const packlst_t *packlst)
{
	return packlst->count_size < packlst->total_size ? TRUE : FALSE;
}

PRIVATE bool islastfrag(datagram_t *datagram)
{
	return datagram->frag >> OFFSET_FIELD_LENGTH ? FALSE : TRUE;
}

/**
 * Insere o datagrama especificado na lista de offsets da lista de ids.
 * packlst: a lista de ids que será criada ou usada para inserir o datagrama
 * datagram: o datagram a ser inserido.
 */
PUBLIC bool insertoffset(packlst_t **packlst, datagram_t *datagram)
{
	packlst_t *aux;
	bool has_more;
	bool last_frag = islastfrag(datagram);

	LOG("insertoffset");
	//Não há nenhum id alocado ainda...
	if ((*packlst) == NULL)
	{
		LOG("1 insertoffset -> id %u nao existe", datagram->id);
		//... então precisamos alocar
		initpacklst(packlst, datagram->id, datagram->src, datagram->size);
		if((*packlst) == NULL)
			LOG("Esta NULL ---------");
		addoffset(&((*packlst)->offs), datagram);
		//aux = *packlst;
		//aux->count_size += datagram->size;

		//É o último fragmento?
		if (last_frag)
		{
			//Se for o último pacote, sabemos o tamanho da mensagem
			LOG("atualiza total size");
			(*packlst)->total_size = datagram->size + (datagram->frag & OFFSET_FIELD_MASK);
			//return TRUE;
		}
		//verifica se ta completo
		if((*packlst)->total_size == (*packlst)->count_size)
		{
			LOG("insertoffset TRUE 1");
			return TRUE;
		}
		LOG("insertoffset FALSE 1");
		return FALSE;
	}

	//Procura pelo id do datagrama
	aux = getid(packlst, datagram->id, datagram->src);
	LOG("2 insertoffset -> id %u %s", datagram->id, aux == NULL ? "nao existe" : "existe");
	
	if (aux == NULL)
	{
		initpacklst(packlst, datagram->id, datagram->src, datagram->size);
		addoffset((&((*packlst)->offs)), datagram);
		//aux->count_size += datagram->size;
		
		//É o último pacote?
		if (last_frag)
		{
			//É o último pacote, então sabemos o tamanho total da mensagem...
			aux->total_size = datagram->size + (datagram->frag & OFFSET_FIELD_MASK);
		}
		//verifica se ta completo
		if((*packlst)->total_size == (*packlst)->count_size)
		{
			LOG("insertoffset TRUE 2");
			return TRUE;
		}
		LOG("insertoffset FALSE 2");
		return FALSE;
	}
	
	if(addoffset(&(aux->offs), datagram) == FALSE)
	{
		LOG("insertoffset FALSE 3");
		return FALSE;
	}
	//Atualiza o número de bytes recebidos
	aux->count_size += datagram->size;
	//É o último pacote?
	if (last_frag)
	{LOG("ultimo frag");
		//É o último pacote, então sabemos o tamanho total da mensagem...
		aux->total_size = datagram->size + (datagram->frag & OFFSET_FIELD_MASK);
	}
	//verifica se ta completo
	LOG("tam aux %d --- total %d", aux->count_size,  aux->total_size);
	if(aux->total_size == aux->count_size)
	{
		LOG("insertoffset TRUE 3");
		return TRUE;
	}
	LOG("insertoffset FALSE 4");
	return FALSE;
	//has_more = hasmorefrags(aux);
	//LOG("has_more %s", has_more == TRUE ? "true" : "false");		
	//return has_more;
}

/**
 * Adiciona um fragmento na lista de fragmentos, de forma odernada.
 * offs: lista de offsets
 * datagram: o datagrama a ser inserido
*/
PRIVATE bool addoffset(offsetlst_t **offs, datagram_t *datagram)
{
	LOG("addoffset");
	if ((*offs) == NULL)
	{
		LOG("addoffset -> offs = NULL");
		(*offs) = ALLOC(offsetlst_t, sizeof(offsetlst_t));
		(*offs)->datagram = datagram;
		(*offs)->next = NULL;
		return TRUE;
	}
	
	LOG("addoffset -> offs != NULL");
	offsetlst_t *aux = *offs;
	offsetlst_t *prev = NULL;
	LOG("addoffset -> offset: %u", datagram->frag & OFFSET_FIELD_MASK);
	while (aux->next)
	{
		uint16_t current_frag = aux->datagram->frag & OFFSET_FIELD_MASK;
		LOG("atual %d", current_frag);
		uint16_t pack_frag = datagram->frag & OFFSET_FIELD_MASK;
		LOG("addoffset -> current_frag: %u | pack_frag: %u | datagram->size: %zu", current_frag, pack_frag, datagram->size);
		if (current_frag + datagram->size == pack_frag)
		{
			LOG("addoffset -> Achou frag (%u)!", pack_frag);
			//return FALSE;
			break;
		}

		prev = aux;
		aux = aux->next;
	}

	if (!aux->next)
	{
		LOG("addoffset -> aux->next = NULL");
		//verifica se não é o mesmo que ta na lista
		LOG("list %d --- new %d", aux->datagram->frag & OFFSET_FIELD_MASK, datagram->frag & OFFSET_FIELD_MASK);
		if((aux->datagram->frag & OFFSET_FIELD_MASK) != (datagram->frag & OFFSET_FIELD_MASK))
		{
			aux->next = ALLOC(offsetlst_t, sizeof(offsetlst_t));
			aux = aux->next;
			aux->datagram = datagram;
			aux->next = NULL;
			LOG("addoffset true");
			return TRUE;
		}
		LOG("addoffset false");
		return FALSE;
	}

	aux->next = ALLOC(offsetlst_t, sizeof(offsetlst_t));
	aux = aux->next;
	aux->datagram = datagram;
	aux->next = prev->next;
	prev->next = aux;
	LOG("addoffset -> done");
	return TRUE;
}

PRIVATE void initpacklst(packlst_t **packlst, uint16_t id, uint16_t src, size_t size)
{
	packlst_t *aux;
	//offsetlst_t *offs;
	
	aux = *packlst;
	(*packlst) = ALLOC(packlst_t, sizeof(packlst_t));
	//aux = *packlst;
	(*packlst)->id = id;
	(*packlst)->src = src;
	(*packlst)->total_size = 0;
	(*packlst)->count_size = size;
	(*packlst)->offs = NULL;
	(*packlst)->next = (struct packlst_t *)aux;
	
}