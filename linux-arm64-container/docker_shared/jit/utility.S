#include "utility.h"


/* MACROS to preserve registers:
 * Claude and I had an interesting conversation about preserving return addresses
 * I was tempted to return x30 (the register that contains the function return address)
 * in another register, but Claude thinks I should push it to the stack because the compiler is already very well
 * optimized for this sort of thing. That makes sense, but this will be something to profile for some of these functions
 */

.macro push_regs
    stp x30, xzr, [sp, #-16]!
.endm

.macro pop_regs
    ldp x30, xzr, [sp], #16 
.endm

.macro function_start
    stp x29, x30, [sp, #-16]!   // Save frame pointer and link register
    mov x29, sp                  // Set up frame pointer
.endm

.macro function_end
    ldp x29, x30, [sp], #16     // Restore frame pointer and link register
    ret
.endm

/* Arm special registers are:
 * x0 (first argument, return register)
 * x1 (second argument)
 * x2 (third argument)
 * etc. (x0-x7)
 * x8: Indirect result location register
 * x16, x17: Intra-procedure-call temporary registers (IP0, IP1)
 * x18: Platform register (for platform specific uses)
 * x29: Frame pointer (for managing stack frame)
 * x30: Link register
 * Volatile general-purpose registers are: x9-x15
 * Non-volatile general-purpose registers are x19-x28
 */

.global run
run:
    function_start

    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!


    // Zero out the first 8 non-volatile registers for the UM
    mov x19, xzr
    mov x20, xzr
    mov x21, xzr
    mov x22, xzr
    mov x23, xzr
    mov x24, xzr
    mov x25, xzr
    mov x26, xzr

    // mov the memory address we want into x27
    mov x27, x0

    // put the address of the large op function handler in x28
    ldr x28, =large_op

    // mov x0, x28
    // bl print_address

loop:

    // Calling the executable memory
    blr x27

    // mov x15, x0
    // bl print_hex_value
    // mov x0, x15
    cbnz x0, loop

done:
    // Printing a success message to show signs of life
    // mov x0, #50  // '2'
    // bl putchar

    // Restore non-volatile registers before returning to main
    ldp x27, x28, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    
    function_end

.global large_op
large_op:
    function_start
    cmp x1, #OP_IN
    beq .in
    
    cmp x1, #OP_OUT
    beq .out


.out:
    bl putchar
function_end

.in:
function_end

