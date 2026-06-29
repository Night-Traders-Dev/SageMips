# ============================================================================
# SageMips Example: Math & Algorithms (Sage)
# Compile: sagemips compile examples/sage/math_demo.sage
# ============================================================================

# Fibonacci (iterative)
proc fib(n):
    let a = 0
    let b = 1
    let i = 0
    while i < n:
        print str(a) + " "
        let c = a + b
        a = b
        b = c
        i = i + 1
    print ""
    return a

# Greatest Common Divisor (Euclidean)
proc gcd(a, b):
    while b != 0:
        let t = b
        b = a % b
        a = t
    return a

# Factorial (recursive)
proc factorial(n):
    if n <= 1:
        return 1
    return n * factorial(n - 1)

# Sum of array
proc array_sum(arr):
    let total = 0
    for val in arr:
        total = total + val
    return total

print "=== SageMips Math Demo ==="
print ""

print "Fibonacci(10):"
let f = fib(10)
print ""

print "GCD(1071, 462) = " + str(gcd(1071, 462))
print "GCD(252, 105)  = " + str(gcd(252, 105))
print ""

print "Factorial(7) = " + str(factorial(7))
print "Factorial(10) = " + str(factorial(10))
print ""

print "Array sum: " + str(array_sum([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]))
