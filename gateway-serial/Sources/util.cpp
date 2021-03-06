#include <stdlib.h>
#include "util.h"

uint32_t readNonce(const uint8_t *data)
{
    uint32_t nonce = *data++;
    nonce |= (uint32_t)*data++ << 8;
    nonce |= (uint32_t)*data++ << 16;
    nonce |= (uint32_t)*data++ << 24;
    return nonce;
}

uint16_t readUint16_t(const uint8_t *data) {
	uint16_t d = *data++;
	d |= (uint16_t)*data++ << 8;
	return d;
}

void writeNonce(uint8_t *data, uint32_t nonce)
{
    *data++ = nonce;
    *data++ = nonce >> 8;
    *data++ = nonce >> 16;
    *data++ = nonce >> 24;
}

uint32_t createNonce()
{
	uint32_t nonce = rand();
	nonce |= (uint32_t)rand() << 16;
    return nonce;
}



