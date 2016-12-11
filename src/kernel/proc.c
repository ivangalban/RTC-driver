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
static void proc_release_segment(gdt_selector_t s) {
  gdt_descriptor_t d;

  d = gdt_get(s);
  if (d == GDT_NULL_ENTRY)
    return;

  gdt_dealloc(s);
  mem_release_frames(gdt_base(d), gdt_limit(d));
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
extern void proc_switch_to_lower_privilege_level(u32 eip, u16 cs, u32 eflags,
                                                 u32 esp, u16 ss, u16 ds,
                                                 u16 es, u16 fs, u16 gs);

void proc_switch_to_userland(proc_t *p) {
  proc_switch_to_lower_privilege_level(p->regs.eip, p->segs.cs, p->regs.eflags,
                                       p->regs.esp, p->segs.ss, p->segs.ds,
                                       p->segs.es, p->segs.fs, p->segs.gs);
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
  char * base;
  u32 code_limit, data_limit;
  u16 code_segment, data_segment;

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

  /* Prepare memory for the process. This is not the best way, but let's use
   * it for now. This depends heavily on the linker script. I haven't found
   * out why some things occur, so I let the script with the configuration I
   * like the most. If only there was a way to tell gcc not to include a
   * .rodata section I wouldn't need a script at all - I guess.
   * At this point we're using weird combination of NMAGIC a.out file with
   * custom locations, which I don't know are standard or not - probably not.
   *  1. The .text section includes the header like in QMAGIC a.out, leaving
   *     address 0x0 useful as NULL.
   *  2. The .data section, which includes also the damned .rodata's, is
   *     aligned to 4KB boudaries. This was what took me hours to find out
   *     in order to solve the weird address assignment to symbols the linker
   *     was making while keeping the output file small.
   *  3. The .bss section comes right after the .data section, no alignment
   *     at all for it.
   * I know virtual memory would solve this gracefully, and I know we might
   * try to solve the security issues having code and data in the same segment
   * be, but I leave that to those after me. Or the future me. I need some
   * linker scripts reference.  */

  /* Compute the size of .text. */
  code_limit = (sizeof(a_out_header) + h.a_text) / MEM_FRAME_SIZE;
  if ((sizeof(a_out_header) + h.a_text) % MEM_FRAME_SIZE != 0)
    code_limit ++;

  /* Compute the size of .data + .bss. */
  data_limit = (h.a_data + h.a_bss) / MEM_FRAME_SIZE;
  if ((h.a_data + h.a_bss) % MEM_FRAME_SIZE != 0)
    data_limit ++;

  /* Add an extra frame for the stack. */
  data_limit ++;

  /* Request free memory from mem. */
  base = (char *)mem_allocate_frames(code_limit + data_limit,
                                     MEM_USER_FIRST_FRAME,
                                     0);
  if (base == NULL) {
    vfs_close(f);
    return -1;
  }

  /* Request code segment from GDT. */
  code_segment = gdt_alloc(base,
                           code_limit + data_limit,
                           GDT_GRANULARITY_4K      |
                           GDT_OP_SIZE_32          |
                           GDT_PRESENT             |
                           GDT_DPL_USER            |
                           GDT_DESC_TYPE_CODE_DATA |
                           GDT_CODE_SEGMENT        |
                           GDT_CODE_EXEC_READ      |
                           GDT_CODE_NON_CONFORMING);
  if (code_segment == 0) {
    mem_release_frames(base, code_limit + data_limit);
    vfs_close(f);
    return -1;
  }

  /* Allocate a segment for data. */
  data_segment = gdt_alloc(base,
                           code_limit + data_limit,
                           GDT_GRANULARITY_4K      |
                           GDT_OP_SIZE_32          |
                           GDT_PRESENT             |
                           GDT_DPL_USER            |
                           GDT_DESC_TYPE_CODE_DATA |
                           GDT_DATA_SEGMENT        |
                           GDT_DATA_READ_WRITE     |
                           GDT_DATA_EXPAND_UP);
  if (data_segment == 0) {
    vfs_close(f);
    gdt_dealloc(code_segment);
    mem_release_frames(base, code_limit + data_limit);
    return -1;
  }

  /* Load the .text segment into memory. */
  for (vfs_lseek(f, 0, SEEK_SET), br = 0;
       br < sizeof(a_out_header) + h.a_text;
       br += r) {
    r = vfs_read(f, base + br, sizeof(a_out_header) + h.a_text - br);
    if (r == -1 || r == 0) {
      vfs_close(f);
      gdt_dealloc(code_segment);
      gdt_dealloc(data_segment);
      mem_release_frames(base, code_limit + data_limit);
      return -1;
    }
  }

  /* Load the .data segment into memory. */
  for (br = 0;
       br < h.a_data;
       br += r) {
    r = vfs_read(f, base + code_limit * MEM_FRAME_SIZE + br, h.a_data - br);
    if (r == -1 || r == 0) {
      vfs_close(f);
      gdt_dealloc(code_segment);
      gdt_dealloc(data_segment);
      mem_release_frames(base, code_limit + data_limit);
      return -1;
    }
  }

  /* We need the file no more so we can close it. */
  vfs_close(f);

  /* zero the .bss segment which is located right after the .data segment. */
  memset(base + code_limit * MEM_FRAME_SIZE + h.a_data, 0, h.a_bss);

  /* Now we have all we need. We can start deallocating the old resources. */
  proc_release_memory(proc_cur);
  proc_clear_regs(proc_cur);
  /* TODO: Files are not dealt with now, but when CLOSE_ON_EXEC gets added
   *       we'll have to do something here. */

  /* And set the new ones. */
  proc_cur->segs.cs = GDT_SEGMENT_SELECTOR(code_segment, GDT_RPL_USER);
  proc_cur->segs.ds = GDT_SEGMENT_SELECTOR(data_segment, GDT_RPL_USER);
  proc_cur->segs.ss = proc_cur->segs.ds;
  proc_cur->segs.es = proc_cur->segs.ds;
  proc_cur->segs.gs = proc_cur->segs.ds;
  proc_cur->segs.fs = proc_cur->segs.ds;

  proc_cur->regs.eip = h.a_entry;
  proc_cur->regs.esp = (code_limit + data_limit) * MEM_FRAME_SIZE;

  /* Do the switch. */
  proc_switch_to_userland(proc_cur);

  /* And never return, but the compiler doesn't know. */
  return -1;
}
