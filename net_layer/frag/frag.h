#ifndef FRAG_H
#define FRAG_H

#define NO_MORE_FRAGS 0
#define MORE_FRAGS 1 << 15
#define OFFSET_FIELD_LENGTH 15
#define OFFSET_FIELD_MASK ~(1 << OFFSET_FIELD_LENGTH)


int fragpack(offsetlst_t **frags, byte *data, size_t size, uint16_t mtu, uint16_t offset);
int fragpack2(offsetlst_t **frags, byte *data, size_t size, uint16_t mtu, uint16_t offset);

#endif