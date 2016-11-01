#include <fb.h>
#include <hw.h>
#include <string.h>
#include <mem.h>
#include <pic.h>
#include <serial.h>
#include <kb.h>
#include <errors.h>
#include <devices.h>

/* Just the declaration of the second, main kernel routine. */
void kmain2();

void kmain(void *gdt_base, void *mem_map) {
  /* This is a hack. We need to set the stack to the right place, but we need
   * to do this AFTER the memory allocator has checked and initialized the
   * memory. To do this we'll manipulate the stack in a dangerous way, but
   * since we won't return from this function its somehow safe. Also, to keep
   * things simple, this function should have no activation registry - or at
   * least that's what I think, since I can't be sure what will GCC do.
   * In any case, after relocating the stack we'll just call kmain_stage_2
   * so there should be no need for anything in the stack since the address
   * to kmain2 must be statically computed. */

  /* Luckily, framebuffer driver is basically static, except for fb_printf.
   * If we have to print anything here let it feel TERRIBLE. */
  fb_reset();

  /* Initialize the memory. We're using the stack set during the real-mode
   * initial steps. */
  if (mem_setup(gdt_base, mem_map) == -1) {
    kernel_panic("Could not initialize memory :(");
  }
  /* Our stack will be 4K long situated at the end of the kernel space, right
   * before the user space. We need to allocate this very frame. */
  if (mem_allocate_frames(1,
                          MEM_KERNEL_STACK_FRAME,
                          MEM_USER_FIRST_FRAME) == NULL) {
    kernel_panic("Could not allocate a frame for the kernel's stack :(");
  }

  /* And now comes the magic. Fingers crossed. */
  mem_relocate_stack_to((void *)MEM_KERNEL_STACK_TOP);

  /* At this point, we are standing in the new stack. We can continue but we
   * can't return here. */
  kmain2();
}

void kmain2() {
  serial_port_config_t sc;
  char buf[2];
  dev_char_device_t *zero, *null;

  /* Now we're here, let's set the panic level to hysterical: nothing here
   * can fail. */
  set_panic_level(PANIC_HYSTERICAL);

  /* Set up the interrupt subsytem. */
  itr_set_up();

  /* Initializes the dev subsystem. */
  dev_init();

  /* Complete memory initialization. */
  mem_init();

  /* Initializes the PICs. This mask all interrupts. */
  pic_init();

  /* Activate the keyboard. */
  kb_init();
  pic_unmask_dev(PIC_KEYBOARD_IRQ);

  /* We can now turn interrupts on, they won't reach us (yet). */
  hw_sti();

  zero = dev_get_char_device(DEV_MAKE_DEV(DEV_MEM_MAJOR, 5)); /* 5 is ZERO */
  zero->ops->open(zero, 0);
  zero->ops->read(zero, buf);
  zero->ops->release(zero);

  null = dev_get_char_device(DEV_MAKE_DEV(DEV_MEM_MAJOR, 3)); /* 3 is NULL */
  null->ops->open(null, 0);
  null->ops->write(null, buf);
  null->ops->release(null);

  sc.divisor = 3;  /* Baud rate = 115200 / 3 = 38400 */
  sc.available_interrupts = SERIAL_INT_DATA_AVAILABLE;    /* No interrupts */
  sc.line_config = SERIAL_CHARACTER_LENGTH_8 |  /* Standard 8N1 config */
                   SERIAL_PARITY_NONE |
                   SERIAL_SINGLE_STOP_BIT;
  if (serial_init(SERIAL_COM1, &sc) == -1) {
    kernel_panic("Could not intialize COM1 :(");
  }
  pic_unmask_dev(PIC_SERIAL_1_IRQ);

  /* This is the idle loop. */
  while (1) {
    buf[0] = 0; buf[1] = 0;
    serial_read(SERIAL_COM1, buf, 1);
    fb_write(buf, 1);
  }
}
