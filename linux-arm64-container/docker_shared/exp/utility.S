#include "utility.h"

.global test
test:
    MOVZ w1, #0x0004        // Load lower 16 bits
    MOVK w1, #0x0000, LSL 16 // Load upper 16 bits

    // str w1, [x0]

    // ldr x9, [x15, wA, UXTW #3]
    ldr x9, [x15, w0, UXTW #3]
    ldr x9, [x28, w0, UXTW #3]

    blr x28
    blr x15

    br x28
    br x15

    blr x13

    add x9, x28, x0
    add x9, x28, x1
    add x9, x28, x19

    
    ret