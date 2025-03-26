#include "utility.h"

.global test
test:
    mov $2, %rax
    mov $3, %r8d
    mov $4, %r9d

    mov $3, %al
    cmp $3, %al
    mov $3, %ax
    cmp $3, %ax
    mov $3, %eax
    cmp $3, %eax
    
    ret
