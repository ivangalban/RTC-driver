#include <proc.h>
#include <string.h>
#include <gdt.h>
#include <mem.h>

#define PROC_MAX_PROC   10

/* We only handle a.out omagic format. */
typedef struct {
  int a_magic;
  int a_text;
  int a_data;
  int a_bss;
  int a_syms;
  int a_entry;
  int a_trsize;
  int a_drsize;
} a_out_header;

/* My processes. */
static proc_t procs[PROC_MAX_PROC];

/* Pointer to current process. */
proc_t * proc_cur;

/* Starts the process manager. */
int proc_init() {
  memset(&procs, 0, PROC_MAX_PROC * sizeof(proc_t));

  /* Prepare the initial process, aka init. */
  procs[0].pid = 1;
  procs[0].ppid = 1;  /* init is his own father: the mystery of the trinity. */

  /* Sets the current user space process to the structure set. When ready,
   * the kernel will call proc_exec() to execute it. */
  proc_cur = procs;

  return 0;
}

/*****************************************************************************
 * Release                                                                   *
 *****************************************************************************/

/* Helper to release a segment. */
static void proc_release_segment(u16 s) {
  gdt_descriptor_t d;

  /* Release all segments. */
  d = gdt_get(s);
  if (d != GDT_NULL_ENTRY) {
    gdt_dealloc(s);
    mem_release_frames(gdt_base(d), gdt_limit(d));
  }
}

/* Release the resources of a process. */
static void proc_release_memory(proc_t *p) {
  proc_release_segment(p->segs.cs);
  proc_release_segment(p->segs.ds);
  proc_release_segment(p->segs.ss);
  proc_release_segment(p->segs.es);
  proc_release_segment(p->segs.fs);
  proc_release_segment(p->segs.gs);
}

/* Blank the registers associated to a process. */
static void proc_clear_regs(proc_t *p) {
  p->regs.eax = 0;
  p->regs.ebx = 0;
  p->regs.ecx = 0;
  p->regs.edx = 0;
  p->regs.edi = 0;
  p->regs.esi = 0;
  p->regs.ebp = 0;
  p->regs.esp = 0;
  p->regs.eip = 0;
  p->regs.eflags = 0;
}

/*****************************************************************************
 * Switch to user space                                                      *
 *****************************************************************************/
/* This must be implemented in assembly. */
extern void proc_switch_to_lower_privilege_level(u32 ss, u32 esp,
                                                 u32 cs, u32 eip);

void proc_switch_to_userland(proc_t *p) {
  proc_switch_to_lower_privilege_level(p->segs.ss, p->regs.esp, p->segs.cs,
                                       p->regs.eip);
}


/*****************************************************************************
 * Exec                                                                      *
 *****************************************************************************/

/* Replaces the current process with the binary located at path. */
int proc_exec(char *path) {
  vfs_file_t *f;
  a_out_header h;
  struct stat s;
  ssize_t r, br;
  void * base;
  u32 limit;
  u16 code, data;

  /* TODO: Handle execution permissions. */
  if (vfs_stat(path, &s) == -1)
    return -1;

  f = vfs_open(path, FILE_O_READ, 0);
  if (f == NULL)
    return -1;

  r = vfs_read(f, &h, sizeof(a_out_header));
  if (r == -1 || r != sizeof(a_out_header)) {
    vfs_close(f);
    return -1;
  }

  /* Compute the required space for the process. data and cs will be set to
   * the same region because it's simple and because the linking script puts
   * both segments together. This can be improved later. */
  limit = ( h.a_text + h.a_data + h.a_bss ) / MEM_FRAME_SIZE;
  if (( h.a_text + h.a_data + h.a_bss ) % MEM_FRAME_SIZE != 0)
    limit ++;

  /* Give it an extra frame for the stack. */
  limit ++;

  /* Request free memory from mem. */
  base = mem_allocate_frames(limit, MEM_USER_FIRST_FRAME, 0);
  if (base == NULL) {
    vfs_close(f);
    return -1;
  }

  /* Request two new segments from GDT. */
  code = gdt_alloc(base, limit, GDT_GRANULARITY_4K           |
                                GDT_OP_SIZE_32               |
                                GDT_PRESENT                  |
                                GDT_DPL_USER                 |
                                GDT_DESC_TYPE_CODE_DATA      |
                                GDT_CODE_SEGMENT             |
                                GDT_CODE_EXEC_READ           |
                                GDT_CODE_NON_CONFORMING);
  if (code == 0) {
    vfs_close(f);
    return -1;
  }

  data = gdt_alloc(base, limit, GDT_GRANULARITY_4K           |
                                GDT_OP_SIZE_32               |
                                GDT_PRESENT                  |
                                GDT_DPL_USER                 |
                                GDT_DESC_TYPE_CODE_DATA      |
                                GDT_DATA_SEGMENT             |
                                GDT_DATA_READ_WRITE          |
                                GDT_DATA_EXPAND_UP);
  if (data == 0) {
    vfs_close(f);
    gdt_dealloc(code);
    return -1;
  }

  /* Load the executable in memory. TODO: This must be changed when the format
   * of the executable gets updated and more standard. Right now it's a a.out
   * but with particular linking options that make including the header
   * necessary. */
  for (vfs_lseek(f, 0, SEEK_SET), br = 0;
       br < sizeof(a_out_header) + h.a_text + h.a_data;
       br += r) {
    r = vfs_read(f, base + br, sizeof(a_out_header) + h.a_text + h.a_data - br);
    if (r == -1 || r == 0) {
      vfs_close(f);
      gdt_dealloc(code);
      gdt_dealloc(data);
      return -1;
    }
  }
  /* We need the file no more. */
  vfs_close(f);

  /* zero the bss segment. */
  memset(base + br, 0, h.a_bss);

  /* Now we have all we need. We can start deallocating the old resources. */
  proc_release_memory(proc_cur);
  proc_clear_regs(proc_cur);
  /* TODO: Files are not dealt with now, but when CLOSE_ON_EXEC gets added
   *       we'll have to do something here. */

  /* And set the new ones. */
  proc_cur->segs.cs = GDT_SEGMENT_SELECTOR(code, GDT_RPL_USER);
  proc_cur->segs.ds = GDT_SEGMENT_SELECTOR(data, GDT_RPL_USER);
  proc_cur->segs.ss = proc_cur->segs.ds;
  /* TODO: What should we do about es, gs, and the others? */

  proc_cur->regs.eip = h.a_entry;
  proc_cur->regs.esp = limit * MEM_FRAME_SIZE; /* Relative to SS ;) */

  /* Do the switch. */
  proc_switch_to_userland(proc_cur);

  /* And never return, but the compiler doesn't know. */
  return 0;
}
