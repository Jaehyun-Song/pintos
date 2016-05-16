#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "filesys/filesys.h"

typedef enum seg
  {
    SEG_CODE,
    SEG_DATA,
    SEG_STACK,
    SEG_MMAP
  } seg_type;

struct page
  {
    void *addr;
    seg_type type;
    bool is_load;

    struct file *f;
    size_t read_bytes;
    off_t file_offset;

    bool is_swap;
    size_t swap_offset;

    struct hash_elem elem;
  };

void init_page_table (struct hash *);
void free_page_table (struct hash *);
bool set_page_table_entry (void *, seg_type type, struct file *, off_t ofs, size_t read_bytes);
bool lazy_loading (struct page *);
bool stack_growth (void *);
struct page *page_lookup (void *);

#endif /* vm/page.h */
