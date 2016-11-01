#include <devices.h>
#include <list.h>
#include <errors.h>

/* Globals */
list_t chr_devs;
list_t blk_devs;

/* Just init out globals */
int dev_init() {
  list_init(&chr_devs);
  list_init(&blk_devs);
  return 0;
}

/* Register block device */
int dev_register_block_device(dev_block_device_t *dev) {
  dev_block_device_t *d;

  d = (dev_block_device_t *)list_find(&blk_devs, dev->devid);
  if (d != NULL) {
    /* Device is already registered... TODO: What should be do? */
    return 0;
  }
  return list_add(&blk_devs, dev, dev->devid);
}

/* Remove block device */
int dev_remove_block_device(dev_t devid) {
  int pos;

  pos = list_find_pos(&blk_devs, devid);
  if (pos == -1)
    return -1;

  return list_del(&blk_devs, pos);
}

/* Get block device */
dev_block_device_t * dev_get_block_device(dev_t devid) {
  return (dev_block_device_t *)list_find(&blk_devs, devid);
}

/* Register block device */
int dev_register_char_device(dev_char_device_t *dev) {
  dev_char_device_t *d;

  d = (dev_char_device_t *)list_find(&blk_devs, dev->devid);
  if (d != NULL) {
    /* Device is already registered... TODO: What should be do? */
    return 0;
  }
  return list_add(&blk_devs, dev, dev->devid);
}

/* Remove block device */
int dev_remove_char_device(dev_t devid) {
  int pos;

  pos = list_find_pos(&blk_devs, devid);
  if (pos == -1)
    return -1;

  return list_del(&blk_devs, pos);
}

/* Get block device */
dev_char_device_t * dev_get_char_device(dev_t devid) {
  return (dev_char_device_t *)list_find(&blk_devs, devid);
}
