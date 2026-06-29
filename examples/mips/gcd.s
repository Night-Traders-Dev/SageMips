# SageMips Example: GCD (Euclidean Algorithm)
# GCD(1071, 462) = 21, GCD(252, 105) = 21

    .text
    .globl main

gcd:                           # gcd(a0=a, a1=b) -> v0
    beq   $a1, $zero, done
    nop
    div   $a0, $a1
    mfhi  $t0
    move  $a0, $a1
    move  $a1, $t0
    b     gcd
    nop
done:
    move  $v0, $a0
    jr    $ra
    nop

pgcd:                          # print_gcd(msg:a0, a:a1, b:a2)
    addiu $sp, $sp, -16
    sw    $ra, 12($sp)
    sw    $a1, 8($sp)
    sw    $a2, 4($sp)
    sw    $a0, 0($sp)

    addiu $v0, $zero, 2        # print_str(msg)
    syscall

    lw    $a0, 8($sp)
    lw    $a1, 4($sp)
    jal   gcd
    nop

    move  $a0, $v0
    addiu $v0, $zero, 1        # print_int(result)
    syscall

    addiu $v0, $zero, 6        # newline
    addiu $a0, $zero, 0x0A
    syscall

    lw    $ra, 12($sp)
    addiu $sp, $sp, 16
    jr    $ra
    nop

main:
    la    $a0, m1
    addiu $a1, $zero, 1071
    addiu $a2, $zero, 462
    jal   pgcd
    nop

    la    $a0, m2
    addiu $a1, $zero, 252
    addiu $a2, $zero, 105
    jal   pgcd
    nop

    la    $a0, m3
    addiu $a1, $zero, 0
    addiu $a2, $zero, 5
    jal   pgcd
    nop

    la    $a0, m4
    addiu $a1, $zero, 17
    addiu $a2, $zero, 0
    jal   pgcd
    nop

    addiu $v0, $zero, 9        # halt
    syscall

    .data
m1: .asciiz "GCD(1071,462) = "
m2: .asciiz "GCD(252,105)  = "
m3: .asciiz "GCD(0,5)      = "
m4: .asciiz "GCD(17,0)     = "
