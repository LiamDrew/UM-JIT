#include <stdio.h>
#include <assert.h>

__attribute__((visibility("default"))) 
unsigned char read_char(void)
{
    printf("Function was called\n");
    int x = getc(stdin);
    assert(x != EOF);
    return (unsigned char)x;
}