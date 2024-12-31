// // #include "stdio.h"

// // int main() {
// //     unsigned char c = 54;
// //     putchar(c);
// //     return 0;
// // }

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <sys/mman.h>
// #include <unistd.h>

// // Function pointer type for our JIT-compiled function
// typedef int (*JitFunction)(void);

// // Create executable memory region and write machine code
// JitFunction create_function()
// {
//     // Get page size for alignment
//     size_t page_size = sysconf(_SC_PAGESIZE);

//     // Get the actual address of putchar
//     void *putchar_addr = (void *)&putchar;

//     // Machine code to print '6' and return 0
//     unsigned char code[] = {
//         0x55,                   // push   %rbp
//         0x48, 0x89, 0xe5,       // mov    %rsp,%rbp
//         0x48, 0x83, 0xec, 0x10, // sub    $0x10,%rsp
//         0xc6, 0x45, 0xff, 0x36, // movb   $0x36,-0x1(%rbp)
//         0x0f, 0xb6, 0x45, 0xff, // movzbl -0x1(%rbp),%eax
//         0x89, 0xc7,             // mov    %eax,%edi
//     };

//     // Second part of code - after we insert the address
//     unsigned char code2[] = {
//         0xb8, 0x00, 0x00, 0x00, // mov    $0x0,%eax
//         0x00,
//         0xc9, // leave
//         0xc3  // ret
//     };

//     // Calculate total size needed
//     size_t total_size = sizeof(code) + sizeof(void *) + sizeof(code2);
//     (void)total_size;

//     // Allocate a page of memory with read, write, and execute permissions
//     void *memory = mmap(NULL, page_size,
//                         PROT_READ | PROT_WRITE | PROT_EXEC,
//                         MAP_PRIVATE | MAP_ANONYMOUS,
//                         -1, 0);

//     if (memory == MAP_FAILED)
//     {
//         perror("mmap failed");
//         exit(1);
//     }

//     // Copy first part of code
//     unsigned char *p = memory;
//     memcpy(p, code, sizeof(code));
//     p += sizeof(code);

//     // Direct call to putchar
//     *p++ = 0xe8; // call instruction
//     // Calculate relative offset for call
//     int32_t call_offset = (int32_t)((char *)putchar_addr - (char *)(p + 4));
//     memcpy(p, &call_offset, sizeof(int32_t));
//     p += sizeof(int32_t);

//     // Copy second part of code
//     memcpy(p, code2, sizeof(code2));

//     // Return the memory region cast to a function pointer
//     return (JitFunction)memory;
// }

// int main()
// {
//     // Create and get our JIT function
//     JitFunction func = create_function();

//     // Execute the JIT-compiled function
//     int result = func();

//     // Clean up (in practice, you'd want to munmap the memory)
//     return result;
// }

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// Function pointer type for our JIT-compiled function
typedef int (*JitFunction)(void);

// Create function now takes memory as parameter and returns how many bytes were written
size_t create_function(void *memory)
{
    // Get the actual address of putchar
    void *putchar_addr = (void *)&putchar;

    // Machine code to print '6' and return 0
    unsigned char code[] = {
        0x55,                   // push   %rbp
        0x48, 0x89, 0xe5,       // mov    %rsp,%rbp
        0x48, 0x83, 0xec, 0x10, // sub    $0x10,%rsp
        0xc6, 0x45, 0xff, 0x36, // movb   $0x36,-0x1(%rbp)
        0x0f, 0xb6, 0x45, 0xff, // movzbl -0x1(%rbp),%eax
        0x89, 0xc7,             // mov    %eax,%edi
    };

    // Second part of code - after we insert the address
    unsigned char code2[] = {
        0xb8, 0x00, 0x00, 0x00, // mov    $0x0,%eax
        0x00,
        0xc9, // leave
        0xc3  // ret
    };

    // Calculate total size needed
    size_t total_size = sizeof(code) + sizeof(void *) + sizeof(code2);

    // Copy first part of code
    unsigned char *p = memory;
    memcpy(p, code, sizeof(code));
    p += sizeof(code);

    // Direct call to putchar
    *p++ = 0xe8; // call instruction
    // Calculate relative offset for call
    int32_t call_offset = (int32_t)((char *)putchar_addr - (char *)(p + 4));
    memcpy(p, &call_offset, sizeof(int32_t));
    p += sizeof(int32_t);

    // Copy second part of code
    memcpy(p, code2, sizeof(code2));

    return total_size;
}

int main()
{
    // Get page size for alignment
    size_t page_size = sysconf(_SC_PAGESIZE);

    // Allocate a page of memory with read, write, and execute permissions
    void *memory = mmap(NULL, page_size,
                        PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1, 0);

    if (memory == MAP_FAILED)
    {
        perror("mmap failed");
        exit(1);
    }

    // Create the function in our allocated memory
    size_t bytes_written = create_function(memory);
    (void)bytes_written;

    // Cast memory to function pointer and execute
    JitFunction func = (JitFunction)memory;
    int result = func();

    // Clean up
    munmap(memory, page_size);

    return result;
}