# SageMips Example: Prime Counting (up to 200)
# Finds and prints all primes <= 200

    .text
    .globl main

main:
    addiu $s0, $zero, 200      # bound
    addiu $s1, $zero, 2        # n = 2
    addiu $s5, $zero, 0        # count

ploop:
    slt   $t0, $s1, $s0
    beq   $t0, $zero, done
    nop

    # is_prime(n)
    addiu $s2, $zero, 2        # d = 2
    addiu $t9, $zero, 1        # flag = true

dloop:
    mul   $t0, $s2, $s2
    slt   $t1, $t0, $s1        # d*d <= n ?
    beq   $t1, $zero, check
    nop
    div   $s1, $s2
    mfhi  $t2
    beq   $t2, $zero, nprime
    nop
    addiu $s2, $s2, 1
    b     dloop
    nop

nprime:
    addiu $t9, $zero, 0

check:
    beq   $t9, $zero, skip
    nop

    addiu $v0, $zero, 1        # print prime
    addiu $a0, $s1, 0
    syscall
    addiu $v0, $zero, 6
    addiu $a0, $zero, 0x2C     # ','
    syscall
    addiu $a0, $zero, 0x20     # ' '
    syscall
    addiu $s5, $s5, 1

skip:
    addiu $s1, $s1, 1
    b     ploop
    nop

done:
    addiu $v0, $zero, 6        # newline
    addiu $a0, $zero, 0x0A
    syscall
    addiu $v0, $zero, 1        # print count
    addiu $a0, $s5, 0
    syscall
    addiu $v0, $zero, 6
    addiu $a0, $zero, 0x0A
    syscall
    addiu $v0, $zero, 9        # halt
    syscall
