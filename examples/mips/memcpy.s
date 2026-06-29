# SageMips Example: Memory Copy (custom memcpy + strlen)

    .text
    .globl main

main:
    la    $a0, source
    jal   strlen
    nop
    move  $s0, $v0
    la    $a0, dest
    la    $a1, source
    move  $a2, $s0
    jal   memcpy
    nop
    la    $t0, dest
    add   $t0, $t0, $s0
    sb    $zero, 0($t0)
    la    $a0, source
    addiu $v0, $zero, 2
    syscall
    addiu $v0, $zero, 6
    addiu $a0, $zero, 0x0A
    syscall
    la    $a0, dest
    addiu $v0, $zero, 2
    syscall
    addiu $v0, $zero, 6
    addiu $a0, $zero, 0x0A
    syscall
    addiu $v0, $zero, 9
    syscall

memcpy:                        # memcpy(dest:a0, src:a1, n:a2)
    beq   $a2, $zero, mc_ex
    nop
mc_lp:
    lbu   $t0, 0($a1)
    sb    $t0, 0($a0)
    addiu $a0, $a0, 1
    addiu $a1, $a1, 1
    addiu $a2, $a2, -1
    bgtz  $a2, mc_lp
    nop
mc_ex:
    jr    $ra
    nop

strlen:                        # strlen(s:a0) -> v0
    move  $v0, $zero
sl_lp:
    lbu   $t0, 0($a0)
    beq   $t0, $zero, sl_ex
    nop
    addiu $a0, $a0, 1
    addiu $v0, $v0, 1
    b     sl_lp
    nop
sl_ex:
    jr    $ra
    nop

    .data
source: .asciiz "Copied successfully!"
dest:   .space 64
