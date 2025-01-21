.global main
.text
main:
    movl %r9d, %eax
    orq %rax, %rbp
    cmp $0, %r9d
    mov $60, %rax    # exit syscall number
    xor %rdi, %rdi   # return code 0
    syscall

ret
