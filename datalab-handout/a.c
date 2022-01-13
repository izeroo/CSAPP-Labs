#include <stdio.h>
int main() {
  unsigned int a = 0x3f800000;
  printf("%f\n", a);
  printf("%x\n", a);
  float b = a;
  int c = b;
  printf("%d", c);
  return 0;
}
