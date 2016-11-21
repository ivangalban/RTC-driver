#include <vfs.h>
#include <list.h>
#include <mem.h>

#define VFS_MAX_VNODES        1024
#define VFS_MAX_FILES         1024
#define VFS_DEFAULT_BLK_SIZE  1024

/* Filesystem types. */
static list_t vfs_fs_types;

/* Superblocks. */
static list_t vfs_sbs;

/* Mountpoints. */
static list_t vfs_mountpoints;

/* Dentries. */
static list_t vfs_dentries;

/* VNodes. */
static list_t vfs_vnodes;

/* Open files. */
static list_t vfs_files;


/* Heads of the whole VFS tree. */
static vfs_fs_t     * vfs_root_fs;
static vfs_vnode_t  * vfs_root_node;
static vfs_dentry_t * vfs_root_dentry;


vfs_fs_t * vfs_fs_alloc() {
  vfs_fs_t *fs = (vfs_fs_t *)kalloc(sizeof(vfs_fs_t));
  if (fs == NULL)
    return NULL;
  memset(fs, 0, sizeof(vfs_fs_t));
  return fs;
}

int vfs_fs_release(vfs_fs_t *fs) {
  if (fs->fs_count > 0) {
    set_errno(E_BUSY);
    return -1;
  }
  kfree(fs);
  return 0;
}

int vfs_init() {
  list_init(&vfs_vnodes);
  list_init(&vfs_files);
  list_init(&vfs_fss);

  return 0;
}
