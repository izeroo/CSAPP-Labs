#include <stdio.h>
#define ALIGNMENT 16
#define DSIZE 8
int main()
{
    void *a = 0xdeadbeef;
    char *b = 0xdeadbeef;
    printf("%d\n", a==b);
}