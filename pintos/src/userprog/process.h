#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <hash.h>			// IMTC
#include "threads/thread.h"
#include "threads/synch.h"		// IMTC
#include "vm/page.h"			// IMTC

#define ERROR -1
#define DEFAULT_STATUS 0

typedef int pid_t;	// IMTC
typedef int mapid_t;	// IMTC

// IMTS
struct mmappte_elem
  {
    struct page *pte;
    struct list_elem elem;
  };

// IMTS
struct map_elem
  {
    struct file *f;
    mapid_t mapid;
    struct list ptelist;
    struct list_elem elem;
  };

// IMTS
struct PCB
  {
    pid_t pid;
    int status;
    bool is_load;
    bool is_exec;
    bool is_exit;
    struct thread *parent;
    struct list descriptor;
    struct list child_list;
    struct list_elem elem;
    struct semaphore exec;
    struct semaphore wait;
    struct hash page_table;
    struct file *exec_file;
    mapid_t mapid;
    struct list maplist;
  };

// IMTS
struct descriptor_elem
  {
    struct file *f;
    int fd_num;
    struct list_elem elem;
  };

//struct list descriptor;		// IMTC
struct lock file_lock;		// IMTC

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
void process_semaphore_init (void);		// IMTC
void file_descriptor_init (void);		// IMTC
struct PCB *find_child_PCB (pid_t pid);		// IMTC
void free_mmap_list (mapid_t mapid);	// IMTC
bool intf_install_page (void *, void *, bool writable);	// IMTC

#endif /* userprog/process.h */
