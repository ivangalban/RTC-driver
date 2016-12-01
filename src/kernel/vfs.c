/* We'll have two different trees. The first is to hold the actual filesystem
 * entities, i.e. filesytem types, superblocks, vnodes and open files.
 *
 *                    +-----+-----+---------------------------------------
 *  filesystem types: | ft1 | ft2 |
 *                    +--|--+--|--+---------------------------------------
 *                       |     +-----+
 *                       +------+    |
 *                       |      |    |
 *                    +--|--+---|-+--|--+---------------------------------
 *  superblocks:      | sb1 | sb2 | sb3 |
 *                    +--|--+-----+-----+---------------------------------
 *                       |
 *                       +-----+-----+
 *                       |     |     |
 *                    +--|--+--|--+--|--+---------------------------------
 *  vnodes:           | vn1 | vn2 | vn3 |
 *                    +--|--+--|--+--|--+---------------------------------
 *                       |     |     |
 *                       |     |     +-----+
 *                       |     |     |     |
 *                    +--|--+--|--+--|--+--|--+---------------------------
 *  open files:       | of1 | of2 | of3 | of4 |
 *                    +-----+-----+-----+-----+---------------------------
 *
 * All filesystem types will be stored, whether there is a superblock of this
 * type or not. The same will happen to superblocks, regardless of whether
 * they are mounted or not and whether there are vnodes belonging to it in use
 * or not. However, vnodes will only be stored when there is at least an open
 * file referencing it, meaning we won't cache vnodes and we will release them
 * as soon as their reference counter reaches zero.
 *
 * Then we'll have another tree, the dentries tree, which we will use to
 * hold paths and which will work as a path cache. Since superblocks will
 * always be present dentries will hold pointers to the superblocks they belong
 * to and, in case of being a mountpoint, to the superblocks they are the
 * mountpoint of. However, since vnodes are only present when they are in use,
 * dentries will hold only the vnode number.
 *
 *                     *sb1  (vn1) (vn2) (vn3) (vn4)
 *                       |     |     |     |     |
 *                    +--|--+--|--+--|--+--|--+--|--+---------------------
 *  dentries:         | dt1 | dt2 | dt3 | dt4 | dt5 |
 *                    +--|--+-|-|-+-|---+-|-|-+-|---+---------------------
 *                       |    | |   |     | |   |
 *                       +<---+ |   |     | +<--+
 *                       +<-----|---+     |
 *                              +<--------+
 */

#include <vfs.h>
#include <list.h>
#include <mem.h>
#include <fs/rootfs.h>
#include <string.h>
#include <errors.h>

#define VFS_MAX_FILES             1024
#define VFS_DEFAULT_BLK_SIZE      1024
#define VFS_SET_FILE_TYPE(m, t)   (((m) & 0x0fff) | ((t) & 0xf000))


/* Head of the whole VFS tree. */
static vfs_dentry_t * vfs_root_dentry;

/*****************************************************************************/
/* Filesystem types **********************************************************/
/*****************************************************************************/

/* Filesystem types. All registered filesystem types will be here. They are
 * identified by their names (e.g. "rootfs", "devfs", "minix"). */
static list_t vfs_fs_types;

/* Comparison function for fs_types. */
static int vfs_fs_types_cmp(void *item, void *ft_name) {
  return ! strcmp(((vfs_fs_type_t *)item)->ro.ft_name, (char *)ft_name);
}

/* Filesystem type lookup. */
static vfs_fs_type_t * vfs_fs_type_lookup(char *name) {
  return (vfs_fs_type_t *)list_find(&vfs_fs_types,
                                    vfs_fs_types_cmp,
                                    name);
}

/* Allocates a new filesystem type with default values. */
static vfs_fs_type_t * vfs_fs_type_alloc(char *name) {
  vfs_fs_type_t * ft;

  ft = (vfs_fs_type_t *)kalloc(sizeof(vfs_fs_type_t));
  if (ft == NULL)
    return NULL;

  if (list_add(&vfs_fs_types, ft) == -1) {
    kfree(ft);
    set_errno(E_NOMEM);
    return NULL;
  }

  ft->ro.ft_name = (char *)kalloc(strlen(name) + 1);
  if (ft->ro.ft_name == NULL) {
    list_find_del(&vfs_fs_types, vfs_fs_types_cmp, &(ft->ro.ft_name));
    kfree(ft);
    set_errno(E_NOMEM);
    return NULL;
  }
  strcpy(ft->ro.ft_name, name);

  ft->ft_ops.ft_get_sb = NULL;
  ft->ft_ops.ft_kill_sb = NULL;

  return ft;
}

/* Removes a filesystem type. */
static int vfs_fs_type_dealloc(vfs_fs_type_t *ft) {
  /* Remove it from the list. */
  list_find_del(&vfs_fs_types, vfs_fs_types_cmp, &(ft->ro.ft_name));

  /* Free the name. */
  if (ft->ro.ft_name != NULL) {
    kfree(ft->ro.ft_name);
  }

  /* Free it. */
  kfree(ft);
  return 0;
}

/* Registers a filesystem type. */
int vfs_fs_type_register(char *name, vfs_fs_type_config_t config) {
  vfs_fs_type_t *ft;

  ft = vfs_fs_type_lookup(name);
  if (ft != NULL) {
    set_errno(E_EXIST);
    return -1;
  }

  ft = vfs_fs_type_alloc(name);
  if (ft == NULL) {
    /* errno was set by _alloc(). */
    return -1;
  }

  if (config(ft) == -1) {
    vfs_fs_type_dealloc(ft);
    set_errno(E_IO);
    return -1;
  }

  return 0;
}

/*****************************************************************************/
/* Superblocks ***************************************************************/
/*****************************************************************************/

/* Superblocks. All registered superblocks will be here. They will be
 * identified by the device id of the device holding the filesystem. */
static list_t vfs_sbs;

/* Superblock comparison function. */
static int vfs_sb_cmp(void *item, void *devid) {
  return ((vfs_sb_t *)item)->ro.sb_devid == *((dev_t *)devid);
}

/* Looks a superblock up. */
static vfs_sb_t * vfs_sb_lookup(dev_t devid) {
  return (vfs_sb_t *)list_find(&vfs_sbs, vfs_sb_cmp, &devid);
}

/* Creates a superblock and adds it to the list. */
static vfs_sb_t * vfs_sb_alloc(dev_t devid) {
  vfs_sb_t *sb;

  sb = (vfs_sb_t *)kalloc(sizeof(vfs_sb_t));
  if (sb == NULL)
    return NULL;

  if (list_add(&vfs_sbs, sb) == -1) {
    kfree(sb);
    return NULL;
  }

  /* Set default values. */
  sb->sb_blocksize = VFS_DEFAULT_BLK_SIZE;
  sb->sb_blocks = 0;
  sb->sb_max_bytes = 0;
  sb->sb_flags = VFS_SB_F_UNUSED;

  sb->sb_ops.delete_vnode = NULL;
  sb->sb_ops.read_vnode = NULL;
  sb->sb_ops.write_vnode = NULL;
  sb->sb_ops.delete_vnode = NULL;
  sb->sb_ops.mount = NULL;
  sb->sb_ops.unmount = NULL;

  sb->sb_root_vno = 0;
  sb->private_data = NULL;

  sb->ro.sb_devid = devid;
  sb->ro.sb_fs_type = NULL;   /* It will be set when the discovery works. */

  sb->ro.sb_mnt = NULL;

  return sb;
}

/* Deallocs a superblock. */
static int vfs_sb_dealloc(vfs_sb_t *sb) {
  /* Tell the filesystem type the superblock is about to be released. */
  if (sb->ro.sb_fs_type->ft_ops.ft_kill_sb != NULL) {
    if (sb->ro.sb_fs_type->ft_ops.ft_kill_sb(sb) == -1) {
      set_errno(E_IO);
      return -1;
    }
  }

  /* Remove it from the list of superblocks. */
  list_find_del(&vfs_sbs, vfs_sb_cmp, &(sb->ro.sb_devid));

  /* Release the memory. */
  kfree(sb);

  return 0;
}

/*****************************************************************************/
/* Dentries ******************************************************************/
/*****************************************************************************/

/* Dentries. All dentries will be stored here. When paths are traversed
 * dentries are added to the list, but their reference counters are only
 * modified when vnodes are allocated or deallocated. Dentries are not
 * strictly necessary, they exist only for performance purposes, so there is
 * no real problem to release them even when there are vnodes pointed by them.
 * However, with mountpoints the situation is different since dentries are
 * the only way we use to keep track of mounted filesystems. Thus, dentries
 * acting as mountpoint can not be evicted until the superblock they are the
 * mountpoint of is unmonted. This lets us simply use a traditional array
 * instead of a slower list. We keep a counter for every entry just to know
 * what are the best entries to evict when there's no room for a new dentry.
 * We identify empty spaces by checking the d_name field to be NULL because
 * all dentries must have a name. And the eviction algorithm is Least
 * Frecuently Used (LFU), which I know is not the best but is simple to
 * implement. */
#define VFS_MAX_DENTRIES        100
static vfs_dentry_t vfs_dentries[VFS_MAX_DENTRIES]; /* Dentries are 24 bytes
                                                     * long, thus this will
                                                     * make the kernel just
                                                     * 2400 bytes larger. */

/* Resets a dentry to a initial, empty state. */
static void vfs_dentry_reset(vfs_dentry_t *dentry) {
  if (dentry->d_name != NULL)
    kfree(dentry->d_name);

  memset(dentry, 0, sizeof(vfs_dentry_t));
}

/* Allocates a dentry in the cache. */
static vfs_dentry_t * vfs_dentry_get(vfs_dentry_t *parent, char *name) {
  int i;
  int lowest_count, lowest_index;
  vfs_dentry_t *d;

  /* Look for the wanted dentry and in the meantime compute where to store a
   * new dentry in case we need it. */
  for (i = 0, lowest_index = -1; i < VFS_MAX_DENTRIES; i ++) {
    /* If the slot if free this is the one we'll be using in case we don't
     * find the dentry. */
    if (vfs_dentries[i].d_name == NULL) {
      lowest_index = i;
      lowest_count = 0; /* Nothing will be smaller than zero. */
      continue;
    }
    /* This is the wanted dentry, it exists. */
    if (vfs_dentries[i].ro.d_parent == parent &&
        strcmp(vfs_dentries[i].d_name, name) == 0) {
      /* Increment the counter. */
      vfs_dentries[i].ro.d_count ++;
      return vfs_dentries + i;
    }
    /* This is not the one we want but it is a mountpoint, thus it is
     * untouchable. */
    if (vfs_dentries[i].ro.d_mnt_sb != NULL) {
      continue;
    }
    /* This is just normal dentry. Check whether it has the lowest reference
     * counter. */
    if (lowest_index == -1 || vfs_dentries[i].ro.d_count < lowest_count) {
      lowest_index = i;
      lowest_count = vfs_dentries[i].ro.d_count;
    }
  }

  /* If we got here the entry was not found, thus we need to create a new
   * one. */
  if (lowest_index == -1) {
    /* What? Do we really reached the VFS_MAX_DENTRIES mountpoints? How? */
    set_errno(E_LIMIT);
    return NULL;
  }

  /* Ok, set the dentry. */
  d = vfs_dentries + lowest_index;
  vfs_dentry_reset(d);

  d->d_name = (char *)kalloc(strlen(name) + 1);
  if (d->d_name == NULL) {
    set_errno(E_NOMEM);
    return NULL;
  }
  strcpy(d->d_name, name);
  d->ro.d_parent = parent;

  /* Set the superblock this dentry belongs to. */
  if (d->ro.d_parent == NULL) {
    /* This is "/". */
    d->ro.d_sb = NULL;
  }
  else if (d->ro.d_parent->ro.d_mnt_sb == NULL) {
    /* Parent dentry is not a mountpoint, therefore we belong to the same
     * superblock. */
    d->ro.d_sb = d->ro.d_parent->ro.d_sb;
  }
  else {
    /* Parent is a mountpoint, therefore this entry's superblock is the
     * mounted by parent. */
     d->ro.d_sb = d->ro.d_parent->ro.d_mnt_sb;
  }

  /* And since someone requested it, it's count is 1, right? */
  d->ro.d_count = 1;

  return d;
}


/* When a superblock is unmounted all dentries belonging to this superblock
 * must be removed. */
static int vfs_dentry_unmount_sb(vfs_sb_t *sb) {
  int i;

  /* First, check whether we can unmount the superblock. */
  for (i = 0; i < VFS_MAX_DENTRIES; i ++) {
    if (vfs_dentries[i].d_name == NULL) {
      continue;
    }
    if (vfs_dentries[i].ro.d_mnt_sb != NULL &&
        vfs_dentries[i].ro.d_sb == sb) {
      return -1;
    }
  }

  /* Then do the unmount. */
  for (i = 0; i < VFS_MAX_DENTRIES; i ++) {
    if (vfs_dentries[i].d_name == NULL) {
      continue;
    }
    if (vfs_dentries[i].ro.d_sb == sb) {
      vfs_dentry_reset(vfs_dentries + i);
    }
  }

  return 0;
}

/*****************************************************************************/
/* vnodes ********************************************************************/
/*****************************************************************************/

/* We won't use a static approach for vnodes because vnodes are larger
 * structures. However, we must put some limits here. */
#define VFS_MAX_VNODES        1024

/* VNodes. */
static list_t vfs_vnodes;

/* vnode key in the cache. */
typedef struct vfs_vnode_key {
  vfs_sb_t    * v_sb;   /* Superblock this vnode belongs to. */
  int           v_no;   /* vnode number. */
} vfs_vnode_key_t;

/* vnodes comparison function to be used by the list. */
static int vfs_vnodes_cmp(void *item, void *key) {
  vfs_vnode_t *n;
  vfs_vnode_key_t *k;

  n = (vfs_vnode_t *)item;
  k = (vfs_vnode_key_t *)key;

  return n->v_no == k->v_no && n->ro.v_sb == k->v_sb;
}

/* vnodes lookup. */
static vfs_vnode_t * vfs_vnode_lookup(vfs_sb_t *sb, int v_no) {
  vfs_vnode_key_t k;
  k.v_sb = sb;
  k.v_no = v_no;
  return list_find(&vfs_vnodes, vfs_vnodes_cmp, &k);
}

/* Creates an empty vnode, not registered in the cache. We need this because
 * the create operation requires a vnode with no vnode number set, thus we
 * must provide a sort of temporary vnode which is not yet registered in the
 * cache. */
static vfs_vnode_t * vfs_vnode_prealloc(vfs_sb_t *sb) {
  vfs_vnode_t *v;

  v = (vfs_vnode_t *)kalloc(sizeof(vfs_vnode_t));
  if (v == NULL) {
    set_errno(E_NOMEM);
    return NULL;
  }

  v->v_no = 0;
  v->v_mode = 0;
  v->v_size = 0;
  v->v_dev = FILE_NODEV;

  /* TODO: Provide generic implementations if applicable. */
  v->v_iops.lookup = NULL;
  v->v_iops.create = NULL;
  v->v_iops.mkdir = NULL;
  v->v_iops.mknod = NULL;

  v->v_fops.open = NULL;
  v->v_fops.release = NULL;
  v->v_fops.flush = NULL;
  v->v_fops.read = NULL;
  v->v_fops.write = NULL;
  v->v_fops.lseek = NULL;
  v->v_fops.ioctl = NULL;

  v->ro.v_sb = sb;
  v->ro.v_count = 0;

  /* Since this node has no vnode number the filesystem might not be ready
   * to set private_data, that's why we just set it to NULL. Later superblock
   * operations like create or read will fill this field. */
  v->private_data = NULL;

  return v;
}

/* Registers a vnode in the vnode cache. */
static int vfs_vnode_alloc(vfs_vnode_t *node) {
  return list_add(&vfs_vnodes, node);
}

/* Destroys the vnode and takes it out the vnodes list if it was already there.
 * This allows destroying a node that has only been preallocated. */
static int vfs_vnode_dealloc(vfs_vnode_t *node) {
  vfs_vnode_t *n;
  vfs_vnode_key_t k;

  /* Try to remove the node from the list. */
  k.v_no = node->v_no;
  k.v_sb = node->ro.v_sb;
  n = list_find_del(&vfs_vnodes, vfs_vnodes_cmp, &k);

  /* Just double check we didn't screw something somewhere sometime. */
  if (n != NULL && n != node) {
    set_errno(E_CORRUPT);
    return -1;
  }

  kfree(node);
  return 0;
}

/* Acquires a vnode. */
static int vfs_vnode_acquire(vfs_vnode_t *node) {
  node->ro.v_count ++;
  return 0;
}

/* Releases a vnode. If the reference counter reaches zero the node will be
 * destroyed. */
static int vfs_vnode_release(vfs_vnode_t *node) {
  node->ro.v_count --;

  /* Ok, this one has to be destroyed. */
  if (node->ro.v_count < 1) {
    /* Tell the superblock this node is being destroyed. */
    if (node->ro.v_sb->sb_ops.destroy_vnode(node->ro.v_sb, node) == -1) {
      set_errno(E_IO);
      return -1;
    }
    /* And destroy it. */
    return vfs_vnode_dealloc(node);
  }
  return 0;
}

/* Load a vnode from cache or from superblock. This adds the node to the list
 * of nodes and acquires it. */
static vfs_vnode_t * vfs_vnode_get_or_read(vfs_sb_t *sb, int vno) {
  vfs_vnode_t *n;

  /* Look the node in the cache. */
  n = vfs_vnode_lookup(sb, vno);
  /* If not there, create it. */
  if (n == NULL) {
    /* Reserve space for the node. */
    n = vfs_vnode_prealloc(sb);
    if (n == NULL) {
      return NULL;
    }

    /* Set the required fields to ask the superblock to find it. */
    n->v_no = vno;
    n->ro.v_sb = sb;

    /* Ask the superblock to load it. */
    if (sb->sb_ops.read_vnode(sb, n) == -1) {
      vfs_vnode_dealloc(n);
      return NULL;
    }

    /* Register tne node in the cache. */
    if (vfs_vnode_alloc(n) == -1) {
      sb->sb_ops.destroy_vnode(sb, n);
      vfs_vnode_dealloc(n);
      return NULL;
    }
  }

  /* Acquire it. */
  vfs_vnode_acquire(n);
  return n;
}

/* vnodes comparison function to be used by the list when looking for a vnode
 * belonging to a given superblock. */
static int vfs_vnodes_sb_only_cmp(void *item, void *sb) {
  vfs_vnode_t *n;
  vfs_sb_t *s;

  n = (vfs_vnode_t *)item;
  s = (vfs_sb_t *)sb;

  return n->ro.v_sb == s;
}

/* Only checks whether there are vnodes left beloging the to-be-unmounted
 * superblock. */
static int vfs_vnode_unmount_sb(vfs_sb_t *sb) {
  if (list_find(&vfs_vnodes, vfs_vnodes_sb_only_cmp, sb) == NULL)
    return 0;
  return -1;
}

/*****************************************************************************/
/* Files *********************************************************************/
/*****************************************************************************/

/* Open files. */
static list_t vfs_files;

/* Unlike the rest of the structures, files are not meant to be globally
 * identified. Processes will have their own file descriptors table holding
 * pointers to the open file structures. Thus, the list we used for the other
 * objects is not suitable here but we'll use anyway for the sake of speed.
 * However, think of making this list double linked and holding the pointers
 * in the structure itself just as Linux kernel does. Now I see why they do
 * that. */
static int vfs_file_cmp(void *item, void *filp) {
  return item == filp;
}

/* Files are supposed to exist only to access nodes, thus we will require a
 * node to set some default values. This function doesn't acquires the node
 * though it seems it should, but since acquiring the node was probably done
 * when loading the node, let's avoid double acquiring it.  */
static vfs_file_t * vfs_file_open(vfs_vnode_t *node, int flags) {
  vfs_file_t *filp;
  int err;

  filp = (vfs_file_t *)kalloc(sizeof(vfs_file_t));
  if (filp == NULL) {
    set_errno(E_NOMEM);
    return NULL;
  }
  if (list_add(&vfs_files, filp) == -1) {
    kfree(filp);
    set_errno(E_NOMEM);
    return NULL;
  }

  /* Set the initial values. */
  filp->f_pos = 0;
  filp->f_flags = flags;

  filp->f_ops.open = node->v_fops.open;
  filp->f_ops.release = node->v_fops.release;
  filp->f_ops.flush = node->v_fops.flush;
  filp->f_ops.read = node->v_fops.read;
  filp->f_ops.write = node->v_fops.write;
  filp->f_ops.lseek = node->v_fops.lseek;
  filp->f_ops.ioctl = node->v_fops.ioctl;
  filp->f_ops.readdir = node->v_fops.readdir;

  filp->private_data = NULL;

  filp->ro.f_count = 1;
  filp->ro.f_vnode = node;

  /* Try to open the file if open is set. */
  if (filp->f_ops.open != NULL &&
      filp->f_ops.open(filp->ro.f_vnode, filp) == -1) {
    /* Remove it from list. */
    err = get_errno();
    list_find_del(&vfs_files, vfs_file_cmp, filp);
    kfree(filp);
    set_errno(err);
    return NULL;
  }

  return filp;
}

/* Remove and open file. */
static int vfs_file_close(vfs_file_t *filp) {
  vfs_vnode_t *n;

  /* Take it out the list. */
  if (list_find_del(&vfs_files, vfs_file_cmp, filp) == NULL) {
    set_errno(E_NOKOBJ);
    return -1;
  }

  n = filp->ro.f_vnode;

  /* Close the file. */
  if (filp->f_ops.flush != NULL)
    filp->f_ops.flush(filp);

  /* If this the last opened file on this vnode. */
  if (n->ro.v_count == 1 && filp->f_ops.release != NULL)
    filp->f_ops.release(n, filp);

  kfree(filp);

  vfs_vnode_release(n);

  return 0;
}

/*****************************************************************************/
/* Helpers *******************************************************************/
/*****************************************************************************/

/* Just a helper function to check the dentry type and load the backing node.
 * It's just here because I've found myself typing this too much. */
static vfs_vnode_t * vfs_node_from_dentry(vfs_dentry_t *d) {
  /* Get the backing node. */
  if (d->ro.d_mnt_sb == NULL) {
    return vfs_vnode_get_or_read(d->ro.d_sb, d->d_vno);
  }
  else {
    return vfs_vnode_get_or_read(d->ro.d_mnt_sb, d->ro.d_mnt_sb->sb_root_vno);
  }
}

static vfs_dentry_t * vfs_lookup(char *path);

/* Creating all kinds of files is quite similar, so we're going to factorize
 * it here. */
static int vfs_create_node(char *path, mode_t mode, dev_t devid) {
  char *parent_path, *name;
  vfs_dentry_t *parent, *dentry;
  vfs_vnode_t *parent_node;
  int r, err;

  /* TODO: Rememer to do sanitation. */

  /* Let's forbid trying to create "/". */
  if (strcmp(path, "/") == 0) {
    set_errno(E_ACCESS);
    return -1;
  }

  /* Copy the path since we're going to alter it. */
  parent_path = (char *)kalloc(strlen(path) + 1);
  if (parent_path == NULL) {
    set_errno(E_NOMEM);
    return -1;
  }
  strcpy(parent_path, path);

  /* Split parent path and name. */
  name = strrchr(parent_path, '/');
  *name = '\0';
  name ++;

  /* Load parent. */
  if (* parent_path == '\0')
    parent = vfs_root_dentry;
  else
    parent = vfs_lookup(parent_path);

  /* If parent doesn't exist, quit. */
  if (parent == NULL) {
    kfree(parent_path);
    /* lookup set errno. */
    return -1;
  }

  /* Check the cache. */
  dentry = vfs_dentry_get(parent, name);
  if (dentry == NULL) {
    kfree(parent_path);
    set_errno(E_NOMEM);
    return -1;
  }

  /* If dentry was already there and loaded we can quit. */
  if (dentry->d_vno != 0) {
    kfree(parent_path);
    set_errno(E_EXIST);
    return -1;
  }

  /* Load parent node. */
  parent_node = vfs_node_from_dentry(parent);
  if (parent_node == NULL) {
    vfs_dentry_reset(dentry);
    kfree(parent_path);
    /* something really bad happened. */
    return -1;
  }

  /* Check parent is a directory. */
  if (FILE_TYPE(parent_node->v_mode) != FILE_TYPE_DIRECTORY) {
    vfs_vnode_release(parent_node);
    vfs_dentry_reset(dentry);
    kfree(parent_path);
    set_errno(E_NODIR);
    return -1;
  }

  switch (FILE_TYPE(mode)) {
    case FILE_TYPE_DIRECTORY:
      r = parent_node->v_iops.mkdir(parent_node, dentry, mode);
      break;
    case FILE_TYPE_REGULAR:
      r = parent_node->v_iops.create(parent_node, dentry, mode);
      break;
    case FILE_TYPE_CHAR_DEV:
    case FILE_TYPE_BLOCK_DEV:
    case FILE_TYPE_SOCKET:
    case FILE_TYPE_FIFO:
      r = parent_node->v_iops.mknod(parent_node, dentry, mode, devid);
      break;
    case FILE_TYPE_SYMLINK:
      set_errno(E_NOTIMP);
      r = -1;
      break;
    default:
      set_errno(E_INVAL);
      r = -1;
      break;
  }

  /* Request the dir creation. */
  if (r == -1) {
    err = get_errno();
    vfs_vnode_release(parent_node);
    vfs_dentry_reset(dentry);
    kfree(parent_path);
    set_errno(err);
    return -1;
  }

  /* Release the parent node because we are done with it. */
  vfs_vnode_release(parent_node);
  kfree(parent_path);

  return 0;
}


/*****************************************************************************/
/* Lookup ********************************************************************/
/*****************************************************************************/

/* Looks up a path. */
static vfs_dentry_t * vfs_lookup(char *path) {
  char *tmp_path, *path_comp;
  vfs_dentry_t *parent_dentry, *obj;
  vfs_vnode_t *parent_node;
  int err;

  /* If nothing is mounted just quit. */
  if (vfs_root_dentry == NULL) {
    set_errno(E_NOENT);
    return NULL;
  }

  /* TODO: Do some sanitation to paths. We assume path starts with "/". */

  /* Clone the path to work with it but not not altering the path received. */
  tmp_path = (char *)kalloc(strlen(path) + 1);
  if (tmp_path == NULL) {
    set_errno(E_NOMEM);
    return NULL;
  }
  strcpy(tmp_path, path);

  /* Load root dentry. */
  obj = vfs_root_dentry;
  parent_dentry = vfs_root_dentry;
  path_comp = strtok(tmp_path, '/');

  /* Go down until you find a the objective. */
  while (path_comp != NULL) {
    /* First, scan the dentry cache in case it's already loaded. */
    obj = vfs_dentry_get(parent_dentry, path_comp);
    if (obj == NULL) {
      set_errno(E_LIMIT); /* This should not happen unless we have reached a
                           * hundred mountpoints. */
      kfree(tmp_path);
      return NULL;
    }

    /* Since vfs_dentry_get always returns a dentry we need to check whether
     * it's a new one or not. If a dentry was found then it has a node number,
     * whether it be a normal dentry or a mountpoint because mountpoints must
     * exist as directories in their own superblocks. So, if it's a new one
     * its ino will be 0. */
    if (obj->d_vno == 0) {
      /* Load the node associated with the parent dentry. This depends on the
       * dentry type. */
      parent_node = vfs_node_from_dentry(parent_dentry);

      /* If the node doesn't exist just quit. */
      if (parent_node == NULL) {
        vfs_dentry_reset(obj);
        kfree(tmp_path);
        set_errno(E_CORRUPT);
        return NULL;
      }

      /* Check the node we got is indeed a directory. */
      if (FILE_TYPE(parent_node->v_mode) != FILE_TYPE_DIRECTORY) {
        vfs_dentry_reset(obj);
        vfs_vnode_release(parent_node);
        kfree(tmp_path);
        set_errno(E_NODIR);
        return NULL;
      }

      /* Ask the node to lookup a dentry with the given name. */
      if (parent_node->v_iops.lookup(parent_node, obj) == -1) {
        err = get_errno();
        vfs_dentry_reset(obj);
        vfs_vnode_release(parent_node);
        kfree(tmp_path);
        set_errno(err);
        return NULL;
      }

      /* It seems everything went well, so we need the vnode no more. */
      vfs_vnode_release(parent_node);
    }

    /* At this point we have our objective correctly loaded in obj. Let's
     * set parent_dentry to obj in case we need to keep going. */
    parent_dentry = obj;
    path_comp = strtok(NULL, '/');
  }

  /* Here, whatever we have in hand is our goal, isn't it? */
  return obj;
}


/*****************************************************************************/
/* Modules API ***************************************************************/
/*****************************************************************************/

/* Initialize the vfs. */
int vfs_init() {
  list_init(&vfs_fs_types);
  list_init(&vfs_sbs);
  list_init(&vfs_vnodes);
  list_init(&vfs_files);
  memset(&vfs_dentries, 0, sizeof(vfs_dentry_t) * VFS_MAX_DENTRIES);

  vfs_root_dentry = NULL;

  return 0;
}

/*****************************************************************************/
/* Public VFS API ************************************************************/
/*****************************************************************************/

/* Mounts device devid in path using filesystem type fs_type. */
int vfs_mount(dev_t devid, char *path, char *fs_type) {
  vfs_fs_type_t *ft;
  vfs_vnode_t *n;
  vfs_sb_t *sb;
  vfs_dentry_t *d;

  /* Check the validity of the path to be used as mountpoint. */

  if (vfs_root_dentry == NULL) {
    /* Nothing is mounted on "/", therefore nothing is mounted. */
    if (strcmp(path, "/") != 0) {
      /* Trying to mount something not in "/" with no mounted "/". */
      set_errno(E_NOROOT);
      return -1;
    }
    else {
      /* Trying to mount something is "/" when "/" is not mounted. */
      d = vfs_dentry_get(NULL, "/");
      if (d == NULL) {
        set_errno(E_NOMEM);
        return -1;
      }
      d->d_vno = 1; /* This is just to avoid the initial root with no backing
                     * node breaks the system. Really? */
    }
  }
  else {
    /* There exists something mounted on "/". */
    if (strcmp(path, "/") != 0) {
      /* Find the dentry this path resolves to because it is not "/". */
      d = vfs_lookup(path);
      if (d == NULL) {
        set_errno(E_NOENT);
        return -1;
      }
      /* Check this dentry is not already a mountpoint. */
      if (d->ro.d_mnt_sb != NULL) {
        set_errno(E_ACCESS);  /* TODO: Eventually let's remove this stupid
                               *       restriction. */
        return -1;
      }
      /* Get the node associated to this dentry. */
      n = vfs_vnode_get_or_read(d->ro.d_sb, d->d_vno);
      if (n == NULL) {
        set_errno(E_CORRUPT);
        return -1;
      }
      /* Check it's type: we can only allow mounting partitions on
       * directories. */
      if (FILE_TYPE(n->v_mode) != FILE_TYPE_DIRECTORY) {
        set_errno(E_NODIR);
        return -1;
      }
      /* If we got here, d is a valid mountpoint. */
    }
    else {
      /* We want to remount "/". */
      /* TODO: Next step is to do this. */
      set_errno(E_NOTIMP);
      return -1;
    }
  }

  /* Get the filesystem type. */
  ft = vfs_fs_type_lookup(fs_type);
  if (ft == NULL) {
    vfs_dentry_reset(d);
    set_errno(E_NOKOBJ);
    return -1;
  }

  /* Check the superblock is not already mounted. */
  sb = vfs_sb_lookup(devid);
  if (sb != NULL) {
    vfs_dentry_reset(d);
    set_errno(E_MOUNTED);
    return -1;
  }

  /* Prepare a superblock. */
  sb = vfs_sb_alloc(devid);
  if (sb == NULL) {
    vfs_dentry_reset(d);
    set_errno(E_NOMEM);
    return -1;
  }

  /* Probe the filesystem type to handle this device. */
  if (ft->ft_ops.ft_get_sb(sb) == -1) {
    vfs_sb_dealloc(sb);
    vfs_dentry_reset(d);
    set_errno(E_INVFS);
    return -1;
  }

  /* Link together the fs_type and its sb. */
  sb->ro.sb_fs_type = ft;

  /* Attempt mount. */
  if (sb->sb_ops.mount(sb) == -1) {
    ft->ft_ops.ft_kill_sb(sb);
    vfs_sb_dealloc(sb);
    vfs_dentry_reset(d);
    set_errno(E_IO);
    return -1;
  }

  /* Link dentry and mountpoint. */
  d->ro.d_mnt_sb = sb;

  /* TODO: This is not necessarily the best approach. */
  if (d->ro.d_parent == NULL) {
    vfs_root_dentry = d;
  }

  return 0;
}

/* Does stat. */
int vfs_stat(char *path, struct stat *stat) {
  vfs_dentry_t *d;
  vfs_vnode_t *n;

  d = vfs_lookup(path);
  if (d == NULL) {
    /* errno was already set. */
    return -1;
  }

  /* Get the backing node. */
  n = vfs_node_from_dentry(d);

  if (n == NULL) {
    /* errno was already set. */
    return -1;
  }

  stat->ino = n->v_no;
  stat->size = n->v_size;
  stat->mode = n->v_mode;
  stat->dev = n->v_dev;

  vfs_vnode_release(n);

  return 0;
}

/* Create a directory. */
int vfs_mkdir(char *path, mode_t mode) {
  /* Just avoid problems with the mode. */
  return vfs_create_node(path,
                         VFS_SET_FILE_TYPE(mode, FILE_TYPE_DIRECTORY),
                         FILE_NODEV);
}

/* mknod. */
int vfs_mknod(char *path, mode_t mode, dev_t dev) {
  switch (FILE_TYPE(mode)) {
    case FILE_TYPE_DIRECTORY:
    case FILE_TYPE_REGULAR:
    case FILE_TYPE_SYMLINK:
      set_errno(E_INVAL);
      return -1;
    break;
  }
  /* Just avoid problems with the mode. */
  return vfs_create_node(path, mode, dev);
}

/* Opens a file. */
vfs_file_t * vfs_open(char *path, int flags, mode_t mode) {
  vfs_vnode_t *node;
  vfs_dentry_t *dentry;
  vfs_file_t *filp;
  int err;

  /* Start by looking the file. */
  dentry = vfs_lookup(path);
  /* If the file was found but opened with forced creation */
  if (dentry != NULL && flags & FILE_O_CREATE && flags & FILE_O_EXCL) {
    set_errno(E_EXIST);
    return NULL;
  }
  /* If the file was not found but creation is permitted */
  if (dentry == NULL && flags & FILE_O_CREATE) {
    /* Attempt to create the file. */
    if (vfs_create_node(path,
                        VFS_SET_FILE_TYPE(mode, FILE_TYPE_REGULAR),
                        FILE_NODEV) == -1) {
      /* errno was set. */
      return NULL;
    }
    /* And try to pick dentry again. */
    dentry = vfs_lookup(path);
  }

  /* Ok, do we got a file? */
  if (dentry == NULL) {
    /* errno was set by either vfs_lookup above. */
    return NULL;
  }

  /* Get the node. */
  node = vfs_node_from_dentry(dentry);
  if (node == NULL) {
    set_errno(E_CORRUPT);
    return NULL;
  }

  /* Check what actions we can do. */
  if ((flags & FILE_O_READ) && (node->v_fops.read == NULL)) {
    vfs_vnode_release(node);
    set_errno(E_ACCESS);
    return NULL;
  }
  if ((flags & FILE_O_WRITE) && (node->v_fops.write == NULL)) {
    vfs_vnode_release(node);
    set_errno(E_ACCESS);
    return NULL;
  }

  /* TODO: Now we'll assume there's only root and he owns everything. Thus,
   *       we'll check only user permissions. */
  if ((flags & FILE_O_READ) && !(node->v_mode & FILE_PERM_USR_READ)) {
    vfs_vnode_release(node);
    set_errno(E_ACCESS);
    return NULL;
  }
  if ((flags & FILE_O_WRITE) && !(node->v_mode & FILE_PERM_USR_WRITE)) {
    vfs_vnode_release(node);
    set_errno(E_ACCESS);
    return NULL;
  }

  /* Implement O_TRUNC */

  /* Open the file. */
  filp = vfs_file_open(node, flags);
  if (filp == NULL) {
    err = get_errno();
    vfs_vnode_release(node);
    set_errno(err);
    return NULL;
  }

  return filp;
}

/* Write. */
ssize_t vfs_write(vfs_file_t *filp, void *buf, size_t count) {
  if ((filp->f_flags & FILE_O_WRITE) == 0) {
    set_errno(E_BADFD);
    return -1;
  }
  return filp->f_ops.write(filp, (char *)buf, count);
}

/* Read. */
ssize_t vfs_read(vfs_file_t *filp, void *buf, size_t count) {
  if ((filp->f_flags & FILE_O_READ) == 0) {
    set_errno(E_BADFD);
    return -1;
  }
  return filp->f_ops.read(filp, (char *)buf, count);
}

/* lseek */
off_t vfs_lseek(vfs_file_t *filp, off_t off, int whence) {
  if (whence != SEEK_SET && whence != SEEK_END && whence != SEEK_CUR) {
    set_errno(E_INVAL);
    return -1;
  }

  if (filp->f_ops.lseek != NULL) {
    return filp->f_ops.lseek(filp, off, whence);
  }
  switch (whence) {
    case SEEK_SET:
      filp->f_pos = off;
      break;
    case SEEK_CUR:
      filp->f_pos += off;
      break;
    case SEEK_END:
      filp->f_pos = filp->ro.f_vnode->v_size + off;
      break;
  }
  return filp->f_pos;
}

/* Unmounts a mounted superblock. */
int vfs_sb_unmount(vfs_sb_t *sb) {
  vfs_dentry_t *mp;

  /* Check it is mounted. */
  if (!VFS_SB_IS_MOUNTED(sb)) {
    set_errno(E_NOTMOUNTED);
    return -1;
  }

  /* Tell the vnodes table to unmount. It will fail if there is still a open
   * file there. */
  if (vfs_vnode_unmount_sb(sb) == -1) {
    set_errno(E_CORRUPT);
    return -1;
  }

  /* Tell the dentries cache to invalidate all dentries belonging to sb. Notice
   * that the dentry pointing to is doesn't belong to sb but to the parent sb.
   * Thus it doesn't need to be invalidated because it exists as a directory
   * in its own superblock. */
  if (vfs_dentry_unmount_sb(sb) == -1) {
    set_errno(E_BUSY);
    return -1;
  }

  /* Inform the superblock it will be unmounted. */
  if (sb->sb_ops.unmount(sb) == -1) {
    set_errno(E_IO);
    return -1;
  }

  /* Get the mountpoint and remove the mountpoint field. */
  mp = sb->ro.sb_mnt;
  mp->ro.d_mnt_sb = NULL;

  /* Unmount the sb.  */
  sb->sb_flags = VFS_SB_F_UNUSED;
  sb->ro.sb_mnt = NULL;

  return 0;
}
