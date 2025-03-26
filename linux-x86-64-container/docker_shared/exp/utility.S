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

    // add %rcx, %rax

    mov %r8d, %edi
    mov %r9d, %edi
    mov %r8d, %esi
    mov %r9d, %esi
    mov %r8d, %edx
    mov %r9d, %edx

    mov %eax, %r8d
    mov %eax, %r9d


    // lea (%r9d, %r8d, 1), %r8d
    // lea (%r8d, %r9d, 1), %r8d
    // lea (%r8d, %r8d, 1), %r9d
    mov %r8d, %eax
    ret
