#include "utility.h"

.global test
test:
    mov $2, %rax
    mov $3, %r8d
    mov $4, %r9d
    lea (%r8d, %r8d, 1), %r8d
    lea (%r8d, %r8d, 1), %r9d
    lea (%r8d, %r8d, 1), %r10d
    lea (%r8d, %r8d, 1), %r11d

    lea (%rcx, %r8, 1), %rax
    lea (%rcx, %r9, 1), %rax

    add %rcx, %rax
    add %r8, %rax
    add %r9, %rax

    // lea (%r9d, %r8d, 1), %r8d
    // lea (%r8d, %r9d, 1), %r8d
    // lea (%r8d, %r8d, 1), %r9d
    mov %r8d, %eax
    ret
