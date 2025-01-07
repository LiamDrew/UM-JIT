#include <stdio.h>
#include <unistd.h>

int main()
{
    asm volatile("marker_label2: .inst 0xCAFEBABE");
    return 0;
}