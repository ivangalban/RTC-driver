#include <devices.h>
#include <list.h>
#include <errors.h>
#include <vfs.h>
#include <fs/memfs.h>
#include <mem.h>
#include <string.h>

#define DEVFS_ROOT_PATH       "/dev"

/* Globals */
list_t chr_devs;
list_t blk_devs;

/* Devices will be identified by devid, thus we need to provide a specific
 * comparison function for list_find to work properly. */
int dev_blk_list_cmp(void *blk_item, void *devid) {
  return ((dev_block_device_t *)blk_item)->devid == *((dev_t *)devid);
}
int dev_chr_list_cmp(void *chr_item, void *devid) {
  return ((dev_char_device_t *)chr_item)->devid == *((dev_t*)devid);
}

/* Just init out globals and register our filesystem. */
int dev_init() {
  list_init(&chr_devs);
  list_init(&blk_devs);
  if (memfs_create(DEV_FS_NAME, DEV_FS_DEVID, MEMFS_FLAGS_ALLOW_FILES |
                                              MEMFS_FLAGS_ALLOW_DIRS  |
                                              MEMFS_FLAGS_ALLOW_NODES) == -1)
    return -1;
  if (vfs_mkdir("/dev", 0755) == -1)
    return -1;
  if (vfs_mount(DEV_FS_DEVID, "/dev", DEV_FS_NAME) == -1)
    return -1;
  return 0;
}

/* Register block device */
int dev_register_block_device(dev_block_device_t *dev) {
  dev_block_device_t *d;

  d = (dev_block_device_t *)list_find(&blk_devs,
                                      dev_blk_list_cmp,
                                      &(dev->devid));
  if (d != NULL) {
    /* Device is already registered... TODO: What should be do? */
    return 0;
  }
  return list_add(&blk_devs, dev);
}

/* Remove block device */
int dev_remove_block_device(dev_t devid) {
  dev_block_device_t *blk;

  blk = (dev_block_device_t *)list_find_del(&blk_devs,
                                            dev_blk_list_cmp,
                                            &devid);
  if (blk == NULL) {
    return -1;
  }
  /* TODO: Check if we should free this. */
  return 0;
}

/* Get block device */
dev_block_device_t * dev_get_block_device(dev_t devid) {
  return (dev_block_device_t *)list_find(&blk_devs,
                                         dev_blk_list_cmp,
                                         &devid);
}

/* Register char device */
int dev_register_char_device(dev_char_device_t *dev) {
  dev_char_device_t *d;

  d = (dev_char_device_t *)list_find(&chr_devs,
                                     dev_chr_list_cmp,
                                     &(dev->devid));
  if (d != NULL) {
    /* Device is already registered... TODO: What should be do? */
    return 0;
  }
  return list_add(&chr_devs, dev);
}

/* Remove char device */
int dev_remove_char_device(dev_t devid) {
  dev_char_device_t *chr;

  chr = (dev_char_device_t *)list_find_del(&chr_devs,
                                           dev_chr_list_cmp,
                                           &devid);

  if (chr == NULL)
    return -1;

  /* TODO: Check if we need to free the object. */
  return 0;
}

/* Get char device */
dev_char_device_t * dev_get_char_device(dev_t devid) {
  return (dev_char_device_t *)list_find(&chr_devs, dev_chr_list_cmp, &devid);
}



/*****************************************************************************/
/* VFS-based API *************************************************************/
/*****************************************************************************/

static dev_char_device_t * dev_char_lookup(dev_t devid) {
  return list_find(&chr_devs, dev_chr_list_cmp, &devid);
}

/* Registers a char device. */
int dev_register_char_dev(dev_t devid,
                          char *name,
                          vfs_file_operations_t *ops) {
  dev_char_device_t *chr;
  char *path;
  mode_t mode;

  if (dev_char_lookup(devid) != NULL) {
    return -1;
  }

  /* Ask for memory to hold the device. */
  chr = (dev_char_device_t *)kalloc(sizeof(dev_char_device_t));
  if (chr == NULL) {
    return -1;
  }

  chr->devid = devid;
  chr->fops.open = ops->open;
  chr->fops.release = ops->release;
  chr->fops.flush = ops->flush;
  chr->fops.read = ops->read;
  chr->fops.write = ops->write;
  chr->fops.lseek = ops->lseek;
  chr->fops.ioctl = ops->ioctl;
  chr->fops.readdir = ops->readdir;

  /* Ask for memory for it's name. */
  chr->name = (char *)kalloc(strlen(name) + 1);
  if (chr->name == NULL) {
    kfree(chr);
    return -1;
  }

  if (list_add(&chr_devs, chr) == -1) {
    kfree(chr->name);
    kfree(chr);
    return -1;
  }

  /* Prepare the temporary root path. */
  path = (char *)kalloc(strlen(DEVFS_ROOT_PATH) + 1 + strlen(name) + 1);
  if (path == NULL) {
    list_find_del(&chr_devs, dev_chr_list_cmp, &devid);
    kfree(chr->name);
    kfree(chr);
    return -1;
  }
  strcpy(path, DEVFS_ROOT_PATH);
  strcpy(path + strlen(DEVFS_ROOT_PATH), "/");
  strcpy(path + strlen(DEVFS_ROOT_PATH) + 1, name);

  /* Try to compute perms. TODO: This should be more flexible. */
  mode = FILE_TYPE_CHAR_DEV;
  if (ops->read != NULL)
    mode |= FILE_PERM_USR_READ;
  if (ops->write != NULL)
    mode |= FILE_PERM_USR_WRITE;

  if (vfs_mknod(path, mode, devid) == -1) {
    list_find_del(&chr_devs, dev_chr_list_cmp, &devid);
    kfree(chr->name);
    kfree(chr);
    kfree(path);
    return -1;
  }

  kfree(path);
  return 0;
}


/* Unregisters a char device. */
int dev_unregister_char_dev(dev_t devid) {
  /* WIP */
  return -1;
}

/* Assign default file operations to a device when opened. */
int dev_set_char_operations(vfs_vnode_t *node, vfs_file_t *filp) {
  dev_char_device_t *chr;

  chr = dev_char_lookup(node->v_dev);
  if (chr == NULL) {
    set_errno(E_NODEV);
    return -1;
  }

  filp->f_ops.open = chr->fops.open;
  filp->f_ops.release = chr->fops.release;
  filp->f_ops.flush = chr->fops.flush;
  filp->f_ops.read = chr->fops.read;
  filp->f_ops.write = chr->fops.write;
  filp->f_ops.lseek = chr->fops.lseek;
  filp->f_ops.ioctl = chr->fops.ioctl;
  filp->f_ops.readdir = chr->fops.readdir;

  return 0;
}
