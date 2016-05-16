#include <stdio.h>
#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"
#include "threads/synch.h"

#define SECTOR_OFFSET (PGSIZE / BLOCK_SECTOR_SIZE)

struct lock swap_lock;
//int temp = 0;

void
init_swap (void)
{
  swap_block = block_get_role (BLOCK_SWAP);
  swap_bitmap = bitmap_create (block_size (swap_block) / SECTOR_OFFSET);
  bitmap_set_all (swap_bitmap, 0);
  lock_init (&swap_lock);
//printf ("block cnt : %d\n", block_size (swap_block) / SECTOR_OFFSET);
}

size_t
set_frame_in_block (void *addr)
{
  size_t map_offset, i;
  //enum intr_level old_level = intr_disable ();
  lock_acquire (&swap_lock);
  map_offset = bitmap_scan_and_flip (swap_bitmap, 0, 1, 0);
  //lock_release (&swap_lock);

  for (i = 0; i < SECTOR_OFFSET; i++)
    block_write (swap_block, (SECTOR_OFFSET * map_offset) + i, addr + (BLOCK_SECTOR_SIZE * i));

  lock_release (&swap_lock);
  //intr_set_level (old_level);
//temp++;
//printf ("temp : %d\n", temp);

  return map_offset;
}

void
get_frame_in_block (void *addr, size_t map_offset)
{
  size_t i;
  //enum intr_level old_level = intr_disable ();
  lock_acquire (&swap_lock);
  bitmap_flip (swap_bitmap, map_offset);
  //lock_release (&swap_lock);

  for (i = 0; i < SECTOR_OFFSET; i++)
    block_read (swap_block, (SECTOR_OFFSET * map_offset) + i, addr + (BLOCK_SECTOR_SIZE * i));

  lock_release (&swap_lock);
  //intr_set_level (old_level);
}
