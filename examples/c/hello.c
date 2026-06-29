/*
 * SageMips Example: Hello World (C, freestanding)
 * Compile: sagemips cc examples/c/hello.c
 * Requires: mips-linux-gnu-gcc (or compatible MIPS cross-compiler)
 *
 * This is compiled with -nostdlib, so no printf/puts available.
 * We make syscalls directly via inline MIPS assembly.
 */

/* SageMips syscall numbers */
#define SYSCALL_EXIT      0
#define SYSCALL_PRINT_INT 1
#define SYSCALL_PRINT_STR 2
#define SYSCALL_HALT      9

/* String data */
static const char msg[] = "Hello from C!\n";
static const char ans[] = "Answer: ";

/* syscall wrapper */
static void syscall1(int num, int a0) {
    __asm__ volatile (
        "move $v0, %0\n\t"
        "move $a0, %1\n\t"
        "syscall"
        : : "r"(num), "r"(a0)
        : "$v0", "$a0"
    );
}

static void syscall2(int num, int a0, int a1) {
    __asm__ volatile (
        "move $v0, %0\n\t"
        "move $a0, %1\n\t"
        "move $a1, %2\n\t"
        "syscall"
        : : "r"(num), "r"(a0), "r"(a1)
        : "$v0", "$a0", "$a1"
    );
}

void main(void) {
    /* Print the message string */
    syscall1(SYSCALL_PRINT_STR, (int)msg);

    /* Print "Answer: " */
    syscall1(SYSCALL_PRINT_STR, (int)ans);

    /* Print integer 42 */
    syscall1(SYSCALL_PRINT_INT, 42);

    /* Print newline via direct char write */
    __asm__ volatile (
        "addiu $v0, $zero, 6\n\t"
        "addiu $a0, $zero, 0x0A\n\t"
        "syscall"
        : : : "$v0", "$a0"
    );

    /* Halt the VM */
    syscall1(SYSCALL_HALT, 0);
}

void _start(void) {
    main();
    /* If main returns, halt */
    __asm__ volatile (
        "addiu $v0, $zero, 9\n\t"
        "syscall"
        : : : "$v0"
    );
}
