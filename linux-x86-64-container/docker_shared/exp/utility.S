#include "utility.h"

.global test
test:
    mov %rcx, %rax
    mov (%rcx), %rax

    // mov (%rax, %r8,8) %rax
    // mov (%rcx, %r8,8) %rax
    // mov rax, [rax + r8d*8]
    movq (%rax,%r8,8),%rax
    movq (%rcx,%r8,8),%rax

    call *%rbx
    jmp *%rbx
    ret
