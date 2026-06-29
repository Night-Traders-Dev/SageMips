# SageMips Example: Bubble Sort
# Sorts [64,34,25,12,22,11,90,5,42,7] -> [5,7,11,12,22,25,34,42,64,90]

    .text
    .globl main

print_arr:                     # print_arr(arr:a0, len:a1)
    addiu $sp, $sp, -8
    sw    $ra, 4($sp)
    sw    $s0, 0($sp)
    move  $s0, $a0
    move  $t0, $zero

pa_lp:
    beq   $t0, $a1, pa_ex
    nop
    lw    $a0, 0($s0)
    addiu $v0, $zero, 1
    syscall
    addiu $v0, $zero, 6
    addiu $a0, $zero, 0x20
    syscall
    addiu $s0, $s0, 4
    addiu $t0, $t0, 1
    b     pa_lp
    nop
pa_ex:
    addiu $v0, $zero, 6
    addiu $a0, $zero, 0x0A
    syscall
    lw    $s0, 0($sp)
    lw    $ra, 4($sp)
    addiu $sp, $sp, 8
    jr    $ra
    nop

bubble:                        # bubble(arr:a0, len:a1)
    addiu $sp, $sp, -24
    sw    $ra, 20($sp)
    sw    $s0, 16($sp)         # arr
    sw    $s1, 12($sp)         # len
    sw    $s2, 8($sp)          # i
    sw    $s3, 4($sp)          # j
    sw    $s4, 0($sp)          # tmp

    move  $s0, $a0
    move  $s1, $a1
    addiu $s2, $zero, 0

outer:
    slt   $t0, $s2, $s1
    beq   $t0, $zero, bdone
    nop
    addiu $s3, $zero, 0
    subu  $t1, $s1, $s2
    addiu $t1, $t1, -1

inner:
    slt   $t0, $s3, $t1
    beq   $t0, $zero, inext
    nop
    sll   $t2, $s3, 2
    add   $t3, $s0, $t2
    lw    $t4, 0($t3)
    lw    $t5, 4($t3)
    slt   $t0, $t5, $t4
    beq   $t0, $zero, nosw
    nop
    sw    $t5, 0($t3)
    sw    $t4, 4($t3)
nosw:
    addiu $s3, $s3, 1
    b     inner
    nop
inext:
    addiu $s2, $s2, 1
    b     outer
    nop

bdone:
    lw    $s4, 0($sp)
    lw    $s3, 4($sp)
    lw    $s2, 8($sp)
    lw    $s1, 12($sp)
    lw    $s0, 16($sp)
    lw    $ra, 20($sp)
    addiu $sp, $sp, 24
    jr    $ra
    nop

main:
    la    $a0, arr
    addiu $a1, $zero, 10
    jal   print_arr
    nop
    la    $a0, arr
    addiu $a1, $zero, 10
    jal   bubble
    nop
    la    $a0, arr
    addiu $a1, $zero, 10
    jal   print_arr
    nop
    addiu $v0, $zero, 9
    syscall

    .data
arr: .word 64, 34, 25, 12, 22, 11, 90, 5, 42, 7
