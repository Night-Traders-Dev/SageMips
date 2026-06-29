# SageMips Example: Recursive Factorial
# factorial(10) = 3628800

    .text
    .globl main

factorial:                     # factorial(n:a0) -> v0
    addiu $sp, $sp, -16
    sw    $ra, 12($sp)
    sw    $a0, 8($sp)

    slti  $t0, $a0, 2          # n < 2 ?
    beq   $t0, $zero, recurse
    nop
    addiu $v0, $zero, 1        # return 1
    b     done
    nop

recurse:
    addiu $a0, $a0, -1         # n - 1
    jal   factorial
    nop
    lw    $a0, 8($sp)
    mul   $v0, $a0, $v0        # n * factorial(n-1)

done:
    lw    $ra, 12($sp)
    addiu $sp, $sp, 16
    jr    $ra
    nop

main:
    addiu $a0, $zero, 10
    jal   factorial
    nop

    addiu $a0, $v0, 0
    addiu $v0, $zero, 1        # print_int
    syscall

    addiu $v0, $zero, 6        # newline
    addiu $a0, $zero, 0x0A
    syscall
    addiu $v0, $zero, 9        # halt
    syscall
