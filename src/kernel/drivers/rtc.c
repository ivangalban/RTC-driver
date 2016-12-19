#include <rtc.h>
#include <io.h>
#include <hw.h>
#include <devices.h>
#include <typedef.h>

#define show_call(f, d) fb_printf(#f " :%bd:%bd\n", DEV_MAJOR(d->devid), DEV_MINOR(d->devid))



/*****************************************************************************/
/* New VFS-based API *********************************************************/
/*****************************************************************************/

static int rtc_open(vfs_vnode_t *node, vfs_file_t *filp) {
	/* This checks should be improved. */
  if (filp->f_flags == FILE_O_RW)
    return 0;
  return -1;
}

static ssize_t rtc_write(vfs_file_t *filp, char *buf, size_t count) {
	
	for(int i = 0; i < count; ++i)
		set_RTC_register(i, buf[i]);
	filp->f_pos += count;
	
	return (ssize_t)count;
}

static ssize_t rtc_read(vfs_file_t *filp, char *buf, size_t count) {
	
	for(int i = 0; i < count; ++i)
		buf[i] = get_RTC_register(i);
	filp->f_pos += count;

	return (ssize_t)count;
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
	hw_cli();
	outb(CMOS_ADDRESS, reg);
	u8 ret = inb(CMOS_DATA);
	hw_sti();
	return ret;
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

