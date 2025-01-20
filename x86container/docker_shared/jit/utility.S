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
    push %rsi
.endm

.macro pop_regs
    pop %rsi
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

    # Load the address of the function global into RBX
    lea function(%rip), %rbx

    # Set the 32-bit program pointer RBP to 0
    movl $0, %ebp

    # Put address of active segment to RSI
    mov %rdi, %rsi

loop:
    # Check if the program pointer to the zero segment is NULL
    # test %rdi, %rdi
    test %rsi, %rsi
    jz done

    # Move the program counter to eax
    movl %ebp, %eax

    # Caclulate the address of the function we are going to be calling
    imul $CHUNK, %rax
    # add %rdi, %rax
    add %rsi, %rax

    # Save registers
    push %rsi

    # Call the function
    call *%rax

    # Restore registers
    pop %rsi

    # Move the result into rdi and rsi
    # mov %rax, %rdi
    mov %rax, %rsi

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
    # This will not work
    push_regs
    mov stdout(%rip), %rsi
    call fputc@PLT
    pop_regs
ret

.load:
    push_regs


    call load_program
    pop_regs
ret

.in:
    push_regs
    call getchar@PLT
    pop_regs
ret