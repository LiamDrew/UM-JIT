#include "utility.h"

.global test
test:
    MOVZ w19, #0x0000        // Load lower 16 bits
    MOVK w19, #0xFFFF, LSL 16 // Load upper 16 bits

    MOV w1, w21
    MOV w0, w21
    MOV w0, w20
    MOV w0, w19
    ret