#include <gdt.h>
#include <mem.h>
#include <string.h>

/* Our GDT table will be statically stored. */
#define GDT_MAX_ENTRIES           32  /* This is more than enough for us. */


#define GDT_KERNEL_CODE_OFF       ( GDT_KERNEL_CODE_SEGMENT / \
                                    sizeof(gdt_descriptor_t) )
#define GDT_KERNEL_DATA_OFF       ( GDT_KERNEL_DATA_SEGMENT / \
                                    sizeof(gdt_descriptor_t) )
#define GDT_TSS_OFF               ( GDT_TSS / sizeof(gdt_descriptor_t) )

/* This will be provided by gdt.asm. */
extern void gdt_load_gdtr(void *);
extern void gdt_load_ltr(u16);

/* TSS. We won't use IA-32 tasks as they are supposed to, which is having a
 * task per runnable entity. However, we must have at least one TSS for the
 * only task we'll have. We need it because when interrupts or traps happens
 * when running code in PL 3 the architecture will use the current task's
 * state segment (TSS) to do the stack switch. There is no other way around
 * this so we'll have to deal with it. So, for the architecture, there will
 * be only ONE task and everything will run in its context. */
typedef struct {
  u16 prev_tss;
  u16 reserved_0;
  u32 esp0;
  u16 ss0;
  u16 reserved_1;
  u32 esp1;
  u16 ss1;
  u16 reserved_2;
  u32 esp2;
  u16 ss2;
  u16 reserved_3;
  u32 cr3;
  u32 eip;
  u32 eflags;
  u32 eax;
  u32 ecx;
  u32 edx;
  u32 ebx;
  u32 esp;
  u32 ebp;
  u32 esi;
  u32 edi;
  u16 es;
  u16 reserved_4;
  u16 cs;
  u16 reserved_5;
  u16 ss;
  u16 reserved_6;
  u16 ds;
  u16 reserved_7;
  u16 fs;
  u16 reserved_8;
  u16 gs;
  u16 reserved_9;
  u16 ldtr;
  u16 reserved_a;
  u16 trap; /* Though here only the least significant bit has meaning. */
  u16 iomap;
} __attribute__((__packed__)) gdt_tss_t;


/*****************************************************************************
 * Our vars                                                                  *
 *****************************************************************************/
static gdt_descriptor_t gdt[GDT_MAX_ENTRIES];
static gdt_tss_t gdt_tss;

/*****************************************************************************
 * Our funcs                                                                 *
 *****************************************************************************/

/* Builds a GDT descriptor. */
static gdt_descriptor_t gdt_descriptor(void * base, u32 limit,
                                       gdt_descriptor_t flags) {
  gdt_descriptor_t tmp;

  /* The limit is split in two parts. */
  tmp = limit;
  tmp &= 0x00000000ffffffff;

  flags |= tmp & 0x000000000000ffff;
  flags |= ( tmp & 0x00000000000f0000 ) << 32;

  tmp = (gdt_descriptor_t)base;
  tmp &= 0x00000000ffffffff;

  /* The base is split in three. */
  flags |= ( tmp & 0x000000000000ffff ) << 16;
  flags |= ( tmp & 0x0000000000ff0000 ) << 16;
  flags |= ( tmp & 0x00000000ff000000 ) << 32;

  return flags;
}

void * gdt_base(gdt_descriptor_t d) {
  u32 base;

  base = (d >> 32) & 0xf0000000;
  base |= (d >> 16) & 0x0fffffff;

  return (void *)base;
}

u32 gdt_limit(gdt_descriptor_t d) {
  u32 limit;

  limit = (d >> 32) & 0x000f0000;
  limit |= d & 0x0000ffff;

  return limit;
}

/* Just initializes the GDT. Only called from mem_setup() in a very early stage
 * during the kernel load process. */
void gdt_setup(u32 mem_total_frames) {
  int i;

  struct {
    u16 limit;
    gdt_descriptor_t *base_address;
  } __attribute__((__packed__)) gdt_table_descriptor;


  for (i = 0; i < GDT_MAX_ENTRIES; gdt[i ++] = GDT_NULL_ENTRY);

  /* Set kernel code segment. */
  gdt[GDT_KERNEL_CODE_OFF] = gdt_descriptor((void *)0x00000000,
                                            mem_total_frames,
                                            GDT_GRANULARITY_4K           |
                                            GDT_OP_SIZE_32               |
                                            GDT_PRESENT                  |
                                            GDT_DPL_KERNEL               |
                                            GDT_DESC_TYPE_CODE_DATA      |
                                            GDT_CODE_SEGMENT             |
                                            GDT_CODE_EXEC_READ           |
                                            GDT_CODE_NON_CONFORMING);

  /* Set kernel data segment. */
  gdt[GDT_KERNEL_DATA_OFF] = gdt_descriptor((void *)0x00000000,
                                            mem_total_frames,
                                            GDT_GRANULARITY_4K           |
                                            GDT_OP_SIZE_32               |
                                            GDT_PRESENT                  |
                                            GDT_DPL_KERNEL               |
                                            GDT_DESC_TYPE_CODE_DATA      |
                                            GDT_DATA_SEGMENT             |
                                            GDT_DATA_READ_WRITE          |
                                            GDT_DATA_EXPAND_UP);

  /* We will only have a single TSS and only a couple of fields should be used
   * by the architecture in certain situations. Basically we need it to handle
   * stack switches when interrupts happen in user mode. Probably I'm wrong
   * but I think we have no need of keeping a separated kernel stack for each
   * process if we carefully design the system calls and interrupts handlers
   * in a way that a task switch never occur when a system call is being
   * executed unless the system call itself explicitly calls for it, in which
   * case it should have prepare everything so when the process is scheduled
   * to run it needs the former stack no more. With that thought in mind, I'm
   * setting a hardcoded kernel stack pointer here, which means everytime we
   * come from userspace the stack will end up at the same location and thus
   * will replace the previous contents it held. */
  memset(&gdt_tss, 0, sizeof(gdt_tss_t));
  /* We're setting the read-only fields we need, only those. */
  gdt_tss.ss0 = GDT_SEGMENT_SELECTOR(GDT_KERNEL_DATA_SEGMENT, GDT_RPL_KERNEL);
  gdt_tss.esp0 = MEM_KERNEL_ISTACK_TOP;
  gdt_tss.iomap = sizeof(gdt_tss_t);
  gdt[GDT_TSS_OFF] = gdt_descriptor(&gdt_tss,
                                    sizeof(gdt_tss_t),
                                    GDT_GRANULARITY_1B           |
                                    GDT_PRESENT                  |
                                    GDT_DPL_KERNEL               |
                                    GDT_DESC_TYPE_SYSTEM         |
                                    GDT_SYSTEM_TSS_32);

  /* Make the architecture use the new GDT. */
  gdt_table_descriptor.limit = GDT_MAX_ENTRIES * sizeof(gdt_descriptor_t) - 1;
  gdt_table_descriptor.base_address = gdt;

  /* Load the GDTR. */
  gdt_load_gdtr(&gdt_table_descriptor);
  /* Load the LTR. */
  gdt_load_ltr(GDT_SEGMENT_SELECTOR(GDT_TSS, GDT_RPL_KERNEL));
}

u16 gdt_alloc(void * base, u32 limit, u64 flags) {
  int i;

  for (i = 1; i < GDT_MAX_ENTRIES && gdt[i] != GDT_NULL_ENTRY; i ++);
  if (i == GDT_MAX_ENTRIES)
    return 0;

  gdt[i] = gdt_descriptor(base, limit, flags);

  return (u16)(i + sizeof(gdt_descriptor_t));
}

void gdt_dealloc(u16 idx) {
  int i;

  i = idx / sizeof(gdt_descriptor_t);
  gdt[i] = GDT_NULL_ENTRY;
}

gdt_descriptor_t gdt_get(u16 idx) {
  int i;

  i = idx / sizeof(gdt_descriptor_t);
  return gdt[i];
}
