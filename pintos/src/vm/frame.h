#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "vm/page.h"

struct list frame_table;

struct frame
  {
    void *addr;
    struct page *pte;
    struct thread *t;
    int access_cnt;
    struct list_elem elem;
  };

void init_frame_table (void);
void free_frame (void *);
void *set_frame (struct page *, bool zero_flag);
void *evict_frame (bool zero_flag);
struct frame *evict_policy (void);
void check_frame_accessed_recently (void);
void sort_frame_table (void);
bool check_access_cnt (const struct list_elem *, const struct list_elem *, void * UNUSED);

#endif /* vm/frame.h */
