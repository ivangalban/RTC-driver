/* Common type definitions. */

#ifndef __TYPEDEF_H__
#define __TYPEDEF_H__

/****************************************/
/*    General definitions and types     */
/****************************************/

typedef unsigned char             u8;
typedef unsigned short            u16;
typedef unsigned int              u32;
typedef unsigned long long  int   u64;

typedef char                      s8;
typedef short                     s16;
typedef int                       s32;
typedef long long int             s64;

#define TRUE                      1
#define FALSE                     0

#define NULL                      (void*)0x0

typedef __builtin_va_list         va_list;
#define va_start(v, l)             __builtin_va_start(v, l)
#define va_end(v)                  __builtin_va_end(v)
#define va_arg(v, l)               __builtin_va_arg(v, l)

/**********************************************/
/*    Device related types and definitions    */
/**********************************************/
typedef u16                       dev_t;

#define DEV_MAJOR(dev)            (((dev) & 0xff00) >> 8)
#define DEV_MINOR(dev)            ((dev) & 0x00ff)

/*******************************************/
/*   File-related types and definitions.   */
/*******************************************/
typedef u16 mode_t;   /* File mode. */

/* File types. */
#define FILE_TYPE_UNKNOWN         0x0000
#define FILE_TYPE_FIFO            0x1000
#define FILE_TYPE_CHAR_DEV        0x2000
#define FILE_TYPE_DIRECTORY       0x4000
#define FILE_TYPE_BLOCK_DEV       0x6000
#define FILE_TYPE_REGULAR         0x8000
#define FILE_TYPE_SYMLINK         0xa000
#define FILE_TYPE_SOCKET          0xc000
#define FILE_TYPE_WHT             0xe000 /* Found in the Linux kernel, though
                                          * I don't recognize it. */
#define FILE_TYPE(mode)         ((mode) & 0xf000)  /* ==> file type */

/* POSIX-compliant permissions. */
#define FILE_PERM_SETUID          0x0800
#define FILE_PERM_SETGID          0x0400
#define FILE_PERM_STICKY          0x0200
#define FILE_PERM_USR_READ        0x0100
#define FILE_PERM_USR_WRITE       0x0080
#define FILE_PERM_USR_EXEC        0x0040
#define FILE_PERM_GRP_READ        0x0020
#define FILE_PERM_GRP_WRITE       0x0010
#define FILE_PERM_GRP_EXEC        0x0008
#define FILE_PERM_OTHERS_READ     0x0004
#define FILE_PERM_OTHERS_WRITE    0x0002
#define FILE_PERM_OTHERS_EXEC     0x0001

/* Open flags. */
#define FILE_O_READ               0x00000001
#define FILE_O_WRITE              0x00000002
#define FILE_O_RW                 ( FILE_O_READ | FILE_O_WRITE )
#define FILE_O_CREATE             0x00000004
#define FILE_O_EXCL               0x00000008
#define FILE_O_TRUNC              0x00000010

/* Seek flags. */
#define SEEK_SET                  0x00000000
#define SEEK_CUR                  0x00000001
#define SEEK_END                  0x00000002

typedef u32                       off_t;   /* offsets. */
typedef u32                       size_t;  /* sizes. */
typedef s32                       ssize_t; /* Signed version of size_t. */

struct stat {
  int       ino;
  mode_t    mode;
  size_t    size;
  dev_t     dev;
};

#endif
