/*
 * SageMips Example: Prime Counting (C, freestanding)
 * Count primes up to 500 and print them
 * Compile: sagemips cc examples/c/primes.c
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

static const char header[] = "Primes up to 500:\n";
static const char count_msg[] = "\nCount: ";

/* Check if n is prime */
static int is_prime(int n) {
    int d;
    if (n < 2) return 0;
    for (d = 2; d * d <= n; d++) {
        if (n % d == 0) return 0;
    }
    return 1;
}

void main(void) {
    int count = 0, n;
    print_str(header);

    for (n = 2; n <= 500; n++) {
        if (is_prime(n)) {
            print_int(n);
            count++;
            if (n < 500) {
                __asm__ volatile (
                    "addiu $v0, $zero, 6\n\t"
                    "addiu $a0, $zero, ','\n\t"
                    "syscall\n\t"
                    "addiu $a0, $zero, ' '\n\t"
                    "syscall"
                    : : : "$v0", "$a0"
                );
            }
        }
    }

    print_str(count_msg);
    print_int(count);
    __asm__ volatile (
        "addiu $v0, $zero, 6\n\t"
        "addiu $a0, $zero, 0x0A\n\t"
        "syscall\n\t"
        "addiu $v0, $zero, 9\n\t"
        "syscall"
        : : : "$v0", "$a0"
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
