#ifndef __ERRORS_H__
#define __ERRORS_H__

#define E_OK        0   /* Everything's fine, no error. */
#define E_NOMEM     1   /* Not enough memory. */
#define E_NOKOBJ    2   /* Kernel object could not be found. */
#define E_CORRUPT   3   /* Something got corrupted in the kernel. */

extern int errno;

void set_errno(int);
int get_errno();
void perror(char *);

void kernel_panic(char *msg);

#define PANIC_HYSTERICAL  0   /* Panic about everything... */
#define PANIC_PERROR      1   /* Panic when perror is called. */
#define PANIC_NOPANIC     255 /* Keep going and let the world explode. */

void set_panic_level(int);

#endif
