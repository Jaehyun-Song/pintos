#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void check_vaddr (const void *);	// IMTC
void intf_exit (int status);		// IMTC

#endif /* userprog/syscall.h */
