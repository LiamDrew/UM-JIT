#include "utility.h"

.global test
test:
    MOVZ w19, #0x0601        // Load lower 16 bits
    MOVK w19, #0xF010, LSL 16 // Load upper 16 bits

    MOV w1, w21
    MOV w0, w21
    MOV w0, w20
    MOV w0, w19
    MOV w0, w0
    ADD w1, w2, w3
    blr x28
    ret