#include "utility.h"

.global test
test:
    mov $2, %rax
    mov $3, %r8d
    mov $4, %r9d
    
    ret
