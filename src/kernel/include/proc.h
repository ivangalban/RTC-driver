#ifndef __PROC_H__
#define __PROC_H__

#include <typedef.h>
#include <vfs.h>

#define PROC_MAX_FD     10

typedef struct proc {
  pid_t           pid;                  /* Process ID. */
  pid_t           ppid;                 /* Parent PID. */
  struct {
    u32 eax;
    u32 ebx;
    u32 ecx;
    u32 edx;
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp;
    u32 eip;
    u32 eflags;
  }               regs;                 /* Registers. */
  struct {
    u16 cs;
    u16 ss;
    u16 ds;
    u16 es;
    u16 fs;
    u16 gs;
  }               segs;                 /* Segments. */
  vfs_file_t    * fdesc[PROC_MAX_FD];   /* File descriptors. */
} proc_t;

#endif
