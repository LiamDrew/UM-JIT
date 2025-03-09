// /*
//  * For ARM64 on MacOS, the following registers are non-volatile
//  * X19-X29 */

// #include <stdio.h>
// #include "utility.h"

// typedef void *(*Function)(void);

// void help();

// int main(int argc, char *argv[]) {

//     // int x = 0x12345678;
//     // printf("%d\n", x);
//     // help();
//     void *dense = &test;
//     Function fn = (Function)dense;
//     fn();
//     // test();

//     return 0;
// }

// // void help() {
// //     int x = 0x12345678;
// //     (void)x;
// // }
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// Function prototype for our dynamically created function
typedef int (*SimpleFunc)(void);

int main()
{
    // Size of our executable code segment (one page is typically enough)
    size_t size = sysconf(_SC_PAGESIZE);

    // Allocate executable memory using mmap
    void *mem = mmap(NULL, size,
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS,
                     -1, 0);

    if (mem == MAP_FAILED)
    {
        perror("mmap failed");
        return 1;
    }

    // This is where we'll write our ARM64 machine code
    unsigned char *code = (unsigned char *)mem;

    // The instruction we'll create: MOV X0, #42 followed by RET
    // MOV X0, #42 (0xD2800540)
    code[0] = 0x40; // Least significant byte first (little-endian)
    code[1] = 0x05;
    code[2] = 0x80;
    code[3] = 0xD2;

    // RET instruction (0xD65F03C0)
    code[4] = 0xC0;
    code[5] = 0x03;
    code[6] = 0x5F;
    code[7] = 0xD6;

    // Clear instruction cache to make sure CPU sees our new code
    __builtin___clear_cache(mem, (char *)mem + 8);

    // Cast the memory to a function pointer
    SimpleFunc func = (SimpleFunc)mem;

    // Call our dynamically created function
    int result = func();

    printf("The function returned: %d\n", result);

    // Free the memory using munmap
    munmap(mem, size);

    return 0;
}