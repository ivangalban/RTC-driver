#ifndef __SERIAL_H_
#define __SERIAL_H_

#include <typedef.h>

/* Configuring the devices is now done via ioctl calls.                      */
/* Request                                   |   Arg pointer type   | in/out */
/* ------------------------------------------|----------------------|--------*/
#define SERIAL_IOCTL_GET_DIVISOR          1  /*       u16           |   out  */
#define SERIAL_IOCTL_SET_DIVISOR          2  /*       u16           |   in   */
#define SERIAL_IOCTL_CLEAR_RCX_FIFO       3  /*       None          |   N/A  */
#define SERIAL_IOCTL_CLEAR_TRX_FIFO       4  /*       None          |   N/A  */
#define SERIAL_IOCTL_GET_LINE_PROTO       5  /* serial_line_proto_t |   out  */
#define SERIAL_IOCTL_SET_LINE_PROTO       6  /* serial_line_proto_t |   in   */

/* Line protocol related values. */
typedef u8  serial_line_proto_t;

#define SERIAL_LINE_WORD_LENGTH_5         0x00
#define SERIAL_LINE_WORD_LENGTH_6         0x01
#define SERIAL_LINE_WORD_LENGTH_7         0x02
#define SERIAL_LINE_WORD_LENGTH_8         0x03

#define SERIAL_LINE_SINGLE_STOP_BIT       0x00
#define SERIAL_LINE_DOUBLE_STOP_BITS      0x04  /* It's 1.5 if char len is 5 */

#define SERIAL_LINE_PARITY_NONE           0x00
#define SERIAL_LINE_PARITY_ODD            0x04
#define SERIAL_LINE_PARITY_EVEN           0x14
#define SERIAL_LINE_PARITY_MARK           0x24
#define SERIAL_LINE_PARITY_SPACE          0x34

/* Initialize the serial subsystem */
int serial_init();

#endif
