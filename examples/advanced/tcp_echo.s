# SageMips Advanced Example: TCP Echo Server
# Listens on port 8080, echoes received data back
#
#   sagemips asm examples/advanced/tcp_echo.s
#   sagemips run tcp_echo.mips
#
# Syscalls used:
#   30: socket(domain, type, proto) -> fd
#   31: bind(fd, addr, port)
#   32: listen(fd, backlog)
#   33: accept(fd) -> client_fd
#   34: recv(fd, buf, len) -> bytes
#   35: send(fd, buf, len) -> bytes
#   36: close(fd)

    .text
    .globl main

main:
    # Create socket (AF_INET=2, SOCK_STREAM=1, TCP=6)
    addiu $v0, $zero, 30        # socket
    addiu $a0, $zero, 2         # AF_INET
    addiu $a1, $zero, 1         # SOCK_STREAM
    addiu $a2, $zero, 6         # TCP
    syscall
    move  $s0, $v0              # server_fd

    # Bind to port 8080
    addiu $v0, $zero, 31        # bind
    move  $a0, $s0
    addiu $a1, $zero, 0         # addr = 0.0.0.0
    addiu $a2, $zero, 8080
    syscall

    # Listen
    addiu $v0, $zero, 32        # listen
    move  $a0, $s0
    addiu $a1, $zero, 5         # backlog
    syscall

    # Print ready message
    addiu $v0, $zero, 2         # print_str
    la    $a0, ready_msg
    syscall

accept_loop:
    # Accept connection
    addiu $v0, $zero, 33        # accept
    move  $a0, $s0
    syscall
    move  $s1, $v0              # client_fd

    # Print connected message
    addiu $v0, $zero, 2
    la    $a0, conn_msg
    syscall

echo_loop:
    # Receive data (up to 1024 bytes)
    addiu $v0, $zero, 34        # recv
    move  $a0, $s1
    la    $a1, recv_buf
    addiu $a2, $zero, 1024
    syscall
    move  $s2, $v0              # bytes_received

    # If 0 bytes, client disconnected
    beq   $s2, $zero, client_done
    nop

    # Echo data back
    addiu $v0, $zero, 35        # send
    move  $a0, $s1
    la    $a1, recv_buf
    move  $a2, $s2
    syscall

    b     echo_loop
    nop

client_done:
    # Close client
    addiu $v0, $zero, 36        # close
    move  $a0, $s1
    syscall

    # Accept next client
    b     accept_loop
    nop

    .data
ready_msg: .asciiz "TCP Echo Server listening on port 8080\n"
conn_msg:  .asciiz "Client connected\n"
recv_buf:  .space 1024
