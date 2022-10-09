#include <stddef.h>
#include <stdio.h>
static char *global_static_var;
int main()
{
    printf("%x at %p\n", global_static_var, &global_static_var);
    return 0;
}