#include "utility.h"


/* MACROS to preserve registers:
 * Claude and I had an interesting conversation about preserving return addresses
 * I was tempted to return x30 (the register that contains the function return address)
 * in another register, but Claude thinks I should push it to the stack because the compiler is already very well
 * optimized for this sort of thing. That makes sense, but this will be something to profile for some of these functions
 */

/* Even with the plethora of Arm registers, I'm still running into a register
 * shortage for non-volatile registers. I'm using these macros to preserve
 * these two registers as if they were non-volatile */
.macro push_regs
    stp x14, x15, [sp, #-16]!   // Push x14 and x15 onto stack
.endm

.macro pop_regs
    ldp x14, x15, [sp], #16     // Pop x14 and x15 from stack
.endm

.macro extra_push
    stp x12, x13, [sp, #-16]!   // Push x12 and x13 onto stack
.endm

.macro extra_pop
    ldp x12, x13, [sp], #16     // Pop x12 and x13 from stack
.endm

.macro function_start
    stp x29, x30, [sp, #-16]!   // Save frame pointer and link register
    mov x29, sp                 // Set up frame pointer
.endm

.macro function_end
    ldp x29, x30, [sp], #16     // Restore frame pointer and link register
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

    // store the address of the segment currently being executed
    mov x14, x0 

    // store the address of the value sequence
    mov x15, x1

    // put the address of the large op function handler in x28
    ldr x28, =large_op

    // set the 32-bit program pointer to 0
    mov x27, 0

loop:
    // Finish if the executable memory is NULL
    cbz x0, done

    // NOTE: this should not be the case, but something to test for
    // Restore x14 (load program may have wonked it)
    // mov x14, x0

    /* Add the program counter to the executable memory address to start
     * executing at the correct point */

    // using volatile register x9 as scratch paper
    mov x9, #CHUNK
    
    // x0 = (x27 * x9) + x0
    madd  x0, x27, x9, x0

    // Calling the executable memory
    blr x0

    // mov x12, x0
    // push_regs
    // extra_push
    // bl print_registers_w19_to_w26
    // extra_pop
    // pop_regs
    // mov x0, x12


    // Jump back to the beginning of the loop
    b loop
done:
    // Restore non-volatile registers before returning to main
    ldp x27, x28, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16    
    function_end
ret

.global large_op
large_op:
    cmp x1, #OP_MAP
    beq .map

    cmp x1, #OP_UNMAP
    beq .unmap

    cmp x1, #OP_IN
    beq .in

    cmp x1, #OP_OUT
    beq .out

    cmp x1, #OP_DUPLICATE
    beq .dup

.map:
    function_start
    push_regs
    bl map_segment
    pop_regs
    function_end
ret

.unmap:
    function_start
    push_regs
    bl unmap_segment
    pop_regs
    function_end
ret

.out:
    // Writing out the function start macro because the end is different
    // stp x29, x30, [sp, #-16]!   // Save frame pointer and link register
    // mov x29, sp                 // Set up frame pointer
    function_start
    push_regs
    extra_push // saving regs x12 and x13 for a special short-term purpose
    bl putchar
    extra_pop // restoring regs x12 and x13
    pop_regs
    function_end
    // ldp x29, x30, [sp], #16     // Restore frame pointer and link register
    br x13

.in:
// function_start
    // stp x29, x30, [sp, #-16]!   // Save frame pointer and link register
    // mov x29, sp                 // Set up frame pointer
    function_start
    push_regs
    extra_push
    bl getchar
    extra_pop
    pop_regs
    function_end
    // ldp x29, x30, [sp], #16     // Restore frame pointer and link register
    br x13
// function_end

// NOTE: this one needs help
.dup:
    function_start
    push_regs
    bl load_program
    pop_regs
    function_end
    ret // this should return to the original loop

