#include "utility.h"

/* x86_64 System V register calling convention:
 * rax stores the return value
 * Function argument order is: rdi, rsi, rdx, rcx
 * volatile registers are rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11
 * non-volatile registers are rbx, rbp, r12, r13, r14, r15
 */

.macro push_regs
    push %r8
    push %r9
    push %r10
    push %r11
    push %rcx

.endm

.macro pop_regs
    pop %rcx
    pop %r11
    pop %r10
    pop %r9
    pop %r8
.endm

.global run
run:
    /* Per the x86 calling convention, push the non-volatile registers to the
     * stack */
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15

    /* zero machine registers for JIT compiler use */
    xor %r8, %r8
    xor %r9, %r9
    xor %r10, %r10
    xor %r11, %r11
    xor %r12, %r12
    xor %r13, %r13
    xor %r14, %r14
    xor %r15, %r15

    /* Load the address of the function global into register RBX */
    lea function(%rip), %rbx

    /* The address of the executable segment is currently in rdi. 
     * Save the address of the current executable memory into rbp */
    mov %rdi, %rbp

    /* I am going to use rcx to store the address of the usable memory provided
     * by Virt32 */
    mov %rsi, %rcx
    
    /* Set the 32-bit program pointer RSI to 0 */
    movl $0, %esi

loop:

    /* Move the executable memory address into rdi */
    mov %rbp, %rdi

    /* NOTE: at this point, rdi must contain the address of executable memory */

    /* Check if the program pointer to the zero segment is NULL */
    test %rdi, %rdi
    jz done

    /* Move the program counter to eax */
    movl %esi, %eax

    /* Caclulate the address of the function we are going to be calling */
    imul $CHUNK, %rax
    add %rdi, %rax

    /* Jump to the executable memory */
    jmp *%rax

done:
    /* Restore non-volatile registers */
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx
ret

.global function
function:
    cmp $OP_DUPLICATE, %al
    je .load
    cmp $OP_MAP, %al
    je .map
    cmp $OP_UNMAP, %al
    je .unmap
    cmp $OP_OUT, %al
    je .out
    cmp $OP_IN, %al
    je .in
    cmp $OP_HALT, %al
    je .halt

    jmp .recompile

.recompile:
    /* TODO: add self modifying code as a JIT feature 
     * This likely will require expanding the CHUNK size */
ret

.map:
    push_regs
    mov %rcx, %rsi
    call map_segment
    pop_regs
    /* return address is in rax */
ret

.unmap:
    push_regs
    call unmap_segment
    pop_regs
ret

.out:
    push_regs
    mov stdout(%rip), %rsi
    call fputc@PLT
    pop_regs
ret

.load:

    /* The address of the new executable memory segment needs to go in rax */
    test %edi, %edi
    jne skip
    jmp loop

    skip:
    /* rsi gets updated right before load_program gets called */
    push %rsi
    push_regs
    mov %rcx, %rsi
    call load_program
    pop_regs
    pop %rsi
    mov %rax, %rbp
jmp loop

.in:
    push_regs
    call getchar@PLT
    pop_regs
ret

.halt:
    jmp done