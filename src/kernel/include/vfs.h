/*
 * First, there will be filesystem types. Each type will be responsible of
 * identifying filesystem on every partition scanned on a new device or when
 * "special" filesystems (e.g. rootfs, devfs) are initialized. This process
 * should return a superblock if the identification succeeds. Filesystem types
 * are identified by their names, i.e. a string.
 *
 * Then we'll have superblocks. Each superblock will contain the filesystem-
 * level operations (e.g. mount, unmount, get_inode), which will be used when
 * the filesystem gets mounted into a mountpoint. Unlike Linux, we will only
 * allow a superblock to be mounted once because it's simpler.
 *
 * Dentries are structures associated to filesystems via mountpoints or to
 * files via vnodes. They hold the name of the dentry, the parent dentry in the
 * filesystem tree and the vnode or mounted superblock they refer to.
 * Dentries are identified by their name and their parent dentry.
 *
 * VNodes represent regular files and directories in a given filesystem, as
 * well as devices and special files (e.g. pipes). They are identified by a
 * inode number and hold operations that are performed on inodes, specially
 * those operating on directories (e.g. readdir, create, unlink), and a copy
 * of the operations performed on files. However, processes don't interact
 * directly with vnodes: they should use files instead.
 *
 * Files represent open files in a given filesystem. Basically they hold the
 * information about the interaction between a file and a process, keeping
 * track of the file offset and checking the openning conditions.
 *
 * Finally, user space processes work on files through file descriptors,
 * meaning they can have more than one instance to refer to the same open file.
 *
 * This modules orchestrates this process almos completely, except for the
 * file descriptors, which should be part of the process subsystems once it
 * gets done and we move into userland.
 *
 * All these structures will be stored in the generic lists, they will be
 * allocated and deallocated using specific functions and can be obtained
 * using a lookup function. No direct manipulation of any of these lists is
 * allowed to other modules.
 */

#ifndef __VFS_H__
#define __VFS_H__

#include <typedef.h>
#include <list.h>

#define FILE_PERM_755               ( FILE_PERM_USR_READ    | \
                                      FILE_PERM_USR_WRITE   | \
                                      FILE_PERM_USR_EXEC    | \
                                      FILE_PERM_GRP_READ    | \
                                      FILE_PERM_GRP_EXEC    | \
                                      FILE_PERM_OTHERS_READ | \
                                      FILE_PERM_OTHERS_EXEC )

/* Use this if you want to set v_dev on a non-device file. */
#define FILE_NODEV                  0

/******************************/
/*  Main internal structures  */
/******************************/

/* This exists just to let the compiler know there are structures with these
 * names, otherwise cyclic references won't work. */
typedef struct vfs_fs_type                vfs_fs_type_t;
typedef struct vfs_fs_type_operations     vfs_fs_type_operations_t;
typedef struct vfs_sb                     vfs_sb_t;
typedef struct vfs_sb_operations          vfs_sb_operations_t;
typedef struct vfs_mountpoint             vfs_mountpoint_t;
typedef struct vfs_mountpoint_operations  vfs_mountpoint_operations_t;
typedef struct vfs_dentry                 vfs_dentry_t;
typedef struct vfs_dentry_operations      vfs_dentry_operations_t;
typedef struct vfs_vnode                  vfs_vnode_t;
typedef struct vfs_vnode_operations       vfs_vnode_operations_t;
typedef struct vfs_file                   vfs_file_t;
typedef struct vfs_file_operations        vfs_file_operations_t;

/*****************************************************************************/
/* Filesystem types. *********************************************************/
/*****************************************************************************/

/* Filesystem type operations. */
struct vfs_fs_type_operations {
  /* Tries to identify a superblock. The device is identified by sb->sb_devid.
   * In case of success, the filesystem type must put the right values in the
   * sb_ops field of the superblock structure and return non -1. The rest of
   * the fields can be also set, but are not mandatory at this moment. In case
   * of failure it must return -1 and leave the structure intact. */
  int (* ft_get_sb) (vfs_sb_t *sb);

  /* Deallocates a given superblock, releasing all resources associated.
   * Probably due to the device being ejected, which is something we won't
   * do. It should return non-zero */
  int (* ft_kill_sb) (vfs_sb_t *sb);
};

/* Filesystem type structure. */
struct vfs_fs_type {
  vfs_fs_type_operations_t    ft_ops;       /* FS type operations. */
  struct vfs_fs_type_ro {
    char                    * ft_name;      /* FS type name. */
  } ro;
};


/* Registration takes place in two steps:
 * 1. The module registering the filesystem calls
 *      vfs_register_filesystem_type(name, callback)
 *    where name is the filesystem type's name and callback is of type
 *      vfs_fs_type_config_t
 *    used to configure the filesystem type.
 * 2. The system will call the passed callback in order to complete
 *    registration after the filesystem type structure gets allocated. */

typedef int (* vfs_fs_type_config_t) (vfs_fs_type_t *);
int vfs_fs_type_register(char *name, vfs_fs_type_config_t config);

/*****************************************************************************/
/* Superblocks. **************************************************************/
/*****************************************************************************/

#define VFS_SB_F_UNUSED           0x00000000

#define VFS_SB_F_DIRTY            0x00000001
#define VFS_SB_IS_DIRTY(sb)       ((sb)->sb_flags & VFS_SB_F_DIRTY)
#define VFS_SB_MARK_DIRTY(sb)     ((sb)->sb_flags |= VFS_SB_F_DIRTY)
#define VFS_SB_MARK_CLEAN(sb)     ((sb)->sb_flags &= (~VFS_SB_F_DIRTY))

#define VFS_SB_F_MOUNTED          0x00000002
#define VFS_SB_IS_MOUNTED(sb)     ((sb)->sb_flags & VFS_SB_F_MOUNTED)
#define VFS_SB_MARK_MOUNTED(sb)   ((sb)->sb_flags |= VFS_SB_F_MOUNTED)
#define VFS_SB_MARK_UNMOUNTED(sb) ((sb)->sb_flags &= (~VFS_SB_F_MOUNTED))

/* Superblock operations. */
struct vfs_sb_operations {
  /* Reads a vnode from the filesystem into node. Only the vnode number is
   * suppossed to be set and ro.v_sb. */
  int (* read_vnode) (vfs_sb_t *sb, vfs_vnode_t *node);

  /* Called when the vnode is about to be deleted from memory. Usually used
   * to free private_data or do any operation the filesystem needs to.
   * NOTE: This is meant to physically delete nodes, just to release the
   * resources associated to a vnode that is no longer in use, for instance,
   * when the last open file referencing it is closed.
   * If set to NULL it just won't be called, which is normal in filesystems
   * that don't need to take any special action when this happens. */
  int (* destroy_vnode) (vfs_sb_t *sb, vfs_vnode_t *node);

  /* Writes a vnode into the filesystem. It is suppossed to be an existent
   * vnode, which means there should be a vnode number and the rest of the
   * data. Since data is manipulated via inode operations this is just meant
   * to save metadata. */
  int (* write_vnode) (vfs_sb_t *sb, vfs_vnode_t *node);

  /* Removes a vnode from the filesystem. The vnode is suppossed to exist
   * though only the vnode number and ro.v_sb are guaranteed to be set.
   * It's not necessary to release any data from the vnode structure since
   * destroy_vnode will be called inmediately after. */
  int (* delete_vnode) (vfs_sb_t *sb, vfs_vnode_t *node);

  /* Used to notify the superblock it's being mounted. */
  int (* mount) (vfs_sb_t *sb);

  /* Used to nofity the superblock is being unmounted. */
  int (* unmount) (vfs_sb_t *sb);
};

/* The superblock structure. */
struct vfs_sb {
  size_t                        sb_blocksize;     /* Block size in bytes. */
  size_t                        sb_blocks;        /* Total blocks. */
  size_t                        sb_max_bytes;     /* Maximum file length. */
  vfs_sb_operations_t           sb_ops;           /* Superblock operations. */
  int                           sb_root_vno;      /* Root vnode number. */
  int                           sb_flags;         /* Flags: DIRTY, MOUNTED */
  void                        * private_data;     /* Private data. */
  struct vfs_sb_ro {
    dev_t                       sb_devid;         /* Device ID. */
    vfs_fs_type_t             * sb_fs_type;       /* File system type. */
    vfs_dentry_t              * sb_mnt;           /* Mountpoint dentry. */
  } ro;
};


/*****************************************************************************/
/* Dentries ******************************************************************/
/*****************************************************************************/

/* dentry structure. */
struct vfs_dentry {
  char                    * d_name;       /* Dentry name. */
  int                       d_vno;        /* vnode number this entry refers to
                                           * if not a mountpoint. */
  struct vfs_dentry_ro {
    vfs_dentry_t          * d_parent;     /* Parent dentry. */
    vfs_sb_t              * d_sb;         /* Superblock this dentry belongs
                                           * to. */
    vfs_sb_t              * d_mnt_sb;     /* Filesystem mounted in this
                                           * dentry if mountpoint. */
    int                     d_count;      /* Usage count. */
  } ro;
};


/*****************************************************************************/
/* vnodes ********************************************************************/
/*****************************************************************************/

/* v-node operations. These operations basically take place without the need
 * of an instance to the vnode (aka open file or open dir). */
struct vfs_vnode_operations {
  /* Looks for a dentry with the given dentry name. The vnode is valid, but the
   * dentry will only have d_name set. If the dentry is found, the rest of the
   * data should be filled in. */
  int (* lookup) (vfs_vnode_t *dir, vfs_dentry_t *dentry);

  /* Creates a regular file in dir, named after dentry->d_name and with mode
   * mode. dentry->d_vno must be updated accordingly. */
  int (* create) (vfs_vnode_t *dir, vfs_dentry_t *dentry, mode_t mode);

  /* Create a new directory in dir named after dentry->d_name with mode mode.
   * dentry->d_vno must be updated. */
  int (* mkdir) (vfs_vnode_t *dir, vfs_dentry_t *dentry, mode_t mode);

  /* Creates a new device special file with dev id devid and mode mode in dir
   * named after dentry->d_name. */
  int (* mknod) (vfs_vnode_t *dir, vfs_dentry_t *dentry, mode_t mode,
                 dev_t devid);

  /* TODO: Many more functions. */
};

/* File operations. */
struct vfs_file_operations {
  /* Opens vnode and fills file. Called every time the vnode is opened. To me,
   * this function, as well as release, should be part of vnode_operations
   * instead, but it's true that it would make the file interface used for
   * device drivers require implementing both inode_operations and
   * file_operations, which is kind of weird. */
  int (* open) (vfs_vnode_t *node, vfs_file_t *file);

  /* Releases the vnode. Called when the all opened files referencing this
   * vnode are closed. The file passed is the last of them. */
  int (* release) (vfs_vnode_t *node, vfs_file_t *file);

  /* Called each time a file is closed. */
  int (* flush) (vfs_file_t *file);

  /* Read _count_ bytes from _file_ starting at _off_ into _buf_. */
  ssize_t (* read) (vfs_file_t *file, char *buf, size_t count);

  /* Write _count_ bytes into _file_ starting at _off_ from _buf_. */
  ssize_t (* write) (vfs_file_t *file, char *buf, size_t count);

  /* Update _file_.f_pos to _off_ relative to _origin_. */
  off_t (* lseek) (vfs_file_t *file, off_t off, int whence);

  /* Performs ioctl _cmd_ with _arg_ on _file_. */
  int (* ioctl) (vfs_file_t *file, int cmd, void *arg);

  /* Reads the next name in a directory. This is quite different from Linux
   * interface. */
  char * (* readdir) (vfs_file_t *file);
};

/* vnode structure. */
struct vfs_vnode {
  /* TODO: This structure is very incomplete. */
  int                           v_no;         /* vnode number. */
  mode_t                        v_mode;       /* Type and permissions. */
  size_t                        v_size;       /* File size in bytes. */
  dev_t                         v_dev;        /* Device ID (if device file). */
  vfs_vnode_operations_t        v_iops;       /* Inode operations. */
  vfs_file_operations_t         v_fops;       /* File operations. */
  void                        * private_data; /* Private data. */
  struct vfs_node_ro {
    vfs_sb_t                  * v_sb;         /* The superblock containing
                                               * this vnode. */
    int                         v_count;      /* How many open files use this
                                               * vnode. */
  } ro;
};

/*****************************************************************************/
/* Open files ****************************************************************/
/*****************************************************************************/

/* open file structure. */
struct vfs_file {
  off_t                   f_pos;          /* Current offset in this file. */
  vfs_file_operations_t   f_ops;          /* File operations. */
  void                  * private_data;   /* Pointer to private data associated
                                           * open file if needed. */
  int                     f_flags;        /* Flags used when the file was
                                           * opened. */
  struct vfs_file_ro {
    int                   f_count;        /* How many descriptors reference
                                           * this open file. */
    vfs_vnode_t         * f_vnode;        /* Pointer to the backing vnode. */
  } ro;
};

/*****************************************************************************/
/* Others ********************************************************************/
/*****************************************************************************/

/** Kernel use only. **/
/* Init routine. */
int vfs_init();

/*****************************************************************************/
/* API ***********************************************************************/
/*****************************************************************************/
int vfs_mount(dev_t devid, char *path, char *fs_type);
int vfs_stat(char *path, struct stat *stat);
int vfs_mkdir(char *path, mode_t mode);
int vfs_mknod(char *path, mode_t mode, dev_t dev);
vfs_file_t * vfs_open(char *path, int flags, mode_t mode);
ssize_t vfs_write(vfs_file_t *filp, void *buf, size_t count);
ssize_t vfs_read(vfs_file_t *filp, void *buf, size_t count);
off_t vfs_lseek(vfs_file_t *filp, off_t off, int whence);

#endif
