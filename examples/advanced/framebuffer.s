# SageMips Advanced Example: Framebuffer Graphics
# Draws a pattern to a simulated 320x200 framebuffer using MIPS asm
#
#   sagemips asm examples/advanced/framebuffer.s
#   sagemips run framebuffer.mips

    .text
    .globl main

# Framebuffer: 320x200 pixels at address in $s0
# Each pixel is 4 bytes (RGBA)
# Syscall 20: fb_init(width, height) -> pointer
# Syscall 21: fb_set_pixel(x, y, color)
# Syscall 22: fb_flush()

main:
    # Initialize framebuffer 320x200
    addiu $v0, $zero, 20        # fb_init
    addiu $a0, $zero, 320
    addiu $a1, $zero, 200
    syscall
    move  $s0, $v0              # fb pointer

    # Draw horizontal gradient (red increasing left->right)
    addiu $s1, $zero, 0         # y = 0
y_loop:
    slti  $t0, $s1, 200
    beq   $t0, $zero, done_fb
    nop
    addiu $s2, $zero, 0         # x = 0
x_loop:
    slti  $t0, $s2, 320
    beq   $t0, $zero, next_y
    nop

    # color = (x, y, (x+y)/2)
    move  $a0, $s2              # x
    move  $a1, $s1              # y
    add   $t0, $s2, $s1
    srl   $a2, $t0, 1           # blue = (x+y)/2
    addiu $v0, $zero, 21        # fb_set_pixel
    syscall

    addiu $s2, $s2, 1
    b     x_loop
    nop
next_y:
    addiu $s1, $s1, 1
    b     y_loop
    nop

done_fb:
    # Draw a white crosshair
    addiu $s1, $zero, 90        # y center - 10
ch_y:
    slti  $t0, $s1, 110
    beq   $t0, $zero, ch_done
    nop
    # Horizontal line at y
    addiu $a0, $zero, 150       # x center - 10
    move  $a1, $s1
    addiu $a2, $zero, 255       # white
    addiu $v0, $zero, 21
    syscall
    # Vertical line at x
    addiu $a0, $zero, 160
    move  $a1, $s1
    addiu $a2, $zero, 255
    syscall
    addiu $s1, $s1, 1
    b     ch_y
    nop
ch_done:

    # Flush framebuffer
    addiu $v0, $zero, 22        # fb_flush
    syscall

    # Halt
    addiu $v0, $zero, 9
    syscall
