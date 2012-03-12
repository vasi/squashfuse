#include "swap.h"

#define SWAP(BITS) \
	void sqfs_swapin##BITS(uint##BITS##_t *v) { \
		uint8_t *c = (uint8_t*)v; \
		uint##BITS##_t r = 0; \
		for (int i = sizeof(*v) - 1; i >= 0; --i) { \
			r <<= 8; \
			r += c[i]; \
		} \
		*v = r; \
	}

SWAP(16)
SWAP(32)
SWAP(64)
#undef SWAP

#include "squashfs_fs.h"
#include "swap.c.inc"
