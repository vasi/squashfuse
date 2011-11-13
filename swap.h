#ifndef SQUFS_SWAP_H
#define SQFS_SWAP_H

#include <stdint.h>

uint16_t sqfs_swapin16(uint16_t v);
uint32_t sqfs_swapin32(uint32_t v);
uint64_t sqfs_swapin64(uint64_t v);

#include "squashfs_fs.h"
#include "swap.h.inc"

#endif
