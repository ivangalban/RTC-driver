#include <vfs.h>
#include <string.h>
#include <devices.h>
#include <fs/memfs.h>
#include <fs/rootfs.h>
#include <mem.h>
#include <errors.h>

/*****************************************************************************/
/* Modules API ***************************************************************/
/*****************************************************************************/

/* Initialization only registers the filesystem type. */
int rootfs_init() {
  return memfs_create(ROOTFS_NAME, ROOTFS_DEVID, MEMFS_FLAGS_ALLOW_FILES |
                                                 MEMFS_FLAGS_ALLOW_DIRS  |
                                                 MEMFS_FLAGS_ALLOW_NODES);
}
