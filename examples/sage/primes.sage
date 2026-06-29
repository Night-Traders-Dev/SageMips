# ============================================================================
# SageMips Example: Prime Sieve (Sage)
# Compile: sagemips compile examples/sage/primes.sage
# ============================================================================

proc is_prime(n):
    if n < 2:
        return false
    let d = 2
    while d * d <= n:
        if n % d == 0:
            return false
        d = d + 1
    return true

proc count_primes(limit):
    let count = 0
    let n = 2
    while n <= limit:
        if is_prime(n):
            count = count + 1
            print str(n) + " "
        n = n + 1
    print ""
    return count

print "=== Primes up to 200 ==="
let c = count_primes(200)
print "Count: " + str(c)
