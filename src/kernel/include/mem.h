/* Logically speaking, we'll use a mixed memory model. We'll have two segments
 * for the kernel, one for code and one for data, covering the whole physical
 * address space (i.e. Basic Flat Model). On the other side, we'll also have
 * two more segments, starting at 3M and covering the rest of the address
 * space (TODO: Should we limit it to the dimensions of the actual available
 * RAM?) for both user space code and data.
 *
 * Physical memory will be divided in frame MEM_FRAME_SIZE large, though you
 * can expect this value to be 4K always.
 *
 *  +-----+-------------------------- 0xffffffff (4G)
 *  |     |       |    USER SPACE   |
 *  |     +-------|-----------------| 0x00300000 (3M)
 *  |     |       |  KERNEL STACK   |
 *  |     |       |  KERNEL HEAP    |
 *  |     |       |-----------------| 0x00100000 (1M)
 *  |     |       |     UNUSED      |
 *  |     |       |-----------------| 0x00001000 + sizeof(KERNEL_TEXT)
 *  |     |       |   KERNEL TEXT   |
 *  |     |       |-----------------| 0x00001000 (4K)
 *  |     |       |    RESERVED     |
 *  +-----|-------------------------- 0x00000000
 *  |     |
 *  |     +-- User code and data.
 *  |
 *  +-------- Kernel code and data.
 *
 * This means we need at least 3M of RAM to run and, for the time being, we
 * won't be able to run on systems with reserved regions in the [1M,3M] range.
 *
 * This file includes all memory-related facilities. Part of it is implemented
 * in mem.c, while the other, smaller part requires some assembly and is thus
 * coded in mem.asm.
 *
 */

#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <typedef.h>

/* Addresses and sizes of the most relevant memory sections in terms of the
 * kernel address space. */
#define MEM_FRAME_SIZE            4096
#define MEM_KERNEL_HEAP_ADDR      0x00100000  /* 1M */
#define MEM_KERNEL_HEAP_SIZE      0x00200000  /* 2M */
#define MEM_KERNEL_FIRST_FRAME    ((MEM_KERNEL_HEAP_ADDR) / (MEM_FRAME_SIZE))
#define MEM_KERNEL_STACK_TOP      ((MEM_KERNEL_HEAP_ADDR) + (MEM_KERNEL_HEAP_SIZE))
#define MEM_KERNEL_STACK_FRAME    ((MEM_KERNEL_STACK_TOP) / (MEM_FRAME_SIZE) - 1)

#define MEM_USER_SPACE_ADDR       ((MEM_KERNEL_HEAP_ADDR) + (MEM_KERNEL_HEAP_SIZE))
#define MEM_USER_FIRST_FRAME      ((MEM_USER_SPACE_ADDR) / (MEM_FRAME_SIZE))

/* Initializes memory management system. It receives two addresses: the current
 * GDT base address and the memory map obtained from the BIOS during the real
 * mode part of this kernel. This functionality is supposed to be called very
 * early in the boot up process, even before the dev and vfs subsystems are
 * initialized. The rest of the initialization will take place later in
 * mem_init(), which is in turn called after dev and vfs are set up. */
int mem_setup(void *gdt_base, void *mem_map);

/* Rellocates the memory to the address given. Be aware than after calling
 * this function the previous stack will be lost, including the caller's
 * activation registry. This is just a hack and if there were to be a better
 * way to do this I'll definitively change it. */
void mem_relocate_stack_to(void *);

/* Completes the initialization of the memorys subsystem by publishing all
 * the special files provided by this driver. */
int mem_init();

/* Requests a contiguous space of count frames starting from first_frame but
 * not reaching last_frame. If last_frame is 0, then all frames from
 * first_frame 'till the very end of available physical frames will be
 * inspected.
 * It returns the address to the first byte in the allocated space. */
void * mem_allocate_frames(u32 count, u32 first_frame, u32 last_frame);

/* Releases count frames of memory starting from address addr. If addr is not
 * a page frame aligned address then the frame containing address will also be
 * released. */
void mem_release_frames(void *addr, u32 count);

/* This is the internal logical allocator. It only reserves space inside the
 * kernel heap. */
void * kalloc(u32 bytes);

/* This is the internal logical allocator's free routine. */
void kfree(void *ptr);

/* Prints a map to the framebuffer device of how the physical pages are
 * allocated. */
void mem_inspect();

/* Prints a map to the framebuffer device of how's the logical allocation. */
void mem_inspect_alloc();

#endif
