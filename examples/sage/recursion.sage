# ============================================================================
# SageMips Example: Recursion & Control Flow (Sage)
# Compile: sagemips compile examples/sage/recursion.sage
# ============================================================================

# Recursive factorial with type annotations
proc factorial(n: Int) -> Int:
    if n <= 1:
        return 1
    return n * factorial(n - 1)

# Recursive fibonacci
proc fib(n):
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)

# Ackermann function (deep recursion)
proc ack(m, n):
    if m == 0:
        return n + 1
    elif n == 0:
        return ack(m - 1, 1)
    else:
        return ack(m - 1, ack(m, n - 1))

print "=== Recursion Examples ==="
print ""

print "Factorial(8) = " + str(factorial(8))
print ""

print "Fibonacci numbers (recursive):"
let i = 0
while i < 12:
    print str(fib(i)) + " "
    i = i + 1
print ""
print ""

print "Ackermann(3, 3) = " + str(ack(3, 3))
