/*
 * SageMips Example: Fibonacci (C, freestanding)
 * Compile: sagemips cc examples/c/fib.c
 */

#define SYSCALL_PRINT_INT 1
#define SYSCALL_PRINT_STR 2
#define SYSCALL_PRINT_CHAR 6
#define SYSCALL_HALT      9

static void print_int(int n) {
    __asm__ volatile (
        "move $v0, %0\n\t"
        "move $a0, %1\n\t"
        "syscall"
        : : "r"(SYSCALL_PRINT_INT), "r"(n)
        : "$v0", "$a0"
    );
}

static void print_str(const char* s) {
    __asm__ volatile (
        "move $v0, %0\n\t"
        "move $a0, %1\n\t"
        "syscall"
        : : "r"(SYSCALL_PRINT_STR), "r"(s)
        : "$v0", "$a0"
    );
}

static void print_char(char c) {
    __asm__ volatile (
        "move $v0, %0\n\t"
        "move $a0, %1\n\t"
        "syscall"
        : : "r"(SYSCALL_PRINT_CHAR), "r"((int)c)
        : "$v0", "$a0"
    );
}

static const char header[] = "Fibonacci (first 20, iterative):\n";

/* Iterative Fibonacci */
static int fib(int n) {
    int a = 0, b = 1, i;
    for (i = 0; i < n; i++) {
        print_int(a);
        print_char(' ');
        int c = a + b;
        a = b;
        b = c;
    }
    return a;
}

void main(void) {
    print_str(header);
    fib(20);
    print_char('\n');

    __asm__ volatile (
        "addiu $v0, $zero, 9\n\t"
        "syscall"
        : : : "$v0"
    );
}

void _start(void) {
    main();
    __asm__ volatile (
        "addiu $v0, $zero, 9\n\t"
        "syscall"
        : : : "$v0"
    );
}
