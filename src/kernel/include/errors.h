#ifndef __ERRORS_H__
#define __ERRORS_H__

#define E_OK             0  /* Everything's fine, no error. */
#define E_NOMEM          1  /* Not enough memory. */
#define E_NOKOBJ         2  /* Kernel object could not be found. */
#define E_CORRUPT        3  /* Something got corrupted in the kernel. */
#define E_NODEV          4  /* Invalid device. */
#define E_IO             5  /* IO error. */
#define E_ACCESS         6  /* Access denied trying to open a file. */
#define E_BADFD          7  /* The requested operation is not valid. */
#define E_BUSY           8  /* Device is busy, i.e. locked. */
#define E_NOROOT         9  /* No root filesystem. */
#define E_INVFS         10  /* Invalid filesystem type. */
#define E_MOUNTED       11  /* Filesystem is already mounted. */
#define E_NOTMOUNTED    12  /* Filesystem is not mounted. */
#define E_NOENT         13  /* File not found. */
#define E_NOEMPTY       14  /* Directory is not empty. */
#define E_EXIST         15  /* The resource already exists. */
#define E_NODIR         16  /* Dir operation on no dir node. */
#define E_LIMIT         17  /* Some limit was reached. */
#define E_NOSPACE       18  /* No space left on device. */
#define E_NOTIMP        19  /* No implemented yet. */

extern int errno;

void set_errno(int);
int get_errno();
void perror(char *);

void kernel_panic(char *msg);

#define PANIC_HYSTERICAL  0   /* Panic about everything... */
#define PANIC_PERROR      1   /* Panic when perror is called. */
#define PANIC_NOPANIC     255 /* Keep going and let the world explode. */

void set_panic_level(int);
int get_panic_level();

#endif
