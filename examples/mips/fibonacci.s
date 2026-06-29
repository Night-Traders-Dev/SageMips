# SageMips Example: Fibonacci (20 terms)
# sagemips asm examples/mips/fibonacci.s && sagemips run fibonacci.mips

    .text
    .globl main
main:
    addiu $s0, $zero, 20       # n terms
    addiu $s1, $zero, 0        # a = 0
    addiu $s2, $zero, 1        # b = 1
    addiu $s3, $zero, 0        # i = 0

loop:
    addiu $v0, $zero, 1        # print_int a
    addiu $a0, $s1, 0
    syscall
    addiu $v0, $zero, 6        # print space
    addiu $a0, $zero, 0x20
    syscall

    add   $t0, $s1, $s2        # c = a + b
    move  $s1, $s2             # a = b
    move  $s2, $t0             # b = c

    addiu $s3, $s3, 1
    bne   $s3, $s0, loop
    nop

    addiu $v0, $zero, 6        # newline
    addiu $a0, $zero, 0x0A
    syscall
    addiu $v0, $zero, 9        # halt
    syscall
