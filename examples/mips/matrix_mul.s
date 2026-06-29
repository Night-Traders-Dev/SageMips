# SageMips Example: 2x2 Matrix Multiplication
# C = A * B where A=[1,2;3,4] B=[5,6;7,8] C=[19,22;43,50]

    .text
    .globl main

pmat:                          # print_flat_4(matrix:a0)
    addiu $sp, $sp, -8
    sw    $ra, 4($sp)
    sw    $s0, 0($sp)
    move  $s0, $a0
    move  $t0, $zero
plp:
    slti  $t1, $t0, 4
    beq   $t1, $zero, pmat_ex
    nop
    lw    $a0, 0($s0)
    addiu $v0, $zero, 1
    syscall
    addiu $v0, $zero, 6
    addiu $a0, $zero, 0x20
    syscall
    addiu $s0, $s0, 4
    addiu $t0, $t0, 1
    b     plp
    nop
pmat_ex:
    addiu $v0, $zero, 6
    addiu $a0, $zero, 0x0A
    syscall
    lw    $s0, 0($sp)
    lw    $ra, 4($sp)
    addiu $sp, $sp, 8
    jr    $ra
    nop

main:
    la    $a0, A
    jal   pmat
    nop
    la    $a0, B
    jal   pmat
    nop

    # C = A * B
    la    $t0, A
    la    $t1, B
    la    $t2, C

    # C[0] = A[0]*B[0] + A[1]*B[2]
    lw    $t3, 0($t0)
    lw    $t4, 0($t1)
    mul   $t5, $t3, $t4
    lw    $t3, 4($t0)
    lw    $t4, 8($t1)
    mul   $t6, $t3, $t4
    add   $t7, $t5, $t6
    sw    $t7, 0($t2)

    # C[1] = A[0]*B[1] + A[1]*B[3]
    lw    $t3, 0($t0)
    lw    $t4, 4($t1)
    mul   $t5, $t3, $t4
    lw    $t3, 4($t0)
    lw    $t4, 12($t1)
    mul   $t6, $t3, $t4
    add   $t7, $t5, $t6
    sw    $t7, 4($t2)

    # C[2] = A[2]*B[0] + A[3]*B[2]
    lw    $t3, 8($t0)
    lw    $t4, 0($t1)
    mul   $t5, $t3, $t4
    lw    $t3, 12($t0)
    lw    $t4, 8($t1)
    mul   $t6, $t3, $t4
    add   $t7, $t5, $t6
    sw    $t7, 8($t2)

    # C[3] = A[2]*B[1] + A[3]*B[3]
    lw    $t3, 8($t0)
    lw    $t4, 4($t1)
    mul   $t5, $t3, $t4
    lw    $t3, 12($t0)
    lw    $t4, 12($t1)
    mul   $t6, $t3, $t4
    add   $t7, $t5, $t6
    sw    $t7, 12($t2)

    la    $a0, C
    jal   pmat
    nop

    addiu $v0, $zero, 9
    syscall

    .data
A: .word 1, 2, 3, 4
B: .word 5, 6, 7, 8
C: .word 0, 0, 0, 0
