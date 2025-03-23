#include "utility.h"

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

.macro push_regs
    stp x12, x13, [sp, #-16]!   /* Push x12 and x13 onto stack */
    stp x14, x15, [sp, #-16]!   /* Push x14 and x15 onto stack */
.endm

.macro pop_regs
    ldp x14, x15, [sp], #16     /* Pop x14 and x15 from stack */
    ldp x12, x13, [sp], #16     /* Pop x12 and x13 from stack */
.endm

.macro function_start
    stp x29, x30, [sp, #-16]!   /* Save frame pointer and link register */
    mov x29, sp                 /* Set up frame pointer */
.endm

.macro function_end
    ldp x29, x30, [sp], #16     /* Restore frame pointer and link register */
.endm

.macro restore_default_opcode
    mov w14, #OP_DUPLICATE
.endm

.global _run
_run:
    function_start

    /* Save non-volatile registers */
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!

    /* Zero out the first 8 non-volatile registers for UM use */
    mov x19, xzr
    mov x20, xzr
    mov x21, xzr
    mov x22, xzr
    mov x23, xzr
    mov x24, xzr
    mov x25, xzr
    mov x26, xzr

    /* Store the address of the segment currently being executed */
    mov x27, x0

    /* Store the base usable memory address (umem) */
    mov x28, x1

    /* Load the address of the large_op label into register x15
     * On Linux, the instruction is -> (ldr x15, =large_op)
     * On Darwin, we need the following: */
    adrp x15, large_op@PAGE
    add x15, x15, large_op@PAGEOFF

    /* Store the default value of the large_op opcode in w14 */
    mov w14, #OP_DUPLICATE

    /* x13 will store the instruction pointer address when doing jumps */

    /* Store the address of the handle_map function in x12 */
    adrp x12, handle_map@PAGE
    add x12, x12, handle_map@PAGEOFF

    /* Set the 32-bit program pointer to 0 */
    mov x10, 0

loop:
    /* Finish if the executable memory is NULL
     * cbz x0, done */
    cbz x27, done
    
    mov x0, x27

    /* Calculating the start position in executable memory */
    mov x9, #CHUNK
    
    /* x0 = (x10 * x9) + x0 */
    madd  x0, x10, x9, x0

    /* Calling the executable memory as a function */
    blr x0

    /* Jump back to the beginning of the loop */
    b loop

done:
    /* Restore non-volatile registers before returning to main */
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
    push_regs
    mov x1, x28
    bl _map_segment
    pop_regs
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
    push_regs
    bl _unmap_segment
    pop_regs
    function_end
    restore_default_opcode
ret

.out:
    function_start
    push_regs
    bl _putchar
    pop_regs
    function_end

    restore_default_opcode
    br x13

.in:
    function_start
    push_regs
    bl _getchar
    pop_regs
    function_end
    restore_default_opcode
    br x13

.dup:
    function_start

    /* push x10 */
    stp x9, x10, [sp, #-16]!
    push_regs

    mov x1, x28
    bl _load_program
    
    /* Only when we load and compile a different segment do we have to update
     * x27 to store the new compiled executable memory */
    mov x27, x0
    pop_regs

    /* pop x10 */
    ldp x9, x10, [sp], #16

    function_end
ret /* From here, we will return to the main loop */

