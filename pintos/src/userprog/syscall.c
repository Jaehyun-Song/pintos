#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"		// IMTC
#include "threads/synch.h"		// IMTC
#include "threads/malloc.h"		// IMTC
#include "userprog/process.h"		// IMTC
#include "userprog/pagedir.h"		// IMTC
#include "filesys/filesys.h"		// IMTC
#include "filesys/file.h"		// IMTC
#include "devices/shutdown.h"		// IMTC
#include "devices/input.h"		// IMTC
#include "vm/page.h"			// IMTC
#include "vm/frame.h"			// IMTC

#define USER_ADDR_MIN ((void *) 0x08048000)	// IMTC

//struct lock file_lock;			// IMTC

static void syscall_handler (struct intr_frame *);
void sys_halt (void);			// IMTC
void sys_exit (int status);		// IMTC
pid_t sys_exec (const char *);		// IMTC
int sys_wait (pid_t pid);		// IMTC
bool sys_create (const char *, unsigned initial_size);	// IMTC
bool sys_remove (const char *);				// IMTC
int sys_open (const char *file);			// IMTC
int sys_filesize (int fd);				// IMTC
int sys_read (int fd, void *, unsigned size);		// IMTC
int sys_write (int fd, const void *, unsigned size);	// IMTC
void sys_seek (int fd, unsigned position);		// IMTC
unsigned sys_tell (int fd);				// IMTC
void sys_close (int fd);				// IMTC
mapid_t sys_mmap (int fd, void *);			// IMTC
void sys_munmap (mapid_t mapid);			// IMTC
int set_file (struct file *);				// IMTC
struct file *get_file (int fd);				// IMTC
void close_file (int fd);				// IMTC
void set_mmappte_elem (struct page *, struct map_elem *);	// IMTC
struct map_elem *set_map_elem (struct thread *, struct file *, mapid_t mapid);	// IMTC
unsigned int get_page_vaddr (const void *);		// IMTC
void get_argument (struct intr_frame *, unsigned int *, int n);	// IMTC
bool is_valid_vaddr (const void *);			// IMTC
bool valid_writable_ptr (const void *);			// IMTC

void
syscall_init (void) 
{
  lock_init (&file_lock);		// IMTC
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  get_page_vaddr ((const void *) f->esp);

  switch (*(int *) f->esp)
  {
    case SYS_HALT :
    {
	sys_halt ();
	break;
    }
    case SYS_EXIT :
    {
	unsigned int argv[1];
	get_argument (f, argv, 1);
	sys_exit ((int) argv[0]);
	break;
    }
    case SYS_EXEC :
    {
	unsigned int argv[1];
	get_argument (f, argv, 1);
	f->eax = sys_exec ((const char *) argv[0]);
	break;
    }
    case SYS_WAIT :
    {
	unsigned int argv[1];
	get_argument (f, argv, 1);
	f->eax = sys_wait ((pid_t) argv[0]);
	break;
    }
    case SYS_CREATE :
    {
	unsigned int argv[2];
	get_argument (f, argv, 2);
	f->eax = sys_create ((const char *) argv[0], (unsigned) argv[1]);
	break;
    }
    case SYS_REMOVE :
    {
	unsigned int argv[1];
	get_argument (f, argv, 1);
	f->eax = sys_remove ((const char *) argv[0]);
	break;
    }
    case SYS_OPEN :
    {
	unsigned int argv[1];
	get_argument (f, argv, 1);
	f->eax = sys_open ((const char *) argv[0]);
	break;
    }
    case SYS_FILESIZE :
    {
	unsigned int argv[1];
	get_argument (f, argv, 1);
	f->eax = sys_filesize ((int) argv[0]);
	break;
    }
    case SYS_READ :
    {
	unsigned int argv[3];
	get_argument (f, argv, 3);
	f->eax = sys_read ((int) argv[0], (void *) argv[1], (unsigned) argv[2]);
	break;
    }
    case SYS_WRITE :
    {
	unsigned int argv[3];
	get_argument (f, argv, 3);
	f->eax = sys_write ((int) argv[0], (const void *) argv[1], (unsigned) argv[2]);
	break;
    }
    case SYS_SEEK :
    {
	unsigned int argv[2];
	get_argument (f, argv, 2);
	sys_seek ((int) argv[0], (unsigned) argv[1]);
	break;
    }
    case SYS_TELL :
    {
	unsigned int argv[1];
	get_argument (f, argv, 1);
	f->eax = sys_tell ((int) argv[0]);
	break;
    }
    case SYS_CLOSE :
    {
	unsigned int argv[1];
	get_argument (f, argv, 1);
	sys_close ((int) argv[0]);
	break;
    }
    case SYS_MMAP :
    {
	unsigned int argv[2];
	get_argument (f, argv, 2);
	f->eax = sys_mmap ((int) argv[0], (void *) argv[1]);
	break;
    }
    case SYS_MUNMAP :
    {
	unsigned int argv[1];
	get_argument (f, argv, 1);
	sys_munmap ((mapid_t) argv[0]);
	break;
    }
    case SYS_CHDIR :
    {
	printf ("not defined CHDIR system call\n");
	thread_exit ();

    }
    case SYS_MKDIR :
    {
	printf ("not defined MKDIR system call\n");
	thread_exit ();

    }
    case SYS_READDIR :
    {
	printf ("not defined READDIR system call\n");
	thread_exit ();

    }
    case SYS_ISDIR :
    {
	printf ("not defined ISDIR system call\n");
	thread_exit ();

    }
    case SYS_INUMBER :
    {
	printf ("not defined INUMBER system call\n");
	thread_exit ();

    }
    default :
    {
	printf ("NOT DEFINED STSTEM CALL!!\n");
	thread_exit ();
    }
  }
}

// IMTF
void
sys_halt (void)
{
  shutdown_power_off ();
}

// IMTF
void
sys_exit (int status)
{
  struct thread *t = thread_current ();

  //struct lock temp;
  //lock_init (&temp);
  //lock_acquire (&temp);
  //printf ("list num %d of thread %d\n", list_size (&t->pcb->wait.waiters), t->tid);
  //lock_release (&temp);
//printf ("EXIT\n");
//print_all_list ();

  t->pcb->status = status;
  t->pcb->is_exec = false;
  printf ("%s: exit(%d)\n", t->name, status);
  thread_exit ();
}

// IMTF
pid_t
sys_exec (const char *cmd_line)
{
  get_page_vaddr ((const void *) cmd_line);

  return process_execute (cmd_line);
}

// IMTF
int
sys_wait (pid_t pid)
{
  return process_wait (pid);
}

// IMTF
bool
sys_create (const char *file, unsigned initial_size)
{
  bool success;

  get_page_vaddr ((const void *) file);

  lock_acquire (&file_lock);
  success = filesys_create (file, initial_size);
  lock_release (&file_lock);

  return success;
}

// IMTF
bool
sys_remove (const char *file)
{
  bool success;

  get_page_vaddr ((const void *) file);

  lock_acquire (&file_lock);
  success = filesys_remove (file);
  lock_release (&file_lock);

  return success;
}

// IMTF
int
sys_open (const char *file)
{
  struct file *f;
  int fd;

  get_page_vaddr ((const void *) file);

  lock_acquire (&file_lock);
  f = filesys_open (file);

  if (f == NULL)
  {
    lock_release (&file_lock);
    return ERROR;
  }

  fd = set_file (f);

  if (check_executable_file (file))
    file_deny_write (f);

  lock_release (&file_lock);

  return fd;
}

// IMTF
int
sys_filesize (int fd)
{
  struct file *f;
  int length;

  lock_acquire (&file_lock);
  f = get_file (fd);

  if (f == NULL)
  {
    lock_release (&file_lock);
    return ERROR;
  }

  length = file_length (f);
  lock_release (&file_lock);

  return length;
}

// IMTF
int
sys_read (int fd, void *buffer, unsigned size)
{
  struct file *f;
  uint8_t *temp;
  int _bytes;
  unsigned i;

  get_page_vaddr (buffer);

  if (!valid_writable_ptr (buffer))
    sys_exit (ERROR);

  if (fd == STDIN_FILENO)
  {
    temp = (uint8_t *) buffer;

    for (i = 0; i < size; i++)
	temp[i] = input_getc ();

    return size; 
  }
  else if (fd == STDOUT_FILENO || fd == STDERR_FILENO)
  {
    return ERROR;
  }
  else
  {
    lock_acquire (&file_lock);
    f = get_file (fd);

    if (f == NULL)
    {
	lock_release (&file_lock);
	return ERROR;
    }

    _bytes = file_read (f, buffer, size);
    lock_release (&file_lock);

    return _bytes;
  }
}

// IMTF
int
sys_write (int fd, const void *buffer, unsigned size)
{
  struct file *f;
  int _bytes;

  get_page_vaddr (buffer);

  if (fd == STDOUT_FILENO)
  {
    putbuf (buffer, size);
    return size;
  }
  else if (fd == STDIN_FILENO || fd == STDERR_FILENO)
  {
    return ERROR;
  }
  else
  {
    lock_acquire (&file_lock);
    f = get_file (fd);

    if (f == NULL)
    {
	lock_release (&file_lock);
	return ERROR;
    }

    _bytes = file_write (f, buffer, size);
    lock_release (&file_lock);

    return _bytes;
  }
}

// IMTF
void
sys_seek (int fd, unsigned position)
{
  struct file *f;

  lock_acquire (&file_lock);
  f = get_file (fd);

  if (f == NULL)
  {
    lock_release (&file_lock);
    return;
  }

  file_seek (f, position);
  lock_release (&file_lock);
}

// IMTF
unsigned
sys_tell (int fd)
{
  struct file *f;
  off_t position;

  lock_acquire (&file_lock);
  f = get_file (fd);

  if (f == NULL)
  {
    lock_release (&file_lock);
    return ERROR;
  }

  position = file_tell (f);
  lock_release (&file_lock);

  return position;
}

// IMTF
void
sys_close (int fd)
{
  lock_acquire (&file_lock);
  close_file (fd);
  lock_release (&file_lock);
}

// IMTF
mapid_t
sys_mmap (int fd, void *addr)
{
  struct thread *t;
  struct map_elem *me;
  struct file *temp;
  struct file *file;
  size_t read_bytes;
  off_t ofs = 0;

  if (!is_valid_vaddr (addr) || (uint32_t) addr % PGSIZE != 0)
    return ERROR;

  temp = get_file (fd);

  if (temp == NULL)
    return ERROR;

  file = file_reopen (temp);

  if (file == NULL || file_length (temp) == 0)
    return  ERROR;

  read_bytes = file_length (file);
  t = thread_current ();
  t->pcb->mapid++;

  me = set_map_elem (t, file, t->pcb->mapid);

  while (read_bytes > 0)
  {
    uint32_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;

    if (!set_page_table_entry (addr, SEG_MMAP, file, ofs, page_read_bytes))
    {
	free_mmap_list (t->pcb->mapid);
	return ERROR;
    }
    set_mmappte_elem (page_lookup (addr), me);

    addr += PGSIZE;
    read_bytes -= page_read_bytes;
    ofs += page_read_bytes;
  }

  return t->pcb->mapid;
}

// IMTF
void
sys_munmap (mapid_t mapid)
{
  free_mmap_list (mapid);
}

// IMTF
int
set_file (struct file *f)
{
  struct thread *t = thread_current ();
  struct descriptor_elem *e = (struct descriptor_elem *) malloc (sizeof (struct descriptor_elem));
  struct descriptor_elem *dpt;
  struct list_elem *temp;
  enum intr_level old_level;
  int before_fd = 2;
  
  e->f = f;

  if (list_empty (&t->pcb->descriptor))
  {
    e->fd_num = 3;
    old_level = intr_disable ();
    list_push_back (&t->pcb->descriptor, &e->elem);
    intr_set_level (old_level);
    return 3;
  }
  else
  {
    for (temp = list_begin (&t->pcb->descriptor); temp != list_end (&t->pcb->descriptor); temp = list_next (temp))
    {
	dpt = list_entry (temp, struct descriptor_elem, elem);

	if (dpt->fd_num - before_fd > 1)
	{
	  e->fd_num = before_fd + 1;
	  old_level = intr_disable ();
	  list_insert (&dpt->elem, &e->elem);
	  intr_set_level (old_level);

	  return e->fd_num;
	}
	else
	  before_fd = dpt->fd_num;
    }
  }

  e->fd_num = before_fd + 1;
  old_level = intr_disable ();
  list_push_back (&t->pcb->descriptor, &e->elem);
  intr_set_level (old_level);

  return e->fd_num;
}

// IMTF
struct file *
get_file (int fd)
{
  struct thread *t = thread_current ();
  struct descriptor_elem *temp;
  struct list_elem *e;

  for (e = list_begin (&t->pcb->descriptor); e != list_end (&t->pcb->descriptor); e = list_next (e))
  {
    temp = list_entry (e, struct descriptor_elem, elem);

    if (fd < temp->fd_num)
	return NULL;

    if (fd == temp->fd_num)
	return temp->f;
  }

  return NULL;
}

// IMTF
void
close_file (int fd)
{
  struct thread *t = thread_current ();
  struct descriptor_elem *temp;
  struct list_elem *e;
  enum intr_level old_level;

  for (e = list_begin (&t->pcb->descriptor); e != list_end (&t->pcb->descriptor); e = list_next (e))
  {
    temp = list_entry (e, struct descriptor_elem, elem);

    if (fd < temp->fd_num)
	return;

    if (fd == temp->fd_num)
    {
	file_close (temp->f);
	old_level = intr_disable ();
	list_remove (&temp->elem);
	intr_set_level (old_level);
	free (temp);

	return;
    }
  }
}

// IMTF
void
set_mmappte_elem (struct page *pte, struct map_elem *me)
{
  struct mmappte_elem *e = (struct mmappte_elem *) malloc (sizeof (struct mmappte_elem));

  e->pte = pte;
  list_push_back (&me->ptelist, &e->elem);
}

// IMTF
struct map_elem *
set_map_elem (struct thread *t, struct file *file, mapid_t mapid)
{
  struct map_elem *e = (struct map_elem *) malloc (sizeof (struct map_elem));

  e->f = file;
  e->mapid = mapid;
  list_init (&e->ptelist);
  list_push_back (&t->pcb->maplist, &e->elem);

  return e;
}

// IMTF
unsigned int
get_page_vaddr (const void *vaddr)
{
  void *kaddr;
  struct page *p;
  bool is_load = false;

  check_vaddr (vaddr);
  kaddr = pagedir_get_page (thread_current ()->pagedir, vaddr);

  if (kaddr == NULL)
  {
    p = page_lookup ((void *) vaddr);

    if (p == NULL)
      sys_exit (ERROR);
    else
    {
	if (p->type == SEG_STACK)
	  is_load = stack_growth (p->addr);
	else
	  is_load = lazy_loading (p);

	if (is_load)
	  kaddr = pagedir_get_page (thread_current ()->pagedir, vaddr);
    }
  }

  return (unsigned int) kaddr;
}

// IMTF
void
get_argument (struct intr_frame *f, unsigned int *argv, int n)
{
  int i = 0;
  unsigned int *temp;

  while (i != n)
  {
    temp = (unsigned int *) (f->esp + ((i + 1) * 4));
    check_vaddr ((const void *) temp);
    argv[i] = *temp;
    i++;
  }
}

// IMTF
bool
is_valid_vaddr (const void *vaddr)
{
  if (!is_user_vaddr (vaddr) || vaddr < USER_ADDR_MIN)
    return false;

  return true;
}
// IMTF
void
check_vaddr (const void *vaddr)
{
  if (!is_valid_vaddr (vaddr))
    sys_exit (ERROR);
}

// IMTF
bool
valid_writable_ptr (const void *vaddr)
{
  struct page *p = page_lookup ((void *) vaddr);

  if (p->type == SEG_CODE)
    return false;
  else
    return true;  
}

// IMTF
void
intf_exit (int status)
{
  sys_exit (status);
}
