/* Arquivo: checksum.h
 * Autores: 
 * André Matheus Nicoletti - 12643797
 * Hugo Marques Casarini - 12014742
 * Thamer El Ayssami - 12133302
 * Yago Elias - 12146007
 */
#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <common.h>

#define CHECKSUM_SIZE sizeof(uint16_t)

uint16_t checksum(byte *data, size_t size);

#endif