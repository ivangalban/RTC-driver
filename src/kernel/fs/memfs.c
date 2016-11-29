#include <vfs.h>
#include <string.h>
#include <errors.h>
#include <mem.h>
#include <fs/memfs.h>

#define MEMFS_ROOT_INO      1
#define MEMFS_MAX_FS        5 /* Increase this in case of need. */

/*****************************************************************************/
/* Internals *****************************************************************/
/*****************************************************************************/

/* Superblock. */
typedef struct memfs_super {
  char                * name;     /* Name of the filesytem type. */
  dev_t                 devid;    /* Fake devid. */
  int                   flags;    /* Flags. */
  list_t                nodes;    /* Nodes. */
  int                   last_ino; /* Last assigned inode. */
} memfs_super_t;

/* Nodes. */
typedef struct memfs_node {
  int                   ino;      /* inode number. */
  mode_t                mode;     /* mode. */
  size_t                size;     /* size. */
  dev_t                 devid;    /* devid if device. */
  char                * data;     /* Data */
  list_t                dentries; /* Dentries if directory. */
  memfs_super_t       * super;    /* Super this node belongs to. */
} memfs_node_t;

/* Dentries. */
typedef struct memfs_dentry {
  int                   ino;      /* inode number */
  char                * name;     /* dentry name. */
  memfs_node_t        * dir;      /* dir holding this entry. */
} memfs_dentry_t;


/* All filesystems we manage. */
static memfs_super_t memfs_supers[MEMFS_MAX_FS];


/* dentry comparison function */
static int memfs_dentry_cmp(void *item, void *name) {
  memfs_dentry_t *dentry;

  dentry = (memfs_dentry_t *)item;
  return strcmp(dentry->name, (char *)name) == 0;
}

/* Allocates a dentry in a node. */
static memfs_dentry_t * memfs_dentry_alloc(memfs_node_t *node,
                                           int ino,
                                           char *name) {
  memfs_dentry_t *d;

  d = (memfs_dentry_t *)kalloc(sizeof(memfs_dentry_t));
  if (d == NULL)
    return NULL;

  d->ino = ino;
  d->dir = node;

  d->name = (char *)kalloc(sizeof(name) + 1);
  if (d->name == NULL) {
    kfree(d);
    return NULL;
  }
  strcpy(d->name, name);

  if (list_add(&(node->dentries), d) == -1) {
    kfree(d->name);
    kfree(d);
    return NULL;
  }

  return d;
}

/* Removes a dentry from its directory and from memory. */
static void memfs_dentry_dealloc(memfs_dentry_t *d) {
  list_find_del(&(d->dir->dentries), memfs_dentry_cmp, d->name);
  kfree(d->name);
  kfree(d);
}

/* node comparison function */
static int memfs_node_cmp(void *item, void *ino) {
  memfs_node_t *node;

  node = (memfs_node_t *)item;
  return node->ino == *((int *)ino);
}

/* Allocates a node in its super. */
static memfs_node_t * memfs_node_alloc(memfs_super_t *ms,
                                       mode_t mode,
                                       dev_t devid) {
  memfs_node_t *node;

  node = (memfs_node_t *)kalloc(sizeof(memfs_node_t));
  if (node == NULL)
    return NULL;

  node->ino = ms->last_ino ++;
  node->super = ms;

  if (list_add(&(ms->nodes), node) == -1) {
    kfree(node);
    return NULL;
  }

  node->mode = mode;
  node->devid = devid;
  node->data = NULL;
  list_init(&(node->dentries));

  return node;
}

/* Removes a node and it dentries from its super and from memory. */
static void memfs_node_dealloc(memfs_node_t *node) {
  list_find_del(&(node->super->nodes), memfs_node_cmp, &(node->ino));

  if (node->data != NULL)
    kfree(node->data);

  while (node->dentries.count > 0) {
    memfs_dentry_dealloc((memfs_dentry_t *)list_get(&(node->dentries), 0));
  }

  kfree(node);
}

/* Gets a registered memfs based on its devid. */
static memfs_super_t * memfs_get_super(dev_t devid) {
  int i;

  for (i = 0; i < MEMFS_MAX_FS; i ++) {
    if (memfs_supers[i].name == NULL)
      continue;
    if (memfs_supers[i].devid == devid)
      return memfs_supers + i;
  }
  return NULL;
}

/* Inits a super. */
static int memfs_init_super(memfs_super_t *ms,
                            char *name,
                            dev_t devid,
                            int flags) {
  ms->name = (char *)kalloc(strlen(name) + 1);
  if (ms->name == NULL) {
    return -1;
  }
  strcpy(ms->name, name);

  ms->devid = devid;
  ms->flags = flags;
  ms->last_ino = 0;

  list_init(&(ms->nodes));

  return 0;
}

/* Clears a super. */
static void memfs_clear_super(memfs_super_t *ms) {
  kfree(ms->name);
  ms->name = NULL;
  ms->devid = 0;
  ms->flags = 0;
  ms->last_ino = 0;

  while (ms->nodes.count > 0) {
    memfs_node_dealloc((memfs_node_t *)list_get(&(ms->nodes), 0));
  }
}

/*****************************************************************************/
/* files *********************************************************************/
/*****************************************************************************/

/* Opens vnode and fills file. Called every time the vnode is opened. */
int memfs_file_open(vfs_vnode_t *node, vfs_file_t *file) {
  /* Actually, we need to do nothing special here. */
  return 0;
}

/* Releases the vnode. Called when the all opened files referencing this
 * vnode are closed. The file passed is the last of them. */
int memfs_file_release(vfs_vnode_t *node, vfs_file_t *file) {
  /* Nothing special to do. */
  return 0;
}

/* Called each time a file is closed. */
int memfs_file_flush(vfs_file_t *file) {
  /* Nothing special to do. */
  return 0;
}

/* Read _count_ bytes from _file_ starting at _off_ into _buf_. */
ssize_t memfs_file_read(vfs_file_t *file, char *buf, size_t count, off_t off) {
  memfs_node_t *mn;

  mn = (memfs_node_t *)(file->ro.f_vnode->private_data);

  /* EOF */
  if (off >= mn->size)
    return 0;

  /* Put limits. */
  if (off + count > mn->size) {
    count = mn->size - off;
  }

  memcpy(buf, (char *)(mn->data) + off, count);

  return (ssize_t)count;
}

/* Write _count_ bytes into _file_ starting at _off_ from _buf_. */
ssize_t memfs_file_write(vfs_file_t *file,
                         char *buf,
                         size_t count,
                         off_t off) {
  memfs_node_t *mn;
  char *new_data;

  mn = (memfs_node_t *)(file->ro.f_vnode->private_data);

  if (off + count > mn->size) {
    new_data = (char *)kalloc((off + count) * sizeof(char));
    if (new_data == NULL) {
      set_errno(E_NOSPACE);
      return -1;
    }
    memcpy(new_data, mn->data, mn->size);
    kfree(mn->data);
    mn->data = new_data;
  }
  mn->size = off + count;

  memcpy(mn->data + off, buf, count);

  return (ssize_t)count;
}

/* Update _file_.f_pos to _off_ relative to _origin_. */
off_t memfs_file_lseek(vfs_file_t *file, off_t off, int origin) {
  return 0;
}

/* Reads the next name in a directory. This is quite different from Linux
 * interface. */
char * memfs_file_readdir(vfs_file_t *file) {
  memfs_node_t *mn;
  memfs_dentry_t *md;

  mn = (memfs_node_t *)(file->ro.f_vnode->private_data);
  md = list_get(&(mn->dentries), file->f_pos);

  if (md == NULL)
    return NULL;

  file->f_pos ++;
  return md->name;
}

/*****************************************************************************/
/* inodes ********************************************************************/
/*****************************************************************************/

/* Looks for a dentry with the given dentry name. The vnode is valid, but the
 * dentry will only have d_name set. If the dentry is found, the rest of the
 * data should be filled in. */
int memfs_ino_lookup(vfs_vnode_t *dir, vfs_dentry_t *dentry) {
  memfs_node_t *mn;
  memfs_dentry_t *md;

  mn = (memfs_node_t *)(dir->private_data);
  md = (memfs_dentry_t *)list_find(&(mn->dentries),
                                   memfs_dentry_cmp,
                                   dentry->d_name);
  if (md == NULL) {
    set_errno(E_NOENT);
    return -1;
  }

  dentry->d_vno = md->ino;

  return 0;
}

/* Create a new device file in dir. */
static int memfs_ino_mknod(vfs_vnode_t *dir,
                           vfs_dentry_t *dentry,
                           mode_t mode,
                           dev_t devid) {
  memfs_node_t *mdir, *mn;
  memfs_dentry_t *md;

  mdir = (memfs_node_t *)(dir->private_data);

  mn = memfs_node_alloc(mdir->super, mode, devid);
  if (mn == NULL) {
    set_errno(E_IO);
    return -1;
  }

  md = memfs_dentry_alloc(mdir, mn->ino, dentry->d_name);
  if (md == NULL) {
    memfs_node_dealloc(mn);
    set_errno(E_IO);
    return -1;
  }

  dentry->d_vno = mn->ino;

  return 0;
}

/* Creates a regular file in dir based on dentry which should be completely
 * filled. */
static int memfs_ino_create(vfs_vnode_t *dir,
                            vfs_dentry_t *dentry,
                            mode_t mode) {
  return memfs_ino_mknod(dir, dentry, mode, FILE_NODEV);
}

/* Creates a new directory in dir. */
static int memfs_ino_mkdir(vfs_vnode_t *dir,
                           vfs_dentry_t *dentry,
                           mode_t mode) {
  return memfs_ino_mknod(dir, dentry, mode, FILE_NODEV);
}


/*****************************************************************************/
/* superblock ****************************************************************/
/*****************************************************************************/

/* Reads a vnode from the filesystem into node. Only the vnode number is
 * suppossed to be set and ro.v_sb. */
static int memfs_sb_read_vnode(vfs_sb_t *sb, vfs_vnode_t *node) {
  memfs_super_t *ms;
  memfs_node_t *mn;

  ms = (memfs_super_t *)(sb->private_data);
  mn = (memfs_node_t *)list_find(&(ms->nodes), memfs_node_cmp, &(node->v_no));
  if (mn == NULL) {
    set_errno(E_NOENT);
    return -1;
  }

  node->v_mode = mn->mode;
  node->v_size = mn->size;
  node->v_dev = mn->devid;
  node->private_data = mn;

  /* Set the operations based on file type. */
  switch (FILE_TYPE(node->v_mode)) {
    case FILE_TYPE_DIRECTORY:
      /* Lookup is always valid because at least we need to lookup on "/". */
      node->v_iops.lookup = memfs_ino_lookup;

      /* Set the operations based on the allowed operations. */
      if (ms->flags & MEMFS_FLAGS_ALLOW_DIRS)
        node->v_iops.mkdir = memfs_ino_mkdir;

      if (ms->flags & MEMFS_FLAGS_ALLOW_FILES)
        node->v_iops.create = memfs_ino_create;

      if (ms->flags & MEMFS_FLAGS_ALLOW_NODES)
        node->v_iops.mknod = memfs_ino_mknod;

      node->v_fops.open = memfs_file_open;
      node->v_fops.release = memfs_file_release;
      node->v_fops.flush = memfs_file_flush;
      node->v_fops.readdir = memfs_file_readdir;
      break;
    case FILE_TYPE_REGULAR:
      node->v_fops.open = memfs_file_open;
      node->v_fops.release = memfs_file_release;
      node->v_fops.flush = memfs_file_flush;
      node->v_fops.read = memfs_file_read;
      node->v_fops.write = memfs_file_write;
      node->v_fops.lseek = memfs_file_lseek;
      break;
    case FILE_TYPE_FIFO:
    case FILE_TYPE_CHAR_DEV:
    case FILE_TYPE_BLOCK_DEV:
    case FILE_TYPE_SYMLINK:
    case FILE_TYPE_SOCKET:
    case FILE_TYPE_WHT:
      break;
  }

  return 0;
}

/* Called when the vnode is about to be deleted from memory. Usually used
 * to free private_data or do any operation the filesystem needs to.
 * NOTE: This is meant to physically delete nodes, just to release the
 * resources associated to a vnode that is no longer in use, for instance,
 * when the last open file referencing it is closed. If set to NULL it
 * just won't be called, which is normal in filesystems that don't need
 * to take any special action when this happens. */
static int memfs_sb_destroy_vnode(vfs_sb_t *sb, vfs_vnode_t *node) {
  /* TODO: Check what happens when open, delete, release. */
  return 0;
}

/* Writes a vnode into the filesystem. It is suppossed to be an existent
 * vnode, which means there should be a vnode number and the rest of the
 * data. Since data is manipulated via inode operations this is just meant
 * to save metadata. */
static int memfs_sb_write_vnode(vfs_sb_t *sb, vfs_vnode_t *node) {
  memfs_node_t *mn;

  mn = (memfs_node_t *)(node->private_data);

  mn->mode = node->v_mode;

  return 0;
}

/* Removes a vnode from the filesystem.
 * It's not necessary to release any data from the vnode structure since
 * destroy_vnode will be called inmediately after. */
static int memfs_sb_delete_vnode(vfs_sb_t *sb, vfs_vnode_t *node) {
  /* TODO: Do this. */
  return -1;
}

/* Used to notify the superblock it's being mounted. When called the
 * sb_mnt_root field will be partially set: the underlaying */
static int memfs_sb_mount(vfs_sb_t *sb) {
  return 0;
}

/* Used to nofity the superblock is being unmounted. */
static int memfs_sb_unmount(vfs_sb_t *sb) {
  return 0;
}


/*****************************************************************************/
/* fs_type *******************************************************************/
/*****************************************************************************/
static int memfs_ft_get_sb(vfs_sb_t *sb) {
  memfs_super_t *ms;

  ms = memfs_get_super(sb->ro.sb_devid);
  if (ms == NULL)
    return -1;

  /* This is the primary association. */
  sb->private_data = ms;

  sb->sb_ops.destroy_vnode = memfs_sb_destroy_vnode;
  sb->sb_ops.read_vnode = memfs_sb_read_vnode;
  sb->sb_ops.write_vnode = memfs_sb_write_vnode;
  sb->sb_ops.delete_vnode = memfs_sb_delete_vnode;
  sb->sb_ops.mount = memfs_sb_mount;
  sb->sb_ops.unmount = memfs_sb_unmount;

  return 0;
}

static int memfs_ft_kill_sb(vfs_sb_t *sb) {
  memfs_super_t * ms;

  ms = (memfs_super_t *)(sb->private_data);

  memfs_clear_super(ms);

  return 0;
}

static int memfs_config_fs_type(vfs_fs_type_t *ft) {
  ft->ft_ops.ft_get_sb = memfs_ft_get_sb;
  ft->ft_ops.ft_kill_sb = memfs_ft_kill_sb;
  return 0;
}

/*****************************************************************************/
/* memfs API *****************************************************************/
/*****************************************************************************/
int memfs_create(char *name, dev_t devid, int flags) {
  int i;

  if (memfs_get_super(devid) != NULL) {
    return -1;
  }

  for (i = 0; i < MEMFS_MAX_FS; i ++) {
    if (memfs_supers[i].name == NULL)
      break;
  }

  if (i == MEMFS_MAX_FS) {
    return -1;
  }

  if (memfs_init_super(memfs_supers + i, name, devid, flags) == -1)
    return -1;

  if (vfs_fs_type_register(name, memfs_config_fs_type) == -1) {
    memfs_clear_super(memfs_supers + i);
    return -1;
  }

  return 0;
}
