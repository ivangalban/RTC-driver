#include <typedef.h>
#include <io.h>
#include <string.h>
#include <serial.h>
#include <interrupts.h>
#include <mem.h>
#include <hw.h>
#include <pic.h>
#include <devices.h>
#include <errors.h>
#include <fb.h>
#include <lock.h>
#include <vfs.h>

/* We'll manage all four ISA serial ports. */
#define SERIAL_TOTAL_DEVICES     4
/* Internal buffers will be 64 bytes long. */
#define SERIAL_BUFFER_LEN       64

/* Base ports of the four serial devices. */
#define SERIAL_COM1_BASE        0x03f8
#define SERIAL_COM2_BASE        0x02f8
#define SERIAL_COM3_BASE        0x03e8
#define SERIAL_COM4_BASE        0x02e8

/* IRQs used by the devices after PIC remapping. */
#define SERIAL_COM1_IRQ         PIC_SERIAL_1_IRQ
#define SERIAL_COM2_IRQ         PIC_SERIAL_2_IRQ
#define SERIAL_COM3_IRQ         PIC_SERIAL_1_IRQ  /* Same as COM1 */
#define SERIAL_COM4_IRQ         PIC_SERIAL_2_IRQ  /* Same as COM2 */

/* Minors used by Linux for /dev/ttySN */
#define SERIAL_COM1_MINOR       64
#define SERIAL_COM2_MINOR       65
#define SERIAL_COM3_MINOR       66
#define SERIAL_COM4_MINOR       67

/* Types of UART chipsets we handle. */
#define SERIAL_TYPE_UNKNOWN     0
#define SERIAL_TYPE_8250        1
#define SERIAL_TYPE_16450       2
#define SERIAL_TYPE_16550       3
#define SERIAL_TYPE_16550A      4
#define SERIAL_TYPE_16750       5

/* Each device has its own reading buffer. */
typedef struct serial_buffer {
  u32 write_head;
  u32 read_head;
  char buffer[SERIAL_BUFFER_LEN];
} serial_buffer_t;

/* These are the flags used internally to maintain device status */
#define SERIAL_FLAG_UNUSED      0x00000000
#define SERIAL_FLAG_IN_USE      0x00000001

/* This internal structure will hold a serial device configuration. */
typedef struct serial_device {
  dev_t         devid;          /* MAJOR|MINOR */
  io_port_t     base;           /* base port */
  itr_irq_t     irq;            /* IRQ assigned to this device. */
  u8            type;           /* Type of device. Unknown = not present. */
  char        * name;           /* device name in /dev. */
  u32           flags;          /* Internal flags. */
  struct {
    u16 divisor;                /* Baud rate divisor. */
    u8 interrupts;              /* Interrupts. */
    u8 line_ctl;                /* Line configuration. */
    u8 fifo_ctl;                /* FIFO configuration. */
    u8 modem_ctl;               /* MODEM configuration. NOT USED. */
  } config;
  serial_buffer_t read_buf;     /* reading buffer. */
} serial_device_t;

/* UART is the chipset implementing the serial port. These are it's registers
 * expressed in terms of offsets relative to the base chipset base address.
 *
 * UART Registers
 * Base Address  DLAB  I/O Access  Abbrv.  Register Name
 *     +0         0    Write       THR     Transmitter Holding Buffer
 *     +0         0    Read        RBR     Receiver Buffer
 *     +0         1    Read/Write  DLL     Divisor Latch Low Byte
 *     +1         0    Read/Write  IER     Interrupt Enable Register
 *     +1         1    Read/Write  DLH     Divisor Latch High Byte
 *     +2         x    Read        IIR     Interrupt Identification Register
 *     +2         x    Write       FCR     FIFO Control Register
 *     +3         x    Read/Write  LCR     Line Control Register
 *     +4         x    Read/Write  MCR     Modem Control Register
 *     +5         x    Read        LSR     Line Status Register
 *     +6         x    Read        MSR     Modem Status Register
 *     +7         x    Read/Write  SR      Scratch Register
 */

/* Serial ports' offsets. base is the base address of a COM device. */
#define SERIAL_DATA_PORT(base)                    (base)
#define SERIAL_ENABLED_INTERRUPTS_PORT(base)      ((base) + 1)
#define SERIAL_INTERRUPT_ID_PORT(base)            ((base) + 2)
#define SERIAL_FIFO_CONTROL_PORT(base)            ((base) + 2)
#define SERIAL_LINE_CONTROL_PORT(base)            ((base) + 3)
#define SERIAL_MODEM_CONTROL_PORT(base)           ((base) + 4)
#define SERIAL_LINE_STATUS_PORT(base)             ((base) + 5)
#define SERIAL_MODEM_STATUS_PORT(base)            ((base) + 6)
#define SERIAL_SCRATCH_PORT(base)                 ((base) + 7)

/* When DLAB is enabled DATA and INTERRUPT_ENABLE become DIVISOR_LSB and
 * DIVISOR_MSB respectively. */
#define SERIAL_DIVISOR_LSB_PORT(base)             (base)
#define SERIAL_DIVISOR_MSB_PORT(base)             ((base) + 1)

/* DLAB, which switches the interpretation on ports offsets 0 and 1, is the
 * highest bit in LINE_CONTROL port. */
#define SERIAL_ENABLE_DLAB                        0x80

/* FIFO Control Register-related flags */
#define SERIAL_FIFO_CTRL_ENABLE_FIFO              0x01
#define SERIAL_FIFO_CTRL_CLEAR_RCV_FIFO           0x02
#define SERIAL_FIFO_CTRL_CLEAR_TRX_FIFO           0x04
#define SERIAL_FIFO_CTRL_DMA_MODE_SELECT          0x08
#define SERIAL_FIFO_CTRL_ENABLE_64_BYTES_FIFO     0x20
#define SERIAL_FIFO_CTRL_INT_LEVEL_1              0x00
#define SERIAL_FIFO_CTRL_INT_LEVEL_4_ON_16        0x40
#define SERIAL_FIFO_CTRL_INT_LEVEL_8_ON_16        0x80
#define SERIAL_FIFO_CTRL_INT_LEVEL_14_ON_16       0xc0
#define SERIAL_FIFO_CTRL_INT_LEVEL_16_ON_64       0x40
#define SERIAL_FIFO_CTRL_INT_LEVEL_32_ON_64       0x80
#define SERIAL_FIFO_CTRL_INT_LEVEL_56_ON_64       0xc0

/* Status bits for LINE protocol. */
#define SERIAL_LINE_STATUS_DATA_RECEIVED          0x01
#define SERIAL_LINE_STATUS_OVERRUN_ERROR          0x02
#define SERIAL_LINE_STATUS_PARITY_ERROR           0x04
#define SERIAL_LINE_STATUS_FRAMING_ERROR          0x08
#define SERIAL_LINE_STATUS_BREAK_INTERRUPT        0x10
#define SERIAL_LINE_STATUS_EMPTY_TRANSMITTER_REG  0x20
#define SERIAL_LINE_STATUS_EMPTY_DATA_HOLDING_REG 0x40
#define SERIAL_LINE_STATUS_ERROR_IN_RECV_FIFO     0x20

/* Interrupt bits used in the interrupts ctrl register. */
#define SERIAL_INT_NONE                           0x00
#define SERIAL_INT_DATA_AVAILABLE                 0x01
#define SERIAL_INT_TRANSMITER_EMPTY               0x02
#define SERIAL_INT_LINE_STATUS_CHANGE             0x04
#define SERIAL_INT_MODEM_STATUS_CHANGE            0x08
#define SERIAL_INT_ENABLE_SLEEP_MODE              0x10 /* 16750 */
#define SERIAL_INT_ENABLE_LOW_POWER_MODE          0x20 /* 16750 */
#define SERIAL_INT_RESERVED1                      0x40
#define SERIAL_INT_RESERVED2                      0x80

/* Interrupt identification bits. */
#define SERIAL_IIR_PENDING(iir)                   (!((iir) & 0x01))
/* The Interrupt Pending Flag is low-triggered in hardware, thus 0 means
 * there's a pending interrupt: that's why it's negated above. */

#define SERIAL_IIR_INTERRUPT(iir)                 ((iir) & 0x0e)
#define SERIAL_IIR_MODEM_STATUS                   0x00
#define SERIAL_IIR_TRX_HOLDER_EMPTY               0x02
#define SERIAL_IIR_RCV_DATA_AVAILABLE             0x04
#define SERIAL_IIR_LINE_STATUS                    0x06
#define SERIAL_IIR_RESERVED1                      0x08
#define SERIAL_IIR_RESERVED2                      0x0a
#define SERIAL_IIR_TIMEOUT                        0x0c
#define SERIAL_IIR_RESERVED3                      0x0e

#define SERIAL_IIR_64_FIFO(iir)                   ((iir) & 0x20)

#define SERIAL_IIR_FIFO_CONDITION(iir)            ((iir) & 0xc0)
#define SERIAL_IIR_NO_FIFO                        0x00
#define SERIAL_IIR_RESERVED4                      0x40
#define SERIAL_IIR_NOT_WORKING_FIFO               0x80
#define SERIAL_IIR_FIFO_ENABLED                   0xc0

/* Modem control register bits. */
#define SERIAL_MODEM_CTRL_DATA_TERMINAL_READY     0x01
#define SERIAL_MODEM_CTRL_REQUEST_TO_SEND         0x02
#define SERIAL_MODEM_CTRL_AUX_OUTPUT_1            0x04
#define SERIAL_MODEM_CTRL_AUX_OUTPUT_2            0x08
#define SERIAL_MODEM_CTRL_LOOPBACK_MODE           0x10
#define SERIAL_MODEM_CRTL_AUTOFLOW_MODE           0x20 /* 16750 */
#define SERIAL_MODEM_CTRL_RESERVED1               0x40
#define SERIAL_MODEM_CTRL_RESERVED2               0x80

/* Default values. */
#define SERIAL_DEFAULT_DIVISOR                    3

/* Declaration of the interrupt handler. */
void serial_interrupt_handler(itr_cpu_regs_t regs,
                              itr_intr_data_t data,
                              itr_stack_state_t stack);


/* Serial devices. */
static serial_device_t devices[SERIAL_TOTAL_DEVICES] = {
  {
    .devid = DEV_MAKE_DEV(DEV_TTY_MAJOR, SERIAL_COM1_MINOR),
    .base = SERIAL_COM1_BASE,
    .irq = SERIAL_COM1_IRQ,
    .type = SERIAL_TYPE_UNKNOWN,
    .name = "ttyS0",
    .flags = SERIAL_FLAG_UNUSED
  },
  {
    .devid = DEV_MAKE_DEV(DEV_TTY_MAJOR, SERIAL_COM2_MINOR),
    .base = SERIAL_COM2_BASE,
    .irq = SERIAL_COM2_IRQ,
    .type = SERIAL_TYPE_UNKNOWN,
    .name = "ttyS1",
    .flags = SERIAL_FLAG_UNUSED
  },
  {
    .devid = DEV_MAKE_DEV(DEV_TTY_MAJOR, SERIAL_COM3_MINOR),
    .base = SERIAL_COM3_BASE,
    .irq = SERIAL_COM3_IRQ,
    .type = SERIAL_TYPE_UNKNOWN,
    .name = "ttyS2",
    .flags = SERIAL_FLAG_UNUSED
  },
  {
    .devid = DEV_MAKE_DEV(DEV_TTY_MAJOR, SERIAL_COM4_MINOR),
    .base = SERIAL_COM4_BASE,
    .irq = SERIAL_COM4_IRQ,
    .type = SERIAL_TYPE_UNKNOWN,
    .name = "ttyS3",
    .flags = SERIAL_FLAG_UNUSED
  },
};

/* Configures a serial device. */
void serial_set_config(serial_device_t *dev) {
  if (dev->type == SERIAL_TYPE_UNKNOWN)
    return;

  /* Set divisor. */
  outb(SERIAL_LINE_CONTROL_PORT(dev->base), SERIAL_ENABLE_DLAB);
  outb(SERIAL_DIVISOR_LSB_PORT(dev->base), (u8)(dev->config.divisor & 0x00ff));
  outb(SERIAL_DIVISOR_MSB_PORT(dev->base), (u8)((dev->config.divisor >> 8) & 0x00ff));

  /* Set line control register while clearing DLAB, just in case. */
  outb(SERIAL_LINE_CONTROL_PORT(dev->base), dev->config.line_ctl & 0x7f);

  /* Set the FIFO control, if relevant. */
  if (dev->type != SERIAL_TYPE_8250) {
    outb(SERIAL_FIFO_CONTROL_PORT(dev->base), dev->config.fifo_ctl);
  }

  /* Set the MODEM control, if any. */
  outb(SERIAL_MODEM_CONTROL_PORT(dev->base), dev->config.modem_ctl);

  /* Set interrupts. */
  outb(SERIAL_ENABLED_INTERRUPTS_PORT(dev->base), dev->config.interrupts);
}

/* Forcefully read a byte from the serial line.
 * TODO: What to do if the buffer fills? Discard older data? */
void serial_read_byte(serial_device_t *dev) {
  lock();
  dev->read_buf.buffer[dev->read_buf.write_head] =
    inb(SERIAL_DATA_PORT(dev->base));
  dev->read_buf.write_head = (dev->read_buf.write_head + 1) %
                              SERIAL_BUFFER_LEN;
  unlock();
}

/* Forcefully write a byte to the serial line. */
void serial_write_byte(serial_device_t *dev, char c) {
  outb(SERIAL_DATA_PORT(dev->base), c);
}

/* Checks the line condition.
 * TODO: Do a real checking and try to recover from the error. */
void serial_check_line_condition(serial_device_t *dev) {
  u8 r8;

  r8 = inb(SERIAL_LINE_STATUS_PORT(dev->base));
  fb_printf("[serial %bd]: check_line_condition: %bb\n",
            DEV_MINOR(dev->devid),
            r8);
}

/* Interrupts handler. */
void serial_interrupt_handler(itr_cpu_regs_t regs,
                              itr_intr_data_t data,
                              itr_stack_state_t stack) {
  int i;
  u8 r8;

  for (i = 0; i < SERIAL_TOTAL_DEVICES; i ++) {
    /* Identify the interrupting device(s). */
    if (devices[i].type == SERIAL_TYPE_UNKNOWN)
      continue;
    if (devices[i].irq != data.irq)
      continue;
    r8 = inb(SERIAL_INTERRUPT_ID_PORT(devices[i].base));
    if (!SERIAL_IIR_PENDING(r8))
      continue;

    /* We've got a candidate. */
    switch (SERIAL_IIR_INTERRUPT(r8)) {
      case SERIAL_IIR_RCV_DATA_AVAILABLE:
        serial_read_byte(devices + i);
        break;
      case SERIAL_IIR_TRX_HOLDER_EMPTY:
        /* If there's no data to send to the line, reading again IIR will
         * cause the UART to clear the interrupt. This would make sense if
         * we were using a multiprogramming system, but since there's only
         * one executing process, sending bytes over the serial line is
         * made in blocking, busy-waiting mode, thus there's not much point
         * in doing anything with this interrupt. */
        inb(SERIAL_INTERRUPT_ID_PORT(devices[i].base));
        break;
      case SERIAL_IIR_LINE_STATUS:
        serial_check_line_condition(devices + i);
        break;
      case SERIAL_IIR_TIMEOUT:
        /* This interrupt is issued when there is data in the incoming fifo and
         * the processor hasn't retrieved it in the time it takes to receive
         * four chars from the serial link. It'll be triggered after a single
         * word is received. */
        serial_read_byte(devices + i);
        break;
      default:
        /* Not handled and not expected. */
        break;
    }
  }

  pic_send_eoi(data.irq);
}

static int serial_open(vfs_vnode_t *node, vfs_file_t *f) {
  int i;

  for (i = 0; i < SERIAL_TOTAL_DEVICES; i ++) {
    if (node->v_dev == devices[i].devid) {
      if (devices[i].flags == SERIAL_FLAG_IN_USE) {
        set_errno(E_BUSY);
        return -1;
      }
      devices[i].flags = SERIAL_FLAG_IN_USE;
      f->private_data = devices + i;
      return 0;
    }
  }

  set_errno(E_NODEV);
  return -1;
}

static int serial_release(vfs_vnode_t *node, vfs_file_t *f) {
  serial_device_t * dev;

  dev = (serial_device_t *)(f->private_data);
  dev->flags = SERIAL_FLAG_UNUSED;
  return 0;
}

static ssize_t serial_read(vfs_file_t *filp, char *buf, size_t count) {
  serial_device_t * dev;
  ssize_t bread;

  dev = (serial_device_t *)(filp->private_data);

  /* Make this synchronous by staying here until something arrives. I know
   * this is not a good apporach. */
  while (dev->read_buf.read_head == dev->read_buf.write_head) {
    hw_hlt();
  }

  lock();

  for (bread = 0;

       bread < count &&
       dev->read_buf.read_head != dev->read_buf.write_head;

       buf[bread ++] = dev->read_buf.buffer[dev->read_buf.read_head],
       dev->read_buf.read_head = (dev->read_buf.read_head + 1)
                                  % SERIAL_BUFFER_LEN);

  unlock();

  filp->f_pos += bread;

  return bread;
}

static ssize_t serial_write(vfs_file_t *filp, char *buf, size_t count) {
  serial_device_t * dev;
  ssize_t bwrit;
  u8 r8;

  dev = (serial_device_t *)(filp->private_data);

  for (bwrit = 0; bwrit < count; bwrit ++) {
    /* Wait until there's room in the device's buffer to send data. */
    while (1) {
      r8 = inb(SERIAL_LINE_STATUS_PORT(dev->base));
      if (r8 & SERIAL_LINE_STATUS_EMPTY_DATA_HOLDING_REG)
        break;
    };

    /* Send the byte. */
    serial_write_byte(dev, *(buf + bwrit));
  }

  filp->f_pos += bwrit;

  return bwrit;
}

static off_t serial_lseek(vfs_file_t *filp, off_t off, int whence) {
  set_errno(E_NOSEEK);
  return -1;
}

static int serial_ioctl(vfs_file_t *filp, int request, void *data) {
  return 0;
}

/* Initializes the serial devices and publishes the corresponding devices.
 * TODO: Better device discovery. */
int serial_init() {
  int i;
  u8 r8;
  vfs_file_operations_t ops;

  /* It doesn't matter which device we find, they'll share the same ops. */
  ops.open = serial_open;
  ops.release = serial_release;
  ops.flush = NULL;
  ops.read = serial_read;
  ops.write = serial_write;
  ops.lseek = serial_lseek;
  ops.ioctl = serial_ioctl;
  ops.readdir = NULL;

  /* Identify the devices and load the current values into our device
   * structures. */
  for (i = 0; i < SERIAL_TOTAL_DEVICES; i ++) {
    /* Let's try to figure out which devices are present or not. This may not
     * be the best way, but I don't want to start arbitrarily writing to I/O
     * ports. Thus, I'll check every single RESERVED bit in every single
     * possible port. If a single one happens to be set I'll assumen there's
     * no device present or at least that it is not a UART. */
    r8 = inb(SERIAL_ENABLED_INTERRUPTS_PORT(devices[i].base));
    if ((r8 & SERIAL_INT_RESERVED1) || (r8 & SERIAL_INT_RESERVED2))
      continue;
    r8 = inb(SERIAL_INTERRUPT_ID_PORT(devices[i].base));
    if (r8 & SERIAL_IIR_RESERVED4)
      continue;
    r8 = inb(SERIAL_MODEM_CONTROL_PORT(devices[i].base));
    if ((r8 & SERIAL_MODEM_CTRL_RESERVED1) ||
        (r8 & SERIAL_MODEM_CTRL_RESERVED2))
      continue;

    /* Try to activate all fields -except the reserved ones and the DMA's- and
     * check which ones got actually set. This procedure is taken from
     * Serial Programming/8250 UART Programming. */
    outb(SERIAL_FIFO_CONTROL_PORT(devices[i].base),
         SERIAL_FIFO_CTRL_ENABLE_FIFO           |
         SERIAL_FIFO_CTRL_CLEAR_RCV_FIFO        |
         SERIAL_FIFO_CTRL_CLEAR_TRX_FIFO        |
         SERIAL_FIFO_CTRL_ENABLE_64_BYTES_FIFO  |
         SERIAL_FIFO_CTRL_INT_LEVEL_56_ON_64);

    r8 = inb(SERIAL_INTERRUPT_ID_PORT(devices[i].base));
    /* Since the checkings below has no actual meaning, let's not provide
     * macros for it. What we are testing is whether the bits are reserved
     * or not, which help determine the type of device in use. */
    if (r8 & 0x40) {      /* Bit 6 */
      if (r8 & 0x80) {    /* Bit 7 */
        if (r8 & 0x20) {  /* Bit 5 */
          devices[i].type = SERIAL_TYPE_16750;
        }
        else {
          devices[i].type = SERIAL_TYPE_16550A;
        }
      }
      else {
        devices[i].type = SERIAL_TYPE_16550;
      }
    }
    else {
      /* Write a random number to the scratch register and read it back. On
       * old UARTs this register didn't actually exist. */
      outb(SERIAL_SCRATCH_PORT(devices[i].base), 0x2a);
      r8 = inb(SERIAL_SCRATCH_PORT(devices[i].base));
      if (r8 == 0x2a)
        devices[i].type = SERIAL_TYPE_16450;
      else
        devices[i].type = SERIAL_TYPE_8250;
    }

    /* Let's initialize the device with a low baud rate. We're not hurried,
     * right? */
    devices[i].config.divisor = SERIAL_DEFAULT_DIVISOR;

    /* Let's initialize the line protocol to 8-N-1 with no break interrupt. */
    devices[i].config.line_ctl = SERIAL_LINE_WORD_LENGTH_8    |
                                 SERIAL_LINE_PARITY_NONE      |
                                 SERIAL_LINE_DOUBLE_STOP_BITS;

    /* This are the interrupts we'll handle. */
    devices[i].config.interrupts = SERIAL_INT_DATA_AVAILABLE    |
                                   SERIAL_INT_TRANSMITER_EMPTY  |
                                   SERIAL_INT_LINE_STATUS_CHANGE;

    /* Configure the FIFO if it exists. */
    if (devices[i].type == SERIAL_TYPE_8250 ||
        devices[i].type == SERIAL_TYPE_16450) { /* TODO: Check this. */
      devices[i].config.fifo_ctl = 0; /* No FIFO. */
    }
    else if (devices[i].type == SERIAL_TYPE_16550 ||
             devices[i].type == SERIAL_TYPE_16550A)  {
      /* No 64 bytes internal FIFO. */
      devices[i].config.fifo_ctl = SERIAL_FIFO_CTRL_ENABLE_FIFO    |
                                   SERIAL_FIFO_CTRL_CLEAR_RCV_FIFO |
                                   SERIAL_FIFO_CTRL_CLEAR_TRX_FIFO |
                                   SERIAL_FIFO_CTRL_INT_LEVEL_4_ON_16;
    }
    else {
      /* 64 bytes internal FIFO. */
      devices[i].config.fifo_ctl = SERIAL_FIFO_CTRL_ENABLE_FIFO          |
                                   SERIAL_FIFO_CTRL_CLEAR_RCV_FIFO       |
                                   SERIAL_FIFO_CTRL_CLEAR_TRX_FIFO       |
                                   SERIAL_FIFO_CTRL_ENABLE_64_BYTES_FIFO |
                                   SERIAL_FIFO_CTRL_INT_LEVEL_1;
    }

    devices[i].read_buf.read_head = devices[i].read_buf.write_head = 0;

    serial_set_config(devices + i);

    dev_register_char_dev(devices[i].devid, devices[i].name, &ops);
  }

  /* Set only the interrupt handlers that must be set. */
  for (i = 0; i < SERIAL_TOTAL_DEVICES; i ++) {
    if (devices[i].type != SERIAL_TYPE_UNKNOWN &&
        devices[i].irq == PIC_SERIAL_1_IRQ) {
      itr_set_interrupt_handler(PIC_SERIAL_1_IRQ,
                                serial_interrupt_handler,
                                IDT_PRESENT | IDT_DPL_RING_0 | IDT_GATE_INTR);
      break;
    }
  }
  for (i = 0; i < SERIAL_TOTAL_DEVICES; i ++) {
    if (devices[i].type != SERIAL_TYPE_UNKNOWN &&
        devices[i].irq == PIC_SERIAL_2_IRQ) {
      itr_set_interrupt_handler(PIC_SERIAL_2_IRQ,
                                serial_interrupt_handler,
                                IDT_PRESENT | IDT_DPL_RING_0 | IDT_GATE_INTR);
    }
  }

  return 0;
}
