#include "utility.h"

.global test
test:
    MOVZ w1, #0x0004        // Load lower 16 bits
    MOVK w1, #0x0000, LSL 16 // Load upper 16 bits

    str w1, [x0]
    str w1, [x1]
    str w1, [x0, #4]
    str w1, [x0, #8]
    str w1, [x0, #12]

    ldr w0, [x0, #12]

    ldr x9, [x15, w0, UXTW #3]
    ldr x9, [x15, w19, UXTW #3]
    ldr x9, [x15, w26, UXTW #3]
    // ldr w20, [x9, w20, UXTW #2]
    // ldr x0, [x0, w0, ]

    ldr w0, [x9, w0, UXTW #2]
    ldr w19, [x9, w0, UXTW #2]
    ldr w0, [x9, w19, UXTW #2]

    str w0, [x9, w0, UXTW #2]
    str w19, [x9, w0, UXTW #2]
    str w0, [x9, w19, UXTW #2]
    str w0, [x9, w26, UXTW #2]
    // ldr w19
    // ldr w26

    adr x13, +4
    adr x13, +8
    
    cbz w0, +12
    cbz w1, +12
    br x28

    mov w0, w19
    mov w0, w26
    
    ret