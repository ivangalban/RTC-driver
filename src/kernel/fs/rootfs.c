#include <vfs.h>
#include <string.h>

vfs_fs_t * rootfs_ft_get_fs(char *dev) {
  if (strcmp(dev, "rootfs") == 0)
    return 
}

int mount(vfs_fs_t *fs) {

}

int unmount(vfs_fs_t *fs) {

}

int read_vnode(vfs_vnode_t *vnode) {

}

vfs_fs_operations_t rootfs_fs_operations = {
  .mount = rootfs_fs_mount,
  .unmount = rootfs_fs_unmount,
  .read_vnode = rootfs_fs_readvnode
};

vfs_fs_t rootfs_fs = {
  .fs_dev       = DEV_MAKE_DEV(VFS_ROOTFS_MAJOR, VFS_ROOTFS_MINOR),
  .fs_blocksize = 0,
  .fs_blocks    = 0,
  .fs_type      = VFS_TYPE_ROOTFS,
  .fs_parent    = NULL,
  .fs_mpoint    = NULL,
  .fs_root      = &vfs_rootdir,
  .fs_count     = 0,
  .fs_ops       = &rootfs_fs_operations,
};

/* RootFS implementation. */
int rootfs_vnode_lookup(vfs_vnode_t *dir, vfs_dentry_t *dentry, char *name) {

}

int rootfs_vnode_create(vfs_vnode_t *dir, vfs_dentry_t *dentry, mode_t mode,
                        char *name) {

}

vfs_vnode_operations_t rootfs_vnode_operations = {
  .lookup = rootfs_vnode_lookup,
  .create = rootfs_vnode_create
};


vfs_vnode_t rootfs_root_node = {
  .v_no     = VFS_ROOTFS_ROOT_INO,
  .v_count  = 0,
  .v_mode   = FILE_TYPE_DIRECTORY   |
              FILE_PERM_USR_READ    |
              FILE_PERM_USR_WRITE   |
              FILE_PERM_USR_EXEC    |
              FILE_PERM_GRP_READ    |
              FILE_PERM_GRP_EXEC    |
              FILE_PERM_OTHERS_READ |
              FILE_PERM_OTHERS_EXEC,
  .v_size   = 0,
  .v_fs     = &rootfs_fs,
  .v_fops   = NULL,
  .v_iops   = &rootfs_vnode_operations
};

vfs_dentry_t rootfs_root_dentry = {
  .d_name   = "/",
  .d_vnode  = &rootfs_root_node,
  .d_parent = NULL,
  .d_fs     = &rootfs_fs,
  .d_mount  = NULL,
  .d_count  = 0
};
