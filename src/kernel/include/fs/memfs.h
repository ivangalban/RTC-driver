#ifndef __MEMFS_H__
#define __MEMFS_H__

#include <devices.h>

#define MEMFS_FLAGS_ALLOW_DIRS          0x00000001
#define MEMFS_FLAGS_ALLOW_FILES         0x00000002
#define MEMFS_FLAGS_ALLOW_NODES         0x00000004

int memfs_create(char *name, dev_t devid, int flags);

#endif
