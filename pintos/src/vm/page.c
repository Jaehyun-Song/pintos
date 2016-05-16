#include <string.h>
#include <stdio.h>
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"

unsigned page_hash (const struct hash_elem *, void *);
bool page_less (const struct hash_elem *, const struct hash_elem *, void *);
void page_action (struct hash_elem *, void *);
bool load_seg (struct page *);
bool swap_in (struct page *);

void
init_page_table (struct hash *h)
{
  hash_init (h, page_hash, page_less, NULL);
}

void
free_page_table (struct hash *h)
{
  hash_destroy (h, page_action);
}

unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct page *p = hash_entry (p_, struct page, elem);

  return hash_bytes (&p->addr, sizeof p->addr);
}

bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, elem);
  const struct page *b = hash_entry (b_, struct page, elem);

  return a->addr < b->addr;
}

void
page_action (struct hash_elem *p_, void *aux UNUSED)
{
  struct page *p = hash_entry (p_, struct page, elem);
  struct thread *t = thread_current ();

  if (p->is_load)
  {
    free_frame (pagedir_get_page (t->pagedir, p->addr));
    pagedir_clear_page (t->pagedir, p->addr);
  }

  free (p);
}

bool
set_page_table_entry (void *addr, seg_type type, struct file *file, off_t ofs, size_t read_bytes)
{
  struct page *pte = (struct page *) malloc (sizeof (struct page));

  pte->addr = addr;
  pte->type = type;
  pte->is_load = false;
  pte->f = file;
  pte->read_bytes = read_bytes;
  pte->file_offset = ofs;
  pte->is_swap = false;
  pte->swap_offset = 0;

  return (hash_insert (&thread_current ()->pcb->page_table, &pte->elem) == NULL);
}

bool
lazy_loading (struct page *pte)
{
//printf ("lazy_loading1\n");
  if (pte->is_load)
    return false;

//printf ("lazy_loading2\n");
  //if (pte->type != SEG_STACK)
  //{
  if (pte->is_swap)
    return swap_in (pte);
  else
    return load_seg (pte);
  //}

  return false;
}

bool
stack_growth (void *addr)
{
  struct page *pte = page_lookup (pg_round_up (addr));
  void *frame_addr;
  bool recursive_fin = false;

  if (pte == NULL)
  {
    while (!recursive_fin)
	recursive_fin = stack_growth (pg_round_up (addr) + 4);

    pte = page_lookup (pg_round_up (addr));
  }
//printf ("hash size : %d\n", hash_size (&thread_current ()->pcb->page_table));

  if (pte->type != SEG_STACK)
    return false;

//printf ("0\n");
  if (!set_page_table_entry (pg_round_down (addr), SEG_STACK, NULL, 0, 0))
    return false;
  else
  {
    pte = page_lookup (pg_round_down (addr));
    frame_addr = set_frame (pte, 0);
  }
//printf ("1\n");
  if (!intf_install_page (pte->addr, frame_addr, true))
  {
    hash_delete (&thread_current ()->pcb->page_table, &pte->elem);
    free (pte);
    free_frame (frame_addr);
    return false;
  }
//printf ("2\n");

  return true;
}

bool
load_seg (struct page *pte)
{
  void *addr;
  bool writable = true;
//printf ("load_seg1\n");
  if (pte->read_bytes == 0)
    addr = set_frame (pte, 1);
  else
  {
    addr = set_frame (pte, 0);

    if (file_read_at (pte->f, addr, (off_t) pte->read_bytes, pte->file_offset) != (off_t) pte->read_bytes)
    {
//printf ("file_read_at : %d\n", file_read_at (pte->f, addr, (off_t) pte->read_bytes, pte->file_offset));
//printf ("pte->read_bytes : %d\n", pte->read_bytes);
	free_frame (addr);
	return false;
    }

    memset (addr + pte->read_bytes, 0, PGSIZE - pte->read_bytes);
  }

//printf ("load_seg2\n");
  if (pte->type == SEG_CODE)
    writable = false;

//printf ("load_seg3\n");
  if (!intf_install_page (pte->addr, addr, writable))
  {
    free_frame (addr);
    return false;
  }

//printf ("load_seg4\n");
  return true;
}

bool
swap_in (struct page *pte)
{
//printf ("swap_in\n");
  void *addr = set_frame (pte, 0);
  bool writable = true;
//printf ("AFTER SET_FRAME\n");
  if (pte->type == SEG_CODE)
    writable = false;

//printf ("BEFORE install_page\n");
//printf ("uaddr : %p, kaddr : %p\n", pte->addr, addr);
//printf ("AFTER install_page\n");
//printf ("BEFORE install_page\n");
  //if (!intf_install_page (pte->addr, addr, writable))
  if (!intf_install_page (pte->addr, addr, writable))
  {
    free_frame (addr);
//printf ("in the if in swap_in\n");
    return false;
  }
//printf ("BEFORE GET_FRAME\n");
  get_frame_in_block (pte->addr, pte->swap_offset);
  pte->is_load = true;

//printf ("FIN swap_in\n");
  return true;
}

struct page *
page_lookup (void *addr)
{
  struct page p;
  struct hash_elem *e;
  struct thread *t = thread_current ();

  p.addr = pg_round_down (addr);
  e = hash_find (&t->pcb->page_table, &p.elem);

  return e != NULL ? hash_entry (e, struct page, elem) : NULL;
}

