#include "utility.h"

.global test
test:
    MOVZ w1, #0x0004        // Load lower 16 bits
    MOVK w1, #0x0000, LSL 16 // Load upper 16 bits

    // str w1, [x0]

    mov w0, #HELP
    mov w0, 0

    udiv w19, w19, w19
    udiv w0, w0, w19
    udiv w0, w0, w0
    udiv w0, w1, w0
    udiv w0, w7, w0
    udiv w0, w15, w0
    udiv w0, w19, w0

    
    ret