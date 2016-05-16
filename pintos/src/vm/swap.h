#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <bitmap.h>
#include "devices/block.h"

struct block *swap_block;
struct bitmap *swap_bitmap;

void init_swap (void);
size_t set_frame_in_block (void *);
void get_frame_in_block (void *, size_t map_offset);

#endif /* vm/swap.h */
