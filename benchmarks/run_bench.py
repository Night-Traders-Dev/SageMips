#!/usr/bin/env python3
"""
SageMips Performance Benchmarks
Measures VM instruction throughput, loop performance, branch prediction, etc.
Outputs: benchmark_results.json (for README visualization)
"""
import os, sys, subprocess, time, json, math

SAGEMIPS = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "sagemips"))
if not os.path.exists(SAGEMIPS):
    print("Error: sagemips not built. Run: make")
    sys.exit(1)

TMP = "/tmp/sagemips_bench"
os.makedirs(TMP, exist_ok=True)

def asm(source):
    path = os.path.join(TMP, f"bench_{abs(hash(source))}.s")
    out = os.path.join(TMP, f"bench_{abs(hash(source))}.mips")
    with open(path, "w") as f:
        f.write(source)
    r = subprocess.run([SAGEMIPS, "asm", path, out], capture_output=True, text=True)
    assert r.returncode == 0, f"ASM failed: {r.stderr}"
    return out

def bench(name, source, iterations):
    """Benchmark a MIPS program. Returns (total_time_ms, instructions_executed, mips_rate)"""
    path = asm(source)
    # Estimate instruction count from source (rough)
    lines = [l.strip() for l in source.split("\n") if l.strip() and not l.strip().startswith(".")]
    # Count instructions in loop body
    loop_instructions = 0
    in_loop = False
    for line in lines:
        if "loop" in line.lower() and ":" in line:
            in_loop = True
            continue
        if in_loop and ":" in line:
            break
        if in_loop and line and not line.startswith("#"):
            loop_instructions += 1
    if loop_instructions == 0:
        loop_instructions = len([l for l in lines if l and not l.startswith("#")])
    
    total_instr = loop_instructions * iterations + 5  # +5 for setup/teardown
    
    times = []
    for i in range(5):  # 5 runs for stability
        start = time.perf_counter()
        r = subprocess.run([SAGEMIPS, "run", path], capture_output=True, text=True, timeout=30)
        elapsed = (time.perf_counter() - start) * 1000  # ms
        if r.returncode != 0:
            print(f"  Warning: bench {name} exit={r.returncode}")
        times.append(elapsed)
    
    # Use median
    times.sort()
    median_ms = times[2]
    mips_rate = (total_instr / median_ms) * 1000 / 1e6 if median_ms > 0 else 0
    
    return {
        "name": name,
        "iterations": iterations,
        "instructions": total_instr,
        "time_ms": round(median_ms, 2),
        "mips": round(mips_rate, 2),
        "ns_per_instr": round((median_ms * 1e6) / total_instr, 1) if total_instr > 0 else 0,
    }

def ops_per_sec(name, total_ops, time_ms):
    return round(total_ops / (time_ms / 1000), 2) if time_ms > 0 else 0

results = []

print("SageMips Benchmark Suite")
print("=" * 60)

# ============================================================================
# 1. Integer Arithmetic Throughput
# ============================================================================
print("\n--- Integer Arithmetic ---")

# add + addiu + sub + mul + and + or + xor loop
arith_source = """
.text
main:
    addiu $s0, $zero, 100000   # iterations
    addiu $t0, $zero, 0
    addiu $t1, $zero, 1
    addiu $t2, $zero, 2
loop_a:
    add   $t3, $t0, $t1
    addiu $t4, $t3, 3
    sub   $t5, $t4, $t2
    mul   $t6, $t1, $t2
    and   $t7, $t0, $t1
    or    $t8, $t1, $t2
    xor   $t9, $t0, $t2
    addiu $s0, $s0, -1
    bgtz  $s0, loop_a
    nop
    addiu $v0, $zero, 9
    syscall
"""
r = bench("Integer Arithmetic (7 ops/iter)", arith_source, 100000)
r["ops_per_sec"] = ops_per_sec("arith", 7 * 100000, r["time_ms"])
results.append(r)
print(f"  {r['name']}: {r['time_ms']}ms, {r['mips']} MIPS, {r['ns_per_instr']}ns/instr, {r['ops_per_sec']:.0f} ops/sec")

# ============================================================================
# 2. Branch Performance
# ============================================================================
print("\n--- Branch Performance ---")

branch_source = """
.text
main:
    addiu $s0, $zero, 50000
    addiu $t0, $zero, 0
loop_b:
    beq   $t0, $t0, skip1
    nop
    addiu $t1, $t1, 1
skip1:
    bne   $t0, $t1, skip2
    nop
    addiu $t2, $t2, 2
skip2:
    addiu $t0, $t0, 1
    bgtz  $s0, loop_b
    addiu $s0, $s0, -1
    addiu $v0, $zero, 9
    syscall
"""
r = bench("Branch Heavy (beq+bne+bgtz)", branch_source, 50000)
results.append(r)
print(f"  {r['name']}: {r['time_ms']}ms, {r['mips']} MIPS, {r['ns_per_instr']}ns/instr")

# ============================================================================
# 3. Memory Operations
# ============================================================================
print("\n--- Memory Operations ---")

mem_source = """
.text
main:
    addiu $s0, $zero, 50000
    addiu $sp, $sp, -64
    addiu $t0, $zero, 42
loop_m:
    sw    $t0, 0($sp)
    sw    $t0, 4($sp)
    sw    $t0, 8($sp)
    sw    $t0, 12($sp)
    lw    $t1, 0($sp)
    lw    $t2, 4($sp)
    lw    $t3, 8($sp)
    lw    $t4, 12($sp)
    addiu $s0, $s0, -1
    bgtz  $s0, loop_m
    nop
    addiu $sp, $sp, 64
    addiu $v0, $zero, 9
    syscall
"""
r = bench("Memory Read/Write (8 mem ops/iter)", mem_source, 50000)
r["ops_per_sec"] = ops_per_sec("mem", 8 * 50000, r["time_ms"])
results.append(r)
print(f"  {r['name']}: {r['time_ms']}ms, {r['mips']} MIPS, {r['ops_per_sec']:.0f} mem ops/sec")

# ============================================================================
# 4. Multiplication Heavy
# ============================================================================
print("\n--- Multiplication ---")

mul_source = """
.text
main:
    addiu $s0, $zero, 50000
    addiu $t0, $zero, 3
    addiu $t1, $zero, 7
loop_mul:
    mul   $t2, $t0, $t1
    mult  $t0, $t1
    mflo  $t3
    mfhi  $t4
    mul   $t5, $t2, $t3
    addiu $s0, $s0, -1
    bgtz  $s0, loop_mul
    nop
    addiu $v0, $zero, 9
    syscall
"""
r = bench("Multiply Heavy (5 mul ops/iter)", mul_source, 50000)
r["ops_per_sec"] = ops_per_sec("mul", 5 * 50000, r["time_ms"])
results.append(r)
print(f"  {r['name']}: {r['time_ms']}ms, {r['mips']} MIPS, {r['ops_per_sec']:.0f} mul/sec")

# ============================================================================
# 5. Division Heavy
# ============================================================================
print("\n--- Division ---")

div_source = """
.text
main:
    addiu $s0, $zero, 20000
    addiu $t0, $zero, 100
    addiu $t1, $zero, 7
loop_d:
    div   $t0, $t1
    mflo  $t2
    mfhi  $t3
    div   $t2, $t1
    mflo  $t4
    mfhi  $t5
    addiu $t0, $t0, 1
    addiu $s0, $s0, -1
    bgtz  $s0, loop_d
    nop
    addiu $v0, $zero, 9
    syscall
"""
r = bench("Division Heavy (2 div/iter)", div_source, 20000)
results.append(r)
print(f"  {r['name']}: {r['time_ms']}ms, {r['mips']} MIPS, {r['ns_per_instr']}ns/instr")

# ============================================================================
# 6. Shift Operations
# ============================================================================
print("\n--- Shift Operations ---")

shift_source = """
.text
main:
    addiu $s0, $zero, 100000
    addiu $t0, $zero, 1
loop_s:
    sll   $t1, $t0, 4
    srl   $t2, $t1, 1
    sra   $t3, $t1, 2
    sllv  $t4, $t0, $t1
    srlv  $t5, $t2, $t0
    srav  $t6, $t3, $t0
    addiu $t0, $t0, 1
    addiu $s0, $s0, -1
    bgtz  $s0, loop_s
    nop
    addiu $v0, $zero, 9
    syscall
"""
r = bench("Shift Operations (6 shift/iter)", shift_source, 100000)
results.append(r)
print(f"  {r['name']}: {r['time_ms']}ms, {r['mips']} MIPS")

# ============================================================================
# 7. Fibonacci (real algorithm)
# ============================================================================
print("\n--- Algorithm: Fibonacci ---")

fib_source = """
.text
main:
    addiu $s0, $zero, 5000
    addiu $s1, $zero, 0
    addiu $s2, $zero, 1
    addiu $s3, $zero, 0
loop_f:
    beq   $s3, $s0, done_f
    nop
    add   $s4, $s1, $s2
    move  $s1, $s2
    move  $s2, $s4
    addiu $s3, $s3, 1
    b     loop_f
    nop
done_f:
    addiu $v0, $zero, 9
    syscall
"""
r = bench("Fibonacci (5000 terms)", fib_source, 5000)
results.append(r)
print(f"  {r['name']}: {r['time_ms']}ms, {r['mips']} MIPS")

# ============================================================================
# 8. Nested Loops
# ============================================================================
print("\n--- Nested Loops ---")

nested_source = """
.text
main:
    addiu $s0, $zero, 200
    addiu $t0, $zero, 0
outer:
    addiu $s1, $zero, 200
    addiu $t1, $zero, 0
inner:
    addiu $t2, $t0, 1
    addiu $t3, $t1, 1
    mul   $t4, $t2, $t3
    addiu $t1, $t1, 1
    addiu $s1, $s1, -1
    bgtz  $s1, inner
    nop
    addiu $t0, $t0, 1
    addiu $s0, $s0, -1
    bgtz  $s0, outer
    nop
    addiu $v0, $zero, 9
    syscall
"""
r = bench("Nested Loops (200x200)", nested_source, 200*200)
results.append(r)
print(f"  {r['name']}: {r['time_ms']}ms, {r['mips']} MIPS, {r['instructions']:,} instr")

# ============================================================================
# 9. Prime Sieve (algorithm)
# ============================================================================
print("\n--- Algorithm: Prime Sieve ---")

# Simple prime counting (not Sieve of Eratosthenes — test divisibility)
prime_source = """
.text
main:
    addiu $s0, $zero, 500    # check primes up to 500
    addiu $s1, $zero, 2      # n = 2
    addiu $s5, $zero, 0      # prime count
ploop:
    slt   $t0, $s1, $s0
    beq   $t0, $zero, pdone
    nop
    # is_prime(n): check divisors 2..sqrt(n)
    addiu $s2, $zero, 2      # d = 2
    addiu $t9, $zero, 1      # assume prime = 1
divloop:
    mul   $t0, $s2, $s2
    slt   $t1, $t0, $s1
    beq   $t1, $zero, pcheck
    nop
    div   $s1, $s2
    mfhi  $t2
    bne   $t2, $zero, notdiv
    nop
    addiu $t9, $zero, 0      # not prime
    b      pcheck
    nop
notdiv:
    addiu $s2, $s2, 1
    b      divloop
    nop
pcheck:
    beq   $t9, $zero, notprime
    nop
    addiu $s5, $s5, 1        # count prime
notprime:
    addiu $s1, $s1, 1
    b      ploop
    nop
pdone:
    addiu $v0, $zero, 9
    syscall
"""
r = bench("Prime Counting (n≤500)", prime_source, 500)
results.append(r)
print(f"  {r['name']}: {r['time_ms']}ms, {r['mips']} MIPS, {r['instructions']:,} instr")

# ============================================================================
# 10. Instruction Mix (stress test)
# ============================================================================
print("\n--- Stress: Mixed Instruction Stream ---")

mix_source = """
.text
main:
    addiu $s0, $zero, 100000
    addiu $t0, $zero, 0
    addiu $t1, $zero, 1
    addiu $t2, $zero, 2
mix_loop:
    add   $t3, $t0, $t1
    sub   $t4, $t3, $t2
    and   $t5, $t4, $t1
    or    $t6, $t5, $t2
    xor   $t7, $t6, $t1
    sll   $t8, $t7, 1
    srl   $t9, $t8, 1
    slt   $k0, $t0, $t1
    mul   $k1, $t1, $t2
    addiu $t0, $t0, 1
    addiu $s0, $s0, -1
    bgtz  $s0, mix_loop
    nop
    addiu $v0, $zero, 9
    syscall
"""
r = bench("Mixed ALU (10 ops/iter x 100k)", mix_source, 100000)
results.append(r)
print(f"  {r['name']}: {r['time_ms']}ms, {r['mips']} MIPS, {r['instructions']:,} instr")

# ============================================================================
# Summary
# ============================================================================
print(f"\n{'='*60}")
print("Benchmark Summary")
print(f"{'='*60}")
print(f"{'Benchmark':<40} {'Time':>8} {'MIPS':>8} {'Instr':>10}")
print("-" * 68)
for r in results:
    print(f"{r['name']:<40} {r['time_ms']:>6.1f}ms {r['mips']:>7.2f} {r['instructions']:>10,}")

# Compute aggregate
total_time = sum(r["time_ms"] for r in results)
total_instr = sum(r["instructions"] for r in results)
avg_mips = sum(r["mips"] for r in results) / len(results) if results else 0

print(f"\n  Total time: {total_time:.1f}ms")
print(f"  Total instructions: {total_instr:,}")
print(f"  Average MIPS: {avg_mips:.2f}")

# ============================================================================
# Write JSON for README visualization
# ============================================================================
output = {
    "version": "1.0.0",
    "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
    "summary": {
        "total_time_ms": round(total_time, 1),
        "total_instructions": total_instr,
        "average_mips": round(avg_mips, 2),
    },
    "benchmarks": results,
}

json_path = os.path.join(os.path.dirname(__file__), "benchmark_results.json")
with open(json_path, "w") as f:
    json.dump(output, f, indent=2)

print(f"\nResults written to: {json_path}")
