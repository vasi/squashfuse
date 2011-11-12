#include "swap.h"

#include <libkern/OSByteOrder.h>

uint16_t sqfs_swapin16(uint16_t v) {
	return OSSwapLittleToHostInt16(v);
}

uint32_t sqfs_swapin32(uint32_t v) {
	return OSSwapLittleToHostInt32(v);
}

uint64_t sqfs_swapin64(uint64_t v) {
	return OSSwapLittleToHostInt64(v);
}

#include "squashfs_fs.h"
#include "swap.c.inc"
