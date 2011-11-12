#ifndef SQUFS_SWAP_H
#define SQFS_SWAP_H

#include <stdint.h>
#include <sys/types.h>

uint16_t sqfs_swapin16(uint16_t v);
uint32_t sqfs_swapin32(uint32_t v);
uint64_t sqfs_swapin64(uint64_t v);

void squashfuse_swapin_blocks(uint64_t *p, size_t n);

#include "squashfs_fs.h"
#include "swap.h.inc"

#endif
