#include <errors.h>
#include <fb.h>
#include <string.h>
#include <hw.h>

int errno;
int panic_level;

void set_errno(int _errno) {
  errno = _errno;
  if (panic_level <= PANIC_HYSTERICAL) {
    fb_printf("ERROR: Code %dd\n", errno);
    kernel_panic("HYSTERICAL PANIC!!!");
  }
}

void perror(char * prompt) {
  fb_printf("ERROR: %s : Code: %dd\n", prompt, errno);
  if (panic_level <= PANIC_PERROR)
    kernel_panic("perror makes us PANIC!!!");
}

/* In case we run into PANIC. */
void kernel_panic(char *msg) {
  fb_set_fg_color(FB_COLOR_WHITE);
  fb_set_bg_color(FB_COLOR_RED);
  fb_clear();
  fb_write(msg, strlen(msg));
  hw_cli();
  hw_hlt();
  /* And the world stops ... */
}

void set_panic_level(int level) {
  panic_level = level;
}
