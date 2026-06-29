# SageMips Example: Hello World
# sagemips asm examples/mips/hello.s && sagemips run hello.mips

    .text
    .globl main
main:
    addiu $v0, $zero, 2        # syscall: print_str
    la    $a0, msg
    syscall

    addiu $v0, $zero, 2        # print_str
    la    $a0, ans_msg
    syscall

    addiu $v0, $zero, 1        # print_int
    addiu $a0, $zero, 42
    syscall

    addiu $v0, $zero, 6        # print_char '\n'
    addiu $a0, $zero, 0x0A
    syscall

    addiu $v0, $zero, 9        # halt
    syscall

    .data
msg:     .asciiz "Hello, SageMips!\n"
ans_msg: .asciiz "Answer: "
