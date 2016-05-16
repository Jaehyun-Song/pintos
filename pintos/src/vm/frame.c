#include <stdio.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include "filesys/file.h"

struct lock frame_lock;

struct frame *find_frame (void *);

void
init_frame_table (void)
{
  list_init (&frame_table);
  lock_init (&frame_lock);
}

void
free_frame (void *addr)
{
  struct frame *f;
  enum intr_level old_level = intr_disable ();

  f = find_frame (addr);

  if (f != NULL)
  {
    list_remove (&f->elem);
    free (f);
    palloc_free_page (addr);
  }

  intr_set_level (old_level);
}

void *
set_frame (struct page *pte, bool zero_flag)
{
  struct frame *f;
  void *addr;
  enum intr_level old_level;

  if (zero_flag)
    addr = palloc_get_page (PAL_USER | PAL_ZERO);
  else
    addr = palloc_get_page (PAL_USER);

  while (addr == NULL)
  {
    lock_acquire (&frame_lock);
    addr = evict_frame (zero_flag);
    lock_release (&frame_lock);
  }

  f = (struct frame *) malloc (sizeof (struct frame));
  f->addr = addr;
  f->pte = pte;
  f->t = thread_current ();
  f->access_cnt = 0;
  old_level = intr_disable ();
  list_push_back (&frame_table, &f->elem);
  intr_set_level (old_level);

  f->pte->is_load = true;

  return addr;
}

void *
evict_frame (bool zero_flag)
{
  struct frame *f = evict_policy ();
  enum intr_level old_level;

  if (pagedir_is_dirty (f->t->pagedir, f->pte->addr))
  {
    if (f->pte->type == SEG_MMAP)
    {
	lock_acquire (&file_lock);
	file_write_at (f->pte->f, f->addr, f->pte->read_bytes, f->pte->file_offset);
	lock_release (&file_lock);
    }
    else
    {
	f->pte->is_swap = true;
	f->pte->swap_offset = set_frame_in_block (f->addr);
    }
  }

  f->pte->is_load = false;
  pagedir_clear_page (f->t->pagedir, f->pte->addr);
  palloc_free_page (f->addr);
  old_level = intr_disable ();
  list_remove (&f->elem);
  intr_set_level (old_level);
  free (f);

  if (zero_flag)
    return palloc_get_page (PAL_USER | PAL_ZERO);
  else
    return palloc_get_page (PAL_USER);
}

struct frame *
evict_policy (void)
{
  /*struct list_elem *e = list_begin (&frame_table);
  struct frame *f = list_entry (e, struct frame, elem);
  enum intr_level old_level = intr_disable ();

  while (!is_from_user_pool (f->addr))
  {
    e = list_next (e);
    list_remove (&f->elem);
    list_push_back (&frame_table, &f->elem);
    f = list_entry (e, struct frame, elem);
  }

  intr_set_level (old_level);

  return f;*/


  struct list_elem *e = list_begin (&frame_table);

  return list_entry (e, struct frame, elem);
}

void
check_frame_accessed_recently (void)
{
  struct frame *f;
  struct list_elem *e;
  enum intr_level old_level = intr_disable ();

  for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e))
  {
    f = list_entry (e, struct frame, elem);

    if (pagedir_is_accessed (f->t->pagedir, f->pte->addr))
    {
	f->access_cnt++;
	pagedir_set_accessed (f->t->pagedir, f->pte->addr, false);
    }
  }

  intr_set_level (old_level);

  return;
}

void
sort_frame_table (void)
{
  enum intr_level old_level = intr_disable ();

//print_all_list ();
  list_sort (&frame_table, check_access_cnt, NULL);
  intr_set_level (old_level);

  return;
}

struct frame *
find_frame (void *addr)
{
  struct frame *f;
  struct list_elem *e;

  for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e))
  {
    f = list_entry (e, struct frame, elem);

    if (addr == f->addr)
	return f;
  }

  return NULL;
}

bool
check_access_cnt (const struct list_elem *e1, const struct list_elem *e2, void *aux UNUSED)
{
  struct frame *f1 = list_entry (e1, struct frame, elem);
  struct frame *f2 = list_entry (e2, struct frame, elem);

  return f1->access_cnt < f2->access_cnt;
}
