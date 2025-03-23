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

// .extern _getchar
// .extern _putchar
// .extern _load_program
// .extern _map_segment
// .extern _unmap_segment

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

.macro restore_default_opcode
    mov w14, #OP_DUPLICATE
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
    mov x27, x0

    // NOTE: It would be ideal to use register x28 to store address of the
    // segment sequence, since that way it could be updated in the map_segment
    // function call with inline assembly.

    // store the address of the value sequence
    mov x28, x1

    // ldr x15, =large_op
    // Use this for position-independent code:
    adrp x15, large_op@PAGE
    add x15, x15, large_op@PAGEOFF

    // // put the address of the large op function handler in x28
    // ldr x28, =large_op

    // // store the address of the value sequence
    // mov x15, x1

    // store the default value of the large op in w14
    mov w14, #OP_DUPLICATE

    // x13 will be used to store instruction pointer addresses when doing jumps

    // put the address of the handle_map function in x12
    // ldr x12, =handle_map

    // Use:
    adrp x12, handle_map@PAGE
    add x12, x12, handle_map@PAGEOFF

    // set the 32-bit program pointer to 0
    // mov x27, 0
    mov x10, 0

loop:
    // Finish if the executable memory is NULL
    // cbz x0, done
    cbz x27, done
    
    mov x0, x27

    // Calculating the start position in executable memory
    mov x9, #CHUNK
    
    // x0 = (x10 * x9) + x0
    madd  x0, x10, x9, x0

    // Calling the executable memory
    blr x0


    // stp x9, x10, [sp, #-16]!
    // push_regs
    // extra_push
    // bl print_registers_w19_to_w26
    // extra_pop
    // pop_regs
    // ldp x9, x10, [sp], #16


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

.global handle_map
handle_map:
    function_start
    extra_push
    push_regs

    bl _map_segment

    mov x28, x16

    pop_regs
    extra_pop
    function_end
ret


.global large_op
large_op:

    /* Plan: have x14 become the new opcode getting checked. The JITted code
     * will preserve #OP_DUPLICATE as the default, saving us an instruction in
     * the inject load program function */

    cmp x14, #OP_UNMAP
    beq .unmap

    cmp x14, #OP_IN
    beq .in

    cmp x14, #OP_OUT
    beq .out

    cmp x14, #OP_DUPLICATE
    beq .dup

.unmap:
    function_start
    extra_push
    push_regs
    bl _unmap_segment
    pop_regs
    extra_pop
    function_end

    restore_default_opcode

    // br?
ret

.out:
    function_start
    extra_push
    push_regs
    bl _putchar
    pop_regs
    extra_pop
    function_end

    restore_default_opcode
    br x13

.in:
    function_start
    extra_push
    push_regs
    bl _getchar
    pop_regs
    extra_pop
    function_end

    restore_default_opcode

    br x13

.dup:
    function_start

    /* NOTE: x10 is kind of a temporary situation over here. Once we find a
     * better home for everything else we will decide on a forever home */

    // push x10
    stp x9, x10, [sp, #-16]!

    extra_push
    push_regs
    
    bl _load_program
    
    /* Only when we load and compile a different segment do we have to update
     * x27 to store the new compiled executable memory */
    mov x27, x0

    pop_regs
    extra_pop

    // pop x10
    ldp x9, x10, [sp], #16

    // stp x9, x10, [sp, #-16]!
    // push_regs
    // extra_push
    // bl print_registers_w19_to_w26
    // extra_pop
    // pop_regs
    // ldp x9, x10, [sp], #16



    function_end
ret // this should return to the original loop

