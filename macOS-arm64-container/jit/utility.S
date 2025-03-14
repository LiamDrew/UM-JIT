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

.global _run
_run:
    function_start

    // Zero out the first 8 non-volatile registers
    mov x19, XZR
    mov x20, XZR
    mov x21, XZR
    mov x22, XZR
    mov x23, XZR
    mov x24, XZR
    mov x25, XZR
    mov x26, XZR

    // Load the address of the function global in x27

    // Storing x30 on the stack for future use

loop:
    ; Need to update this such that it calls the executable memory as a function

    ; push_regs
    ; mov x0, #54
    ; bl _putchar      // Will return value in x0
    ; pop_regs
    
    ldr x27, =handler

    ; push_regs

    blr x27

    ; pop_regs

    function_end

.global handler
handler:

    ; push_regs
    function_start
    mov x0, #54
    bl _putchar      // Will return value in x0
    function_end
    ; pop_regs
    ret



