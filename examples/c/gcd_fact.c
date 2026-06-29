/*
 * SageMips Example: GCD & Factorial (C, freestanding)
 * Demonstrates function calls and recursion in freestanding C
 * Compile: sagemips cc examples/c/gcd_fact.c
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

/* Euclidean GCD algorithm */
static int gcd(int a, int b) {
    while (b != 0) {
        int t = b;
        b = a % b;
        a = t;
    }
    return a;
}

/* Recursive factorial */
static int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

void main(void) {
    print_str("GCD(1071, 462)   = ");
    print_int(gcd(1071, 462));
    __asm__ volatile (
        "addiu $v0, $zero, 6\n\taddiu $a0, $zero, 0x0A\n\tsyscall"
        : : : "$v0", "$a0"
    );

    print_str("GCD(252, 105)    = ");
    print_int(gcd(252, 105));
    __asm__ volatile (
        "addiu $v0, $zero, 6\n\taddiu $a0, $zero, 0x0A\n\tsyscall"
        : : : "$v0", "$a0"
    );

    print_str("GCD(0, 5)        = ");
    print_int(gcd(0, 5));
    __asm__ volatile (
        "addiu $v0, $zero, 6\n\taddiu $a0, $zero, 0x0A\n\tsyscall"
        : : : "$v0", "$a0"
    );

    print_str("Factorial(7)     = ");
    print_int(factorial(7));
    __asm__ volatile (
        "addiu $v0, $zero, 6\n\taddiu $a0, $zero, 0x0A\n\tsyscall"
        : : : "$v0", "$a0"
    );

    print_str("Factorial(10)    = ");
    print_int(factorial(10));
    __asm__ volatile (
        "addiu $v0, $zero, 6\n\taddiu $a0, $zero, 0x0A\n\tsyscall"
        : : : "$v0", "$a0"
    );

    __asm__ volatile (
        "addiu $v0, $zero, 9\n\tsyscall"
        : : : "$v0"
    );
}

void _start(void) { main(); }
