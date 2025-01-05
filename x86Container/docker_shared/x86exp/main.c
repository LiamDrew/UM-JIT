#include <stdio.h>

int main()
{
    printf("hello world\n");

    asm volatile(
        "movq $0, %%r8\n\t"
        "movq $0, %%r9\n\t"
        "movq $0, %%r10\n\t"
        "movq $0, %%r11\n\t"
        "movq $0, %%r12\n\t"
        "movq $0, %%r13\n\t"
        "movq $0, %%r14\n\t"
        "movq $0, %%r15\n\t"
        :
        :
        : "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" // Clobber list
    );

    unsigned x = 8;
    char instruction[32]; // Buffer for the instruction string
    sprintf(instruction, "movq $1, %%r%d\n\t", x);

    // Use the generated instruction
    asm volatile(
        instruction
        :
        :
        : "r8" // Make sure to specify the register you're modifying
    );

    return 0;
}