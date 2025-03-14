#include "utility.h"

.global test
test:
    MOVZ w1, #0x0004        // Load lower 16 bits
    MOVK w1, #0x0000, LSL 16 // Load upper 16 bits

    // str w1, [x0]

    mov w10, w0
    mov w1, w19

    mov x0, x14
    mov x0, x27

    // xor x27, x27
    mov x27, #0

    adr x14, +8
    adr x13, +8

    blr x12

    CMP w0, #0
    CMP w19, #0
    CSEL w0, w0, w0, NE

    cbnz w0, +8
    cbz w0, +8
    cbz w0, +8
    cbz w0, +12
    
    ret