# SageMips Examples

Complete examples demonstrating MIPS assembly, Sage, and C compilation with SageMips.

## Directory Structure

```
examples/
├── mips/           # MIPS32 assembly examples (always work)
│   ├── hello.s         Hello World with string+int printing
│   ├── fibonacci.s     Iterative Fibonacci (20 terms)
│   ├── factorial.s     Recursive factorial(10) = 3628800
│   ├── primes.s        Prime sieve up to 200 (46 primes)
│   ├── gcd.s           Euclidean GCD algorithm
│   ├── bubble_sort.s   Bubble sort on 10-element array
│   ├── matrix_mul.s    2×2 matrix multiplication
│   └── memcpy.s        Custom memcpy + strlen implementation
├── sage/           # Sage source (requires sage host compiler)
│   ├── hello.sage       Hello World
│   ├── math_demo.sage   Fibonacci, GCD, factorial, array sum
│   ├── primes.sage      Prime sieve with is_prime()
│   ├── bubble_sort.sage Bubble sort
│   └── recursion.sage   Recursive factorial, fib, Ackermann
└── c/              # Freestanding C (requires mips-linux-gnu-gcc)
    ├── hello.c          Hello World with syscall wrappers
    ├── fib.c            Iterative Fibonacci (20 terms)
    ├── primes.c         Prime counting up to 500
    └── gcd_fact.c       GCD + recursive factorial
```

## Quick Start

### MIPS Assembly (no dependencies)

```bash
# Compile and run any assembly example
sagemips asm examples/mips/hello.s
sagemips run hello.mips

# With trace output to see every instruction
sagemips run hello.mips --trace
```

### Sage (requires `sage` on PATH)

```bash
# Compile Sage to MIPS and run
sagemips compile examples/sage/hello.sage
sagemips run hello.mips
```

### C (requires `mips-linux-gnu-gcc`)

```bash
# Cross-compile C to MIPS and run
sagemips cc examples/c/hello.c
sagemips run hello.mips
```

## Run All MIPS Examples

```bash
for f in examples/mips/*.s; do
    echo "=== $(basename $f) ==="
    sagemips asm "$f" && sagemips run "${f%.s}.mips" && echo ""
done
```

## Output Examples

### examples/mips/hello.s
```
Hello, SageMips!
Answer: 42
```

### examples/mips/fibonacci.s
```
Fibonacci Sequence (first 20):
0 1 1 2 3 5 8 13 21 34 55 89 144 233 377 610 987 1597 2584 4181
```

### examples/mips/factorial.s
```
factorial(10) = 3628800
```

### examples/mips/primes.s
```
Primes up to 200:
2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71,
73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151,
157, 163, 167, 173, 179, 181, 191, 193, 197, 199
Count: 46
```

### examples/mips/gcd.s
```
GCD(1071, 462) = 21
GCD(252, 105) = 21
GCD(0, 5)     = 5
GCD(17, 0)    = 17
```

### examples/mips/bubble_sort.s
```
Before: 64 34 25 12 22 11 90 5 42 7
After:  5 7 11 12 22 25 34 42 64 90
```

### examples/mips/matrix_mul.s
```
Matrix Multiplication (2x2)

A:
  [ 1 2 ]
  [ 3 4 ]
B:
  [ 5 6 ]
  [ 7 8 ]
C = A * B:
  [ 19 22 ]
  [ 43 50 ]
```

### examples/mips/memcpy.s
```
Source: Copied successfully!
Dest:   Copied successfully!
```
