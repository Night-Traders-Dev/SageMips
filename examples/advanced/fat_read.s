# SageMips Advanced Example: FAT12/16/32 Filesystem Reader
# Reads and displays the root directory of a FAT filesystem
#
#   sagemips asm examples/advanced/fat_read.s
#   sagemips run fat_read.mips
#
# Syscalls:
#   40: fs_open(path) -> fd
#   41: fs_read(fd, buf, offset, len) -> bytes
#   42: fs_close(fd)
#   43: fs_listdir(path) -> count (prints names)

    .text
    .globl main

main:
    # List root directory
    addiu $v0, $zero, 2
    la    $a0, header
    syscall

    addiu $v0, $zero, 43        # fs_listdir
    la    $a0, root_path         # "/"
    syscall
    move  $s0, $v0              # file count

    # Print count
    addiu $v0, $zero, 2
    la    $a0, count_msg
    syscall
    addiu $v0, $zero, 1
    move  $a0, $s0
    syscall
    addiu $v0, $zero, 6
    addiu $a0, $zero, 0x0A
    syscall

    # Try to open and read "README.TXT"
    addiu $v0, $zero, 2
    la    $a0, read_header
    syscall

    addiu $v0, $zero, 40        # fs_open
    la    $a0, readme_path
    syscall
    move  $s1, $v0              # fd

    # Check if file exists (fd >= 0)
    bltz  $s1, file_not_found
    nop

    # Read first 512 bytes
    addiu $v0, $zero, 41        # fs_read
    move  $a0, $s1
    la    $a1, file_buf
    addiu $a2, $zero, 0         # offset
    addiu $a3, $zero, 512       # length
    syscall
    move  $s2, $v0              # bytes read

    # Print file contents
    addiu $v0, $zero, 2
    la    $a0, file_buf
    syscall
    addiu $v0, $zero, 6
    addiu $a0, $zero, 0x0A
    syscall

    # Close file
    addiu $v0, $zero, 42        # fs_close
    move  $a0, $s1
    syscall
    b     done_fat
    nop

file_not_found:
    addiu $v0, $zero, 2
    la    $a0, not_found_msg
    syscall

done_fat:
    addiu $v0, $zero, 9
    syscall

    .data
header:       .asciiz "FAT Filesystem Reader\n"
count_msg:    .asciiz "Files in root: "
read_header:  .asciiz "Reading README.TXT:\n"
readme_path:  .asciiz "/README.TXT"
not_found_msg: .asciiz "File not found\n"
root_path:    .asciiz "/"
file_buf:     .space 512
