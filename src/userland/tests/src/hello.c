#include <syscall.h>

char * fmt = "Hola, mundo mundial. %dd\n";
int x, y;

int main(int argc, char *argv[]) {
  x = 100;
  y = 90;

  fb_printf(fmt, x);

  return 0;
}
