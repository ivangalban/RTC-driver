#include <rtc.h>
#include <io.h>
#include <hw.h>
#include <devices.h>



/*****************************************************************************/
/* New VFS-based API *********************************************************/
/*****************************************************************************/

static int rtc_open(vfs_vnode_t *node, vfs_file_t *filp) {
	return 0;
}

static ssize_t rtc_write(vfs_file_t *filp, char *buf, size_t count) {
	return 0;
}

static ssize_t rtc_read(vfs_file_t *filp, char *buf, size_t count) {
	return 0;
}

static vfs_file_operations_t rtc_ops = {
  .open    = rtc_open,
  .release = NULL,
  .flush   = NULL,
  .read    = rtc_read,
  .write   = rtc_write,
  .lseek   = NULL,
  .ioctl   = NULL,
  .readdir = NULL
};


void rtc_init() {
	dev_register_char_dev(DEV_MAKE_DEV(RTC_MAJOR, RTC_MINOR), 
						  "rtc", 
							&rtc_ops);
}


void NMI_enable() {
	outb(CMOS_ADDRESS, inb(CMOS_ADDRESS) & 0x7F);
}


void NMI_disable() {
	outb(CMOS_ADDRESS, inb(CMOS_ADDRESS) | 0x80);
}


u8 get_RTC_register(u8 reg) {
      outb(CMOS_ADDRESS, reg);
      return inb(CMOS_DATA);
}

void set_RTC_register(u8 reg_addres, u8 data) {
	hw_cli();
	outb(CMOS_ADDRESS, reg_addres);
	outb(CMOS_DATA, data);
	hw_sti();
}


int get_update_in_progress_flag() {
      outb(CMOS_ADDRESS, REGA_STATUS);
      return (inb(CMOS_ADDRESS) & 0x80);
}

