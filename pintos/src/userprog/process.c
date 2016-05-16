#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"	// IMTC
#include "threads/synch.h"	// IMTC
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"		// IMTC
#include "vm/frame.h"		// IMTC

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp, char **save_ptr);
int argument_length (char **save_ptr, int *argc);				// IMTC
void insert_argument_to_array (char *dst, const char *src_, char **save_ptr, int dst_offset, int src_length, int padding);	// IMTC
void free_mmap_pte_list (struct thread *, struct list *, struct file *);	// IMTC
void terminate_mmap_list (struct PCB *);	// IMTC
void terminate_descriptor (struct PCB *);	// IMTC
void terminate_child (struct PCB *);		// IMTC

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  char *save_ptr;	// IMTC
  char *temp_file_name;	// IMTC
  int temp_file_length;	// IMTC
  struct PCB *pcb_;	// IMTC
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  temp_file_length = strlen (file_name);	// IMTC
  temp_file_name = (char *) malloc (temp_file_length + 1);	// IMTC
  memcpy (temp_file_name, file_name, temp_file_length + 1);	// IMTC
  temp_file_name = strtok_r (temp_file_name, " ", &save_ptr);	// IMTC

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (temp_file_name, PRI_DEFAULT, start_process, fn_copy);	// IMTC
  free (temp_file_name);	// IMTC

  if (tid == TID_ERROR)
    palloc_free_page (fn_copy);

  pcb_ = find_child_PCB (tid);	// IMTC

  if (pcb_ == NULL)		// IMTC
    thread_exit ();		// IMTC

  sema_down (&pcb_->exec);	// IMTC

  if (!pcb_->is_load)	// IMTC
    tid = ERROR;			// IMTC
 
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  struct thread *t;		// IMTC
  char *file_name = file_name_;
  char *save_ptr;		// IMTC
  struct intr_frame if_;
  bool success;

  file_name = strtok_r (file_name, " ", &save_ptr);		// IMTC

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp, &save_ptr);

  t = thread_current ();		// IMTC

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success)
  {
    sema_up (&t->pcb->exec); 
    thread_exit ();
  }

  t->pcb->is_load = true;		// IMTC
  sema_up (&t->pcb->exec);		// IMTC

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
//printf ("HERE IS WAIT\n");
  struct PCB *pcb_ = find_child_PCB (child_tid);	// IMTC
  struct thread *t = thread_current ();			// IMTC
  int status_;						// IMTC

  if (pcb_ == NULL)					// IMTC
    return ERROR;					// IMTC

  t->pcb->is_exec = false;				// IMTC
  pcb_->is_exec = true;					// IMTC
  sema_down (&pcb_->wait);				// IMTC
  status_ = pcb_->status;				// IMTC
  terminate_child (pcb_);				// IMTC
  t->pcb->is_exec = true;				// IMTC

  return status_;					// IMTC
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
  //enum intr_level old_level;

  if (cur->pcb->is_load == false || cur->pcb->is_exec == true)
    cur->pcb->status = ERROR;		// IMTC

  cur->pcb->is_exit = true;		// IMTC

  terminate_mmap_list (cur->pcb);	// IMTC
  free_page_table (&cur->pcb->page_table);	// IMTC

  if (cur->pcb->parent != NULL)		// IMTC
  {
    /*old_level = intr_disable ();

    if (!list_empty (&cur->pcb->wait.waiters))
    {
      printf ("THREAD %d SEMA LIST IS NOT EMPTY\n", cur->tid);
      intr_set_level (old_level);
      sema_up (&cur->pcb->wait);		// IMTC
    }
    else
    {
      printf ("THREAD %d SEMA LIST IS EMPTY\n", cur->tid);
      intr_set_level (old_level);
    }*/
    sema_up (&cur->pcb->wait);		// IMTC
  }
  else					// IMTC
  {
    terminate_descriptor (cur->pcb);	// IMTC
    lock_acquire (&file_lock);		// IMTC
    file_close (cur->pcb->exec_file);	// IMTC
    lock_release (&file_lock);		// IMTC
    free (cur->pcb);			// IMTC
  }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, const char *file_name, char **save_ptr);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp, char **save_ptr) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  lock_acquire (&file_lock);	// IMTC
  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  t->pcb->exec_file = file;	// IMTC

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, file_name, save_ptr))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  //file_close (file);
  lock_release (&file_lock);	// IMTC
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
/*
      // Get a page of memory.
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      // Load this page.
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      // Add the page to the process's address space.
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }
*/
      if (writable)	// IMTC
	set_page_table_entry ((void *) upage, SEG_DATA, file, ofs, page_read_bytes);	// IMTC
      else		// IMTC
	set_page_table_entry ((void *) upage, SEG_CODE, file, ofs, page_read_bytes);	// IMTC

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      ofs += page_read_bytes;	// IMTC
    }
  return true;
}

// IMTF
int
argument_length (char **save_ptr, int *argc)
{
  int length = 0, ptr_offset = 0;
  bool flag_argc = false;

  if (save_ptr == NULL)
    return length;

  while (true)
  {
    if (*(*save_ptr + ptr_offset) == '\0')
	return length;

    if (*(*save_ptr + ptr_offset) != ' ')
    {
	length++;

	if (flag_argc == false)
	{
	  flag_argc = true;
	  length++;
	  *argc += 1;
	}
    }
    else
	flag_argc = false;

    ptr_offset++;
  }
}

// IMTF
void
insert_argument_to_array (char *dst, const char *src_, char **save_ptr, int dst_offset, int src_length, int padding)
{
  const char *src = src_;
  int ptr_offset = 0;
  bool flag_argc = false;
  int i;
//printf ("%p\n", dst);
  dst += dst_offset - 1;
//printf ("%p\n", dst);

  for (i = 0; i < 4; i++)
    *dst-- = '\0';
//printf ("%p\n", dst);
//printf ("padding : %d\n", padding);
  while (padding != 0)
  {
    *dst-- = '\0';
    padding--;
  }
//printf ("%p\n", dst);

  while (src_length != 0)
  {
    *dst-- = *src++;
    src_length--;
  }
//printf ("%p\n", dst);

  *dst-- = '\0';
//printf ("%p\n", dst);

  if (save_ptr == NULL)
    return;

  while (true)
  {
    if (*(*save_ptr + ptr_offset) == '\0')
    {
	if (*(dst + 1) != '\0')
	  *dst = '\0';
	else
	  dst += 1;
//printf ("%p\n", dst);
//printf ("dst : %s\n", dst);
	return;
    }

    if (*(*save_ptr + ptr_offset) != ' ')
    {
//printf ("char\n");
	*dst-- = *(*save_ptr + ptr_offset);
//printf ("%p\n", dst);

	if (flag_argc == false)
	  flag_argc = true;
    }
    else
    {
//printf ("not char\n");
	if (flag_argc == true)
	  *dst-- = '\0';

	flag_argc = false;
    }

    ptr_offset++;
  }
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, const char *file_name, char **save_ptr) 
{
  uint8_t *kpage;
  bool success = false;
  int argc = 1;		// IMTC
  char **argv;		// IMTC
  char *temp_argv_addr;	// IMTC
  char *str_argv;	// IMTC
  int length, fnlength, aglength;	// IMTC
  int padding, i, addr;		// IMTC

  if (!set_page_table_entry (((uint8_t *) PHYS_BASE) - PGSIZE, SEG_STACK, NULL, 0, 0))
    return success;

  kpage = set_frame (page_lookup (((uint8_t *) PHYS_BASE) - PGSIZE), 1);
  //kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
      {
	free_frame (kpage);
        //palloc_free_page (kpage);
	return success;
      }
    }
//printf ("esp : %p (FIRST VALUE)\n", *esp);
//printf ("file name in setup_stack: %s\n", file_name);
//printf ("save_ptr in setup_stack: %s\n", *save_ptr);
  aglength = argument_length (save_ptr, &argc);		// IMTC
//printf ("aglength : %d\n", aglength);
  fnlength = strlen (file_name);			// IMTC
//printf ("fnlength : %d\n", fnlength);
  length = aglength + fnlength + 5;			// IMTC
  padding = length % 4;					// IMTC

  if (padding != 0)					// IMTC
  {
    padding = 4 - padding;				// IMTC
    length = padding + length;				// IMTC
  }

  str_argv = (char *) malloc (sizeof (char *) * length);	// IMTC
  temp_argv_addr = str_argv;					// IMTC
  argv = (char **) malloc (sizeof (char *) * (argc + 1));	// IMTC

  insert_argument_to_array (str_argv, file_name, save_ptr, length, fnlength, padding);	// IMTC

  *esp -= 1;
  memcpy (*esp, str_argv++, 1);
  //*(unsigned int *) *esp = '\0';	// IMTC
//printf ("value of esp : %c\n", (char) *(unsigned int *)*esp);
//printf ("esp : %p (result of *esp -= 1 from FIRST VALUE)\n", *esp);

  addr = argc;		// IMTC
  for (i = 1; i < length; i++)
  {
    *esp -= 1;
    memcpy (*esp, str_argv++, 1);
//printf ("value of esp : %c\n", (char) *(unsigned int *)*esp);

    if (*str_argv == '\0' && addr != 0)
    {
	argv[--addr] = *esp;
    }
  }
//printf ("esp : %p (SECOND VALUE)\n", *esp);
  addr = argc;		// IMTC
  for (i = 0; i < argc; i++)
  {
    *esp -= sizeof (char *);
    memcpy (*esp, &argv[--addr], sizeof (char *));
//printf ("esp : %p (result of *esp -= sizeof (char *) from above value)\n", *esp);
  }

  argv[argc] = *esp;
  *esp -= sizeof (char *);
  memcpy (*esp, &argv[argc], sizeof (char *));
//printf ("esp : %p (equal calc of above)\n", *esp);

  *esp -= sizeof (int);
  memcpy (*esp, &argc, sizeof (int));
//printf ("esp : %p (result of *esp -= sizeof (int) from above result)\n", *esp);

  argc = 0;
  *esp -= sizeof (void *);
  memcpy (*esp, &argc, sizeof (void *));

  str_argv = temp_argv_addr;
  free (str_argv);
  free (argv);		// IMTC

  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

// IMTF
bool
intf_install_page (void *upage, void *kpage, bool writable)
{
  return install_page (upage, kpage, writable);
}

// IMTF
struct PCB *
find_child_PCB (pid_t pid)
{
  struct thread *t = thread_current ();
  struct PCB *temp;
  struct list_elem *e;
  enum intr_level old_level = intr_disable ();

  for (e = list_begin (&t->pcb->child_list); e != list_end (&t->pcb->child_list); e = list_next (e))
  {
    temp = list_entry (e, struct PCB, elem);

    if (temp->pid == pid)
    {
//printf ("thread %d find child thread %d\n", t->tid, pid);
	intr_set_level (old_level);
	return temp;
    }
  }

//printf ("thread %d can't find child\n", t->tid);
  intr_set_level (old_level);
  return NULL;
}

// IMTF
void
free_mmap_pte_list (struct thread *t, struct list *l, struct file *f)
{
  struct list_elem *e;
  struct mmappte_elem *mme;

  e = list_begin (l);

  while (e != list_end (l))
  {
    mme = list_entry (e, struct mmappte_elem, elem);
    e = list_next (e);

    if (mme->pte->is_load)
    {
	if (pagedir_is_dirty (t->pagedir, mme->pte->addr))
	{
	  lock_acquire (&file_lock);
	  file_write_at (f, mme->pte->addr, mme->pte->read_bytes, mme->pte->file_offset);
	  lock_release (&file_lock);
	}

	free_frame (pagedir_get_page (t->pagedir, mme->pte->addr));
	pagedir_clear_page (t->pagedir, mme->pte->addr);
    }

    hash_delete (&t->pcb->page_table, &mme->pte->elem);
    free (mme->pte);
    list_remove (&mme->elem);
    free (mme);
  }
}

// IMTF
void
free_mmap_list (mapid_t mapid)
{
  struct thread *t = thread_current ();
  struct list_elem *e;
  struct map_elem *me;
  struct list *mmap_ptelist = NULL;

  for (e = list_begin (&t->pcb->maplist); e != list_end (&t->pcb->maplist); e = list_next (e))
  {
    me = list_entry (e, struct map_elem, elem);

    if (me->mapid == mapid)
    {
	mmap_ptelist = &me->ptelist;
	break;
    }
  }

  if (mmap_ptelist == NULL)
    return;

  free_mmap_pte_list (t, mmap_ptelist, me->f);

  lock_acquire (&file_lock);
  file_close (me->f);
  lock_release (&file_lock);

  list_remove (&me->elem);
  free (me);
}

// IMTF
void
terminate_mmap_list (struct PCB *pcb)
{
  mapid_t i;

  for (i = 0; i < pcb->mapid; i++)
    free_mmap_list (i + 1);
}

// IMTF
void
terminate_descriptor (struct PCB *pcb)
{
  struct descriptor_elem *temp;
  struct list_elem *e;
  size_t descriptor_cnt;
  enum intr_level old_level;

  if (!list_empty (&pcb->descriptor))
  {
    old_level = intr_disable ();
    descriptor_cnt = list_size (&pcb->descriptor);
    e = list_begin (&pcb->descriptor);

    while (descriptor_cnt != 0)
    {
	temp = list_entry (e, struct descriptor_elem, elem);
	e = list_next (e);
	file_close (temp->f);
	list_remove (&temp->elem);
	free (temp);
	descriptor_cnt--;
    }

    intr_set_level (old_level);
  }
}

// IMTF
void
terminate_child (struct PCB *pcb)
{
  enum intr_level old_level = intr_disable ();

  list_remove (&pcb->elem);
  intr_set_level (old_level);

  terminate_mmap_list (pcb);
  terminate_descriptor (pcb);
  lock_acquire (&file_lock);
  file_close (pcb->exec_file);
  lock_release (&file_lock);
  free (pcb);
}

