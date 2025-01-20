#include "utility.h"

# Thanks Tom
#define C(val)  $ ## val

# x86_64 volatile registers are rax, rcx, rdx, r8, r9, r10, r11
# non-volatile registers are rbx, rbp, rdi, rsi, rsp, r12, r13, r14, r15
.macro push_regs
    push %r8
    push %r9
    push %r10
    push %r11
    push %rcx
    push %rdx
.endm

.macro pop_regs
    pop %rdx
    pop %rcx
    pop %r11
    pop %r10
    pop %r9
    pop %r8
.endm

.global run
run:
    # zero machine registers for JIT compiler use
    xor %r8, %r8
    xor %r9, %r9
    xor %r10, %r10
    xor %r11, %r11
    xor %r12, %r12
    xor %r13, %r13
    xor %r14, %r14
    xor %r15, %r15

    # Load the address of the function global into rbx
    # Use the instruction pointer for relative offset
    lea function(%rip), %rbx

    mov $0, %rbp

    # intial compiled zero segment is in rdi
    # Potential optimization: have the program pointer live in a register
    # during the program's execution?
    # TODO: All C functions must be implemented in assembly before that can happen

loop:
    # Check if the program pointer to the zero segment is NULL
    test %rdi, %rdi
    jz done

    # Load gs.pc from global memory
    movl gs(%rip), %eax     # Load 32-bit gs.pc into eax
    # movl %rbp, %eax

    # NOTE: could make rsi the address of the current zero segment

    # Caclulate the address of the function we are going to be calling
    imul $CHUNK, %rax
    add %rdi, %rax


    # Save any registers before executing the function

    # Call the function
    call *%rax

    # Restore registers

    # Move the result into rdi
    mov %rax, %rdi

    # Loop back
    jmp loop

done:
    ret

# NOTE: I am going to try doing this Tom's way first to get it working
# I will then see if it can be improved upon

.global function
function:
    # Check for each op
    cmp C(OP_MAP), %al
    je .map
    cmp C(OP_UNMAP), %al
    je .unmap
    cmp C(OP_OUT), %al
    je .out
    cmp C(OP_DUPLICATE), %al
    je .load
    cmp C(OP_IN), %al
    je .in

    # return if unknown op
    ret

.map:
    push_regs
    call map_segment
    pop_regs

    # return address is in rax
ret

.unmap:
    push_regs
    call unmap_segment
    pop_regs
ret

.out:
    mov stdout(%rip), %rsi

    push_regs
    call fputc@PLT
    pop_regs
    ret
.load:
    ret
.in:
    push_regs
    # call read_char
    call getchar@PLT
    pop_regs
    ret

.recompile:
    ret