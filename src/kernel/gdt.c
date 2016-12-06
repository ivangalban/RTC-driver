#include <gdt.h>
#include <mem.h>
#include <string.h>

/* Our GDT table will be statically stored. */
#define GDT_MAX_ENTRIES           32  /* This is more than enough for us. */

/* These are all our entries. */
static gdt_descriptor_t gdt[GDT_MAX_ENTRIES];
static gdt_tss_t gdt_tss;

#define GDT_KERNEL_CODE_OFF       ( GDT_KERNEL_CODE_SEGMENT / \
                                    sizeof(gdt_descriptor_t) )
#define GDT_KERNEL_DATA_OFF       ( GDT_KERNEL_DATA_SEGMENT / \
                                    sizeof(gdt_descriptor_t) )
#define GDT_TSS_OFF               ( GDT_TSS / sizeof(gdt_descriptor_t) )

#define GDT_NULL_ENTRY            0x0000000000000000

/* This will be provided by gdt.asm. */
extern void gdt_load_gdtr(void *);
extern void gdt_load_ltr(u16);

/* Builds a GDT descriptor. */
gdt_descriptor_t gdt_descriptor(u32 base, u32 limit, gdt_descriptor_t flags) {
  gdt_descriptor_t tmp;

  /* The limit is split in two parts. */
  tmp = limit;
  tmp &= 0x00000000ffffffff;

  flags |= tmp & 0x000000000000ffff;
  flags |= ( tmp & 0x0000000000070000 ) << 32;

  tmp = base;
  tmp &= 0x00000000ffffffff;

  /* The base is split in three. */
  flags |= ( tmp & 0x000000000000ffff ) << 16;
  flags |= ( tmp & 0x0000000000ff0000 ) << 16;
  flags |= ( tmp & 0x00000000ff000000 ) << 32;

  return flags;
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
  gdt[GDT_KERNEL_CODE_OFF] = gdt_descriptor(0x00000000,
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
  gdt[GDT_KERNEL_DATA_OFF] = gdt_descriptor(0x00000000,
                                            mem_total_frames,
                                            GDT_GRANULARITY_4K           |
                                            GDT_OP_SIZE_32               |
                                            GDT_PRESENT                  |
                                            GDT_DPL_KERNEL               |
                                            GDT_DESC_TYPE_CODE_DATA      |
                                            GDT_DATA_SEGMENT             |
                                            GDT_DATA_READ_WRITE          |
                                            GDT_DATA_EXPAND_UP);

  /* We won't need the TSS until we create a user space process. Once that
   * happens we'll set the right values into it. However, let's register it
   * and move it to the LTR. */
  memset(&gdt_tss, 0, sizeof(gdt_tss_t));
  gdt[GDT_TSS_OFF] = gdt_descriptor((u32)(&gdt_tss),
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
