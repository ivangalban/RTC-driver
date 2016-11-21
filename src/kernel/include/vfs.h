/*
 * First, there will be filesystem types. Each type will be responsible of
 * identifying filesystem on every partition scanned on a new device or when
 * "special" filesystems (e.g. rootfs, devfs) are initialized. This process
 * should return a superblock if the identification succeeds.
 *
 * Then we'll have superblocks. Each superblock will contain the filesystem-
 * level operations (e.g. mount, unmount, get_inode), which will be used when
 * the filesystem gets mounted into a mountpoint.
 *
 * A mountpoint is a structure holding both a superblock and a dentry in a
 * parent filesystem. This dentry must be of type directory and its content
 * will be those of the mounted filesystem's root directory.
 *
 * Dentries are structures associated to filesystems via mountpoints or to
 * files via vnodes. They hold the name of the dentry, the parent dentry in the
 * filesystem tree and the vnode they refer to.
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
#include <devices.h>

#define FILE_MAX_NAME_LEN           32  /* MINIX only use 30 bytes. */

/*********************************************/
/*   File mode (i.e. type and permissions)   */
/*********************************************/
typedef u16 mode_t;   /* File mode. */

/* File types. */
#define FILE_TYPE_UNKNOWN       0x0000
#define FILE_TYPE_FIFO          0x1000
#define FILE_TYPE_CHAR_DEV      0x2000
#define FILE_TYPE_DIRECTORY     0x4000
#define FILE_TYPE_BLOCK_DEV     0x6000
#define FILE_TYPE_REGULAR       0x8000
#define FILE_TYPE_SYMLINK       0xa000
#define FILE_TYPE_SOCKET        0xc000
#define FILE_TYPE_WHT           0xe000 /* Found in the Linux kernel, though I
                                        * don't recognize it. */
#define FILE_TYPE(mode)         ((mode) & 0xf000)  /* ==> file type */

/* POSIX-compliant permissions. */
#define FILE_PERM_SETUID        0x0800
#define FILE_PERM_SETGID        0x0400
#define FILE_PERM_STICKY        0x0200
#define FILE_PERM_USR_READ      0x0100
#define FILE_PERM_USR_WRITE     0x0080
#define FILE_PERM_USR_EXEC      0x0040
#define FILE_PERM_GRP_READ      0x0020
#define FILE_PERM_GRP_WRITE     0x0010
#define FILE_PERM_GRP_EXEC      0x0008
#define FILE_PERM_OTHERS_READ   0x0004
#define FILE_PERM_OTHERS_WRITE  0x0002
#define FILE_PERM_OTHERS_EXEC   0x0001

/* File open status. Once a file is opened and in use, these flags control
 * which operations can be performed on them. */
#define FILE_FMODE_READ   0x0001  /* File is readable. */
#define FILE_FMODE_WRITE  0x0002  /* File is writtable. */
#define FILE_FMODE_LSEEK  0x0004  /* File is seekable. */


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

/** Filesystem types. **/
struct vfs_fs_type {
  char                      * ft_name;  /* FS type name. */
  int                         ft_count; /* Superblocks of this type in use. */
  list_t                      ft_sbs;   /* Superblocks of this type. */
  vfs_fs_type_operations_t  * ft_ops;   /* FS type operations. */
};
/* Filesystem type operations. */
struct vfs_fs_type_operations {
  /* Tries to identify a partition in the given device_path. If successful
   * it should return a superblock, otherwise NULL. Special devices like
   * devfs will receive hardcoded strings during initialization instead of
   * a real path. */
  vfs_sb_t * (ft_get_sb *) (char *path);

  /* Deallocates a given superblock, releasing all resources associated. */
  void * (fs_kill_fs *) (vfs_sb_t *fs);
};
/* Allocates a filesystem type structure. */
vfs_fs_type_t * vfs_fs_type_alloc();
/* Destroys a filesystem type structure. */
void vfs_fs_type_destroy(vfs_fs_type_t *ft);
/* Registers a filesystem type. */
int vfs_fs_type_register(vfs_fs_type_t *ft);
/* Unregisters a filesystem type. */
int vfs_fs_type_unregister(vfs_fs_type_t *ft);
/* Looks up for a filesystem type structure. */
vfs_fs_type_t * vfs_fs_type_lookup(char *name);

/** Superblocks. **/
struct vfs_sb {
  dev_t                         sb_devid;     /* Device ID. */
  size_t                        sb_blocksize; /* Block size in bytes. */
  ssize_t                       sb_blocks;    /* Total blocks in partition. */
  size_t                        sb_max_bytes; /* Maximum file length. */
  int                           sb_dirty;     /* Superblock is dirty. */
  vfs_dentry_t                * sb_root;      /* Root dentry. */
  vfs_fs_type_t               * sb_fs_type;   /* File system type. */
  vfs_sb_operations_t         * sb_ops;       /* Superblock operations. */
  list_t                        sb_mps;       /* Mountpoints from this sb. */
  list_t                        sb_vnodes;    /* vnodes in use. */
  list_t                        sb_dentries;  /* Dentries in use. */
  void                        * private_data; /* Private data. */
};
/* Superblocks operations. */
struct vfs_sb_operations {
  /* Allocates a vnode in memory. */
  vfs_vnode_t * (alloc_vnode *) (vfs_sb_t *sb);

  /* Destroys a vnode in memory. */
  void * (destroy_vnode *) (vfs_vnode_t *node);

  /* Fills the vnode structure. node.v_no must be set. */
  int (read_vnode *) (vfs_vnode_t *node);

  /* Writes a vnode into disk. */
  int (write_vnode *) (vfs_vnode_t *node);

  /* Removes a vnode from disk. */
  int (delete_vnode *) (vfs_vnode_t *node);

  /* Writes superblock */
  int (write_super *) (vfs_sb_t *sb);

  /* Releases the superblock when not in use. */
  int (release_super *) (vfs_sb_t *sb);
};
/* Registers a superblock. */
int vfs_sb_register(vfs_sb_t *sb);
/* Unregisters a superblock. */
int vfs_sb_unregister(vfs_sb_t *sb);
/* Looks up a superblock. */
vfs_sb_t * vfs_sb_lookup(dev_t devid);

/** Mountpoints **/
struct vfs_mountpoint {
  vfs_sb_t                    * m_sb;         /* Superblock. */
  vfs_dentry_t                * m_dentry;     /* Mountpoint dentry. */
  vfs_mountpoint_t            * m_parent;     /* Parent mountpoint. */
  list_t                        m_kids;       /* Child mountpoints. */
  void                        * private_data; /* Private data. */
};
/* Mount a filesystem. */
int vfs_mountpoint_mount(vfs_sb_t *sb, vfs_dentry_t *dentry);
/* Unmount a filesystem. */
int vfs_mountpoint_unmount(vfs_mountpoint_t *mp);
/* Lookup a mountpoint. */
vfs_mountpoint_t * vfs_mountpoint_lookup(vfs_dentry_t *dentry);


/** VNodes **/
/* TODO: This structure is very incomplete. */
struct vfs_vnode {
  int                           v_no;         /* vnode number. */
  int                           v_count;      /* How many open files use this
                                               * vnode. */
  mode_t                        v_mode;       /* Type and permissions. */
  size_t                        v_size;       /* File size in bytes. */
  dev_t                         v_dev;        /* Device ID (if device file). */
  vfs_sb_t                    * v_sb;         /* Superblock containing this
                                               * vnode. */
  list_t                      * v_dentries;   /* Dentries poiting to this
                                               * inode. */
  vfs_file_operations_t       * v_fops;       /* File operations. */
  vfs_vnode_operations_t      * v_iops;       /* Inode operations. */
  void                        * private_data; /* Private data. */
};
/* VNode operations. */
struct vfs_vnode_operations {
  /* Looks for a dentry with the given dentry name. */
  vfs_dentry_t * (lookup *) (vfs_vnode_t *dir, vfs_dentry_t *dentry);

  /* Creates a regular file in dir. */
  int (create *) (vfs_vnode_t *dir, vfs_dentry_t *dentry, mode_t mode);

  /* Creates a new directory. */
  int (mkdir *) (vfs_vnode_t *dir, vfs_dentry_t *dentry, mode_t mode);

  /* Create a device file. */
  int (mknod *) (vfs_vnode_t *dir, vfs_dentry_t *dentry, mode_t mode,
                 dev_t devid);

  /* TODO: Many more functions. */
};
/* Adds a vnode to the vnodes list. vnodes are allocated by superblocks. */
int vfs_vnode_add(vfs_vnode_t *node);
/* Removes a vnode from the list. */
int vfs_vnode_release(vfs_vnode_t *node);
/* Looks up a vnode. */
vfs_vnode_t * vfs_vnode_lookup(vfs_dentry_t *dentry);


/** Dentries. **/
struct vfs_dentry {
  char                    * name;         /* Dentry name. */
  int                       len;          /* Name length. */
  vfs_dentry_t            * d_parent;     /* Parent dentry. */
  vfs_sb_t                * d_sb;         /* Superblock this dentry belongs
                                           * to. */
  vfs_vnode_t             * d_vnode;      /* vnode this entry refers to
                                           * (if not a mountpoint). */
  vfs_mountpoint_t        * d_mp;         /* Filesystem mounted in this
                                           * dentry (if mountpoint). */
  int                       d_count;      /* Usage count. */
};
/* Allocates a dentry. */
vfs_dentry_t * vfs_dentry_alloc();
/* Destroys a dentry. */
int vfs_dentry_destroy(vfs_dentry_t *dentry);
/* Registers a dentry. */
int vfs_dentry_add(vfs_dentry_t *dentry);
/* Unregister a denty. */
int vfs_dentry_release(vfs_dentry_t *dentry);
/* Looks a dentry up. */
// TODO: int vfs_dentry_lookup(?);

/* NOTE: HERE. */

/* Open file. */
struct vfs_file {
  int                     f_count;  /* How many descriptors reference this open
                                     * file. */
  int                     f_flags;  /* Flags used when opening the file. */
  mode_t                  f_mode;   /* Mode (used when file is created?). */
  loff_t                  f_pos;    /* Current offset in this file. */
  vfs_file_operations_t * f_ops;    /* File operations. */
  vfs_vnode_t           * f_vnode;  /* Pointer to the backing vnode. */
  void                  * f_private_data; /* Pointer to private data associated
                                           * open file if needed. */
};

/* File operations. Not all the functions must be defined. When set to NULL
 * the vfs will provide a default behavior. */
struct vfs_file_operations {
  /* Creates a new _file_ from _node_. */
  int (f_open *) (vfs_node_t *node, vfs_file_t *file);
  /* Releases the file. Only called when reference count reaches 0. */
  int (f_release *) (vfs_node_t *node, vfs_file_t *file);
  /* Called each time a file is closed. */
  int (f_flush *) (vfs_file_t *file);
  /* Read _count_ bytes from _file_ starting at _off_ into _buf_. */
  ssize_t (f_read *) (vfs_file_t *file, char *buf, size_t count, loff_t off);
  /* Write _count_ bytes into _file_ starting at _off_ from _buf_. */
  ssize_t (f_write *) (fvs_file_t *file, char *buf, size_t count, loff_t off);
  /* Update _file_.f_pos to _off_ relative to _origin_. */
  loff_t (f_lseek *) (vfs_file_t *file, loff_t off, int origin);
  /* Performs ioctl _cmd_ with _arg_ on _vnode_ via _file_. */
  int (f_ioctl *) (vfs_node_t *vnode, vfs_file_t *file, int cmd, void *arg);
};





/* Kernel use only. */
int vfs_init();

/* Filesystem API. */

/* Allocates a new filesystem structure. It does not register it. */
vfs_fs_t * vfs_fs_alloc(char *name);
/* Releases a filesystem structure if it's not in use. */
int vfs_fs_release(vfs_fs_t *);


int vfs_fs_lookup(vfs_fs_t *, vfs_dentry_t *);

int vfs_mount(vfs_fs_t *fs);
int vfs_unmount(vfs_fs_t *fs);

/* VFS API. */
int vfs_open(vfs_vnode_t * vnode, vfs_file_t *file);
int vfs_read(vfs_file_t *file, char *buf, size_t count);
int vfs_write(vfs_file_t *file, char *buf, size_t count);
int vfs_lseek(vfs_file_t *file, loff_t off, int origin);
int vfs_close(vfs_file_t *file);


#endif
