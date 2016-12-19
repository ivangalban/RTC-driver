#include <rtc.h>
#include <io.h>
#include <hw.h>
#include <devices.h>


static int rtc_open_b(dev_char_device_t *dev, dev_mode_t mode) {
	return 0;
}


static int rtc_read_b(dev_char_device_t *dev, char *c) {
	return 0;
}

static int rtc_write_b(dev_char_device_t *dev, char *c) {
	return 0;
}


static dev_char_device_operations_t rtc_operations = {
  .open    = rtc_open_b,
  .release = NULL,
  .read    = rtc_read_b,
  .write   = rtc_write_b,
  .ioctl   = NULL
};


static dev_char_device_t dev_rtc = {
  .devid = DEV_MAKE_DEV(RTC_MAJOR, RTC_MINOR),
  .count = 0,
  .ops = &rtc_operations
};


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
	dev_register_char_device(&dev_rtc);
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

