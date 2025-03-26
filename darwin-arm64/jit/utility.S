#include "utility.h"

/* Arm registers conventions are:
 * x0 (first argument, return register)
 * x1 (second argument)
 * x2 (third argument)
 * etc. (x0-x7)
 * x8: Indirect result location register
 * Volatile general-purpose registers are: x9-x15
 * x16, x17: Intra-procedure-call temporary registers (IP0, IP1)
 * x18: Platform register (for platform specific uses)
 * Non-volatile general-purpose registers are x19-x28
 * x29: Frame pointer (for managing stack frame)
 * x30: Link register
 */

.macro push_regs
    stp x12, x13, [sp, #-32]! /* Push x12 and x13 onto stack */
    stp x14, x15, [sp, #16]   /* Push x14 and x15 onto stack */
.endm

.macro pop_regs
    ldp x14, x15, [sp, #16]   /* Pop x14 and x15 from stack */
    ldp x12, x13, [sp], #32   /* Pop x12 and x13 from stack */
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

    adrp x13, handle_unmap@PAGE
    add x13, x13, handle_unmap@PAGEOFF

    /* Store the address of the handle_map function in x12 */
    adrp x12, handle_map@PAGE
    add x12, x12, handle_map@PAGEOFF

    /* Set the 32-bit program pointer to 0 */
    mov x10, 0

loop:
    /* Finish if the executable memory is NULL
     * cbz x0, done */
    cbz x27, done
    
    /* Calculating the start position in executable memory */
    mov x0, x27
    mov x9, #CHUNK
    
    /* x0 = (x10 * x9) + x0 */
    madd  x0, x10, x9, x0

    /* Calling the executable memory as a function */
    br x0

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

.global large_op
large_op:

    /* x14 contains the value of the opcode getting checked. We are preserving
    * #OP_DUPLICATE as the default opcode and checking it first because it is
    * common and expected to be very fast. */

    cmp x14, #OP_DUPLICATE
    beq .dup

    cmp x14, #OP_IN
    beq .in

    cmp x14, #OP_OUT
    beq .out

    cmp x14, #OP_HALT
    beq .halt

.dup:
    /* If w0 is not 0, duplicate the segment and load it into segment 0.
     * Otherwise, branch back to the loop */
    cbnz w0, +8
    b loop

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

    /* Branch back to the main loop */
    b loop

.out:
    function_start
    push_regs
    bl _putchar
    pop_regs
    function_end

    restore_default_opcode
ret

.in:
    function_start
    push_regs
    bl _getchar
    pop_regs
    function_end
    restore_default_opcode
ret

.halt:
    /* Zero out the active executable memory segment and branch back to the
     * loop, ending the program. */
    mov x27, #0
    b loop

.global handle_map
handle_map:
    function_start
    push_regs

    mov x1, x28
    bl _map_segment

    pop_regs
    function_end
ret

.global handle_unmap
handle_unmap:
    function_start
    push_regs

    bl _unmap_segment

    pop_regs
    function_end
ret

