#include "utility.h"

.global test
test:
    MOVZ w1, #0x0004        // Load lower 16 bits
    MOVK w1, #0x0000, LSL 16 // Load upper 16 bits

    // str w1, [x0]

    CMP w0, #0
    CMP w19, #0
    CSEL w0, w0, w0, NE
    
    ret