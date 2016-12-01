#include <fb.h>
#include <hw.h>
#include <string.h>
#include <mem.h>
#include <pic.h>
#include <serial.h>
#include <kb.h>
#include <errors.h>
#include <devices.h>
#include <vfs.h>
#include <fs/rootfs.h>

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
  // serial_port_config_t sc;
  char buf[2];
  dev_char_device_t *s;
  char *msg = "You pressed ESC.\n";
  char *msg2;
  int i;
  struct stat st;
  vfs_file_t *f;

  /* Now we're here, let's set the panic level to hysterical: nothing here
   * can fail. */
  set_panic_level(PANIC_HYSTERICAL);

  /* Set up the interrupt subsytem. */
  itr_set_up();

  /* Initialize the Virtual File System. */
  vfs_init();

  /* Intializes the rootfs. */
  rootfs_init();

  /* Mount rootfs on "/" */
  vfs_mount(ROOTFS_DEVID, "/", ROOTFS_NAME);

  /* Initializes the dev subsystem. */
  dev_init();

  set_panic_level(PANIC_PERROR);
  f = vfs_open("/text.txt", FILE_O_RW | FILE_O_CREATE, 0644);
  vfs_write(f, msg, strlen(msg));
  msg2 = (char *)kalloc(strlen(msg) + 10);
  memset(msg2, '_', strlen(msg) + 10);
  vfs_lseek(f, 0, SEEK_SET);
  vfs_read(f, msg2 + 5, strlen(msg));
  fb_write(msg2, strlen(msg) + 10);

  /* Complete memory initialization now as a device and filesystem module. */
  mem_init();

  /* Initializes the PICs. This mask all interrupts. */
  pic_init();

  /* Activate the keyboard. */
  kb_init();
  pic_unmask_dev(PIC_KEYBOARD_IRQ);

  serial_init();
  pic_unmask_dev(PIC_SERIAL_1_IRQ);
  pic_unmask_dev(PIC_SERIAL_2_IRQ);

  hw_sti();

  s = dev_get_char_device(DEV_MAKE_DEV(DEV_TTY_MAJOR, 64));
  if (s == NULL)
    kernel_panic("No TTY:64\n");
  s->ops->open(s, 0);

  /* We can now turn interrupts on, they won't reach us (yet). */

  fb_printf("Idle loop.\n");

  /* This is the idle loop. */
  while (1) {
    buf[0] = 0; buf[1] = 0;
    s->ops->read(s, buf);
    if (buf[0] == 27) {
      for (i = 0; i < strlen(msg); i ++) {
        if (s->ops->write(s, msg + i) == -1)
          kernel_panic("Write returned -1\n");
      }
    }
    else {
      fb_write(buf, 1);
    }
    hw_hlt();
  }
}
