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
    // lea (%r9d, %r8d, 1), %r8d
    // lea (%r8d, %r9d, 1), %r8d
    // lea (%r8d, %r8d, 1), %r9d
    mov %r8d, %eax
    ret
