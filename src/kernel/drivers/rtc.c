#include <rtc.h>
#include <io.h>
#include <hw.h>
#include <devices.h>
#include <typedef.h>
#include <errors.h>

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
	
	hw_cli();
	for(int i = 0; i < count; ++i)
		set_RTC_register(REGISTER_VALUES[i], buf[i]);
	hw_sti();
	filp->f_pos += count;

	return (ssize_t)count;
}

static ssize_t rtc_read(vfs_file_t *filp, char *buf, size_t count) {
	
	hw_cli();
	for(int i = 0; i < count; ++i)
		buf[i] = get_RTC_register(REGISTER_VALUES[i]);
	hw_sti();
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

	REGISTER_VALUES[0] = REG_SECONDS;
	REGISTER_VALUES[1] = REG_MINUTES;
	REGISTER_VALUES[2] = REG_HOURS;
	REGISTER_VALUES[3] = REG_DAY;
	REGISTER_VALUES[4] = REG_MONTH;
	REGISTER_VALUES[5] = REG_YEAR;
	REGISTER_VALUES[6] = REGB_STATUS;

	dev_register_char_dev(DEV_MAKE_DEV(RTC_MAJOR, RTC_MINOR), 
						  "rtc", 
							&rtc_ops);
	fdrtc = vfs_open("/dev/rtc", FILE_O_RW, 0);
  	if(fdrtc == NULL)
  		kernel_panic("no /dev/rtc\n");
}


void NMI_enable() {
	outb(CMOS_ADDRESS, inb(CMOS_ADDRESS) & 0x7F);
}


void NMI_disable() {
	outb(CMOS_ADDRESS, inb(CMOS_ADDRESS) | 0x80);
}


u8 get_RTC_register(u8 reg) {
	outb(CMOS_ADDRESS, reg);
	u8 ret = inb(CMOS_DATA);
	return ret;
}

void set_RTC_register(u8 reg_addres, u8 data) {
	outb(CMOS_ADDRESS, reg_addres);
	outb(CMOS_DATA, data);
}


int get_update_in_progress_flag() {
      outb(CMOS_ADDRESS, REGA_STATUS);
      return (inb(CMOS_DATA) & 0x80);
}

