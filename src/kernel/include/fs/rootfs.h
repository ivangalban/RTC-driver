/* This is the implementation of the initial rootfs filesystem. */

#ifndef __FS_ROOTFS_H__
#define __FS_ROOTFS_H__

#include <devices.h>

/* Let's reuse the reserved UNNAMED MAJOR just to be homogeneus. */
#define ROOTFS_MAJOR    DEV_UNNAMED_MAJOR
#define ROOTFS_MINOR    1
#define ROOTFS_DEVID    DEV_MAKE_DEV(ROOTFS_MAJOR, ROOTFS_MINOR)

#define ROOTFS_NAME     "rootfs"

int rootfs_init();

#endif
