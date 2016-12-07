#include <syscall.h>

char * fmt = "Hola, mundo mundial. %d\n";

int main(int argc, char *argv[]) {
  fb_printf(fmt, 100);
  return 0;
}
