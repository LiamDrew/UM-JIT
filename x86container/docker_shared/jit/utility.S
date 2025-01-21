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
    push %rsi # for now
.endm

.macro pop_regs
    pop %rsi
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

    # Put address of global tag into RBX
    lea function(%rip), %rbx

    # Put address of active segment into RCX
    mov %rdi, %rcx

    # Put 0 into RDX (program pointer)
    movl $0, %edx
    
    # Address of zero vals segment will live in RSI

    # Upper bits will live in RBP
    mov %rsi, %rbp
    # Instead, load the mask into another register first:
    mov $0xFFFFFFFF00000000, %rax    # Load mask into rax
    and %rax, %rbp                    # Apply mask

loop:
    # Check if the program pointer to the zero segment is NULL
    test %rcx, %rcx
    
    jz done

    # Move the program counter to eax
    movl %edx, %eax

    # Caclulate the address of the function we are going to be calling
    imul $CHUNK, %rax
    add %rcx, %rax

    # Call the function
    call *%rax

    # Move the result into rcx
    mov %rax, %rcx

    # Loop back
    jmp loop

done:
    ret

.global function
function:
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
    cmp $6, %al
    je .debug
ret

.map:
    push_regs
    call map_segment    # puts correct return value in rax
    pop_regs
ret

.unmap:
    push_regs
    call unmap_segment
    pop_regs
ret

.out:
    push %rsi
    push_regs
    mov stdout(%rip), %rsi
    call fputc@PLT
    pop_regs
    pop %rsi
ret

.load:
    # RDX (the program counter) gets updated right before load program gets
    # called, so we have to save its value here only
    push %rdx
    push_regs
    call load_program
    pop_regs
    pop %rdx
ret

.in:
    push_regs
    call getchar@PLT
    pop_regs
ret

.debug:
    push %rax
    push %rbp

    push %rdi
    push %rsi

    # Print RAX first - need to move saved rax value into rsi for printf
    mov 24(%rsp), %rsi   # Get saved rax value from stack
    lea rax_fmt(%rip), %rdi  # First arg is format string
    xor %eax, %eax      # Clear AL (number of vector registers used)
    call printf@PLT

    # Print RBP
    mov 16(%rsp), %rsi   # Get saved rbp value from stack
    lea rbp_fmt(%rip), %rdi  # First arg is format string
    xor %eax, %eax      # Clear AL
    call printf@PLT

    pop %rsi
    pop %rdi
    pop %rbp
    pop %rax


    mov $0, %rdi
    call exit@PLT
ret

.section .rodata
rax_fmt:
    .string "RAX: 0x%lx\n"
rbp_fmt:
    .string "RBP: 0x%lx\n"
