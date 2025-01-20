#include "utility.h"

.macro push_regs
    push %r8
    push %r9
    push %r10
    push %r11
.endm

.macro pop_regs
    pop %r11
    pop %r10
    pop %r9
    pop %r8
.endm

.global zero_regs
zero_regs:
    xor %r8, %r8
    xor %r9, %r9
    xor %r10, %r10
    xor %r11, %r11
    xor %r12, %r12
    xor %r13, %r13
    xor %r14, %r14
    xor %r15, %r15
ret

.global out
    push_regs
    call fputc@PLT
    pop_regs