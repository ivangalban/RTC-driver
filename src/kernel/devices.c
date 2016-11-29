#include <devices.h>
#include <list.h>
#include <errors.h>
#include <vfs.h>
#include <fs/memfs.h>

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
