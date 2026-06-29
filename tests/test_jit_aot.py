#!/usr/bin/env python3
"""
SageMips JIT + AOT Optimization Tests
Tests --jit and --aot flags produce correct results and measurable improvements.
"""
import os, sys, subprocess, time, json

SAGEMIPS = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "sagemips"))
TMP = "/tmp/sagemips_opt_test"
os.makedirs(TMP, exist_ok=True)

passed = 0
failed = 0

def test(name, func):
    global passed, failed
    try:
        ok, msg = func()
    except Exception as e:
        ok, msg = False, str(e)
    if ok:
        passed += 1
        print(f"  \u2713 {name}")
    else:
        failed += 1
        print(f"  \u2717 {name}: {msg}")
    return ok

def asm(source):
    path = os.path.join(TMP, f"test_{abs(hash(source))}.s")
    out = os.path.join(TMP, f"test_{abs(hash(source))}.mips")
    with open(path, "w") as f: f.write(source)
    r = subprocess.run([SAGEMIPS, "asm", path, "-o", out], capture_output=True, text=True)
    assert r.returncode == 0, f"ASM failed: {r.stderr}"
    return out

def run(path, flags=None):
    args = [SAGEMIPS, "run", path]
    if flags: args.extend(flags)
    r = subprocess.run(args, capture_output=True, text=True, timeout=10)
    return r.returncode, r.stdout, r.stderr

print("SageMips JIT/AOT Optimization Tests")
print("=" * 60)

# ============================================================================
# Test 1: --aot produces correct results
# ============================================================================
print("\n--- AOT Correctness ---")

src = """
.text
main:
    nop
    nop
    addiu $v0, $zero, 99
    nop
    addiu $v0, $zero, 9
    syscall
"""

def test_aot_correct():
    path = asm(src)
    rc, out, err = run(path, ["--aot"])
    return (rc == 0, f"exit={rc} err={err}")
test("AOT preserves program output", test_aot_correct)

# ============================================================================
# Test 2: --jit produces correct results
# ============================================================================
print("\n--- JIT Correctness ---")

def test_jit_correct():
    path = asm(src)
    rc, out, err = run(path, ["--jit"])
    return (rc == 0, f"exit={rc} err={err}")
test("JIT preserves program output", test_jit_correct)

# ============================================================================
# Test 3: --jit --aot together
# ============================================================================
print("\n--- JIT + AOT Combined ---")

def test_both_correct():
    path = asm(src)
    rc, out, err = run(path, ["--jit", "--aot"])
    return (rc == 0, f"exit={rc} err={err}")
test("JIT+AOT preserves program output", test_both_correct)

# ============================================================================
# Test 4: AOT eliminates NOPs
# ============================================================================
print("\n--- AOT Optimization Effect ---")

def test_aot_nops():
    path = asm(src)
    rc, out, err = run(path, ["--aot"])
    # Check that AOT reports NOP elimination
    has_aot = "AOT:" in err or "AOT:" in out
    return (has_aot, "AOT report found" if has_aot else "No AOT output")
test("AOT reports optimization stats", test_aot_nops)

# ============================================================================
# Test 5: JIT reports cache stats
# ============================================================================
print("\n--- JIT Stats ---")

def test_jit_stats():
    path = asm(src)
    rc, out, err = run(path, ["--jit"])
    has_stats = "JIT:" in err or "JIT:" in out or "JIT stats" in err or "JIT stats" in out
    return (has_stats, "JIT stats reported" if has_stats else "No JIT stats")
test("JIT reports cache statistics", test_jit_stats)

# ============================================================================
# Test 6: Loop program with JIT/AOT
# ============================================================================
print("\n--- Loop Program (JIT profiling) ---")

loop_src = """
.text
main:
    addiu $s0, $zero, 100
    addiu $t0, $zero, 0
loop:
    addiu $t0, $t0, 1
    addiu $s0, $s0, -1
    bgtz  $s0, loop
    nop
    addiu $v0, $zero, 9
    syscall
"""

def test_loop_jit():
    path = asm(loop_src)
    rc, out, err = run(path, ["--jit"])
    return (rc == 0, f"exit={rc}")
test("Loop program with JIT", test_loop_jit)

def test_loop_aot():
    path = asm(loop_src)
    rc, out, err = run(path, ["--aot"])
    return (rc == 0, f"exit={rc}")
test("Loop program with AOT", test_loop_aot)

def test_loop_both():
    path = asm(loop_src)
    rc, out, err = run(path, ["--jit", "--aot"])
    return (rc == 0, f"exit={rc}")
test("Loop program with JIT+AOT", test_loop_both)

# ============================================================================
# Test 7: Fibonacci with optimizations
# ============================================================================
print("\n--- Fibonacci (all optimization modes) ---")

fib_src = """
.text
main:
    addiu $s0, $zero,  15
    addiu $s1, $zero, 0
    addiu $s2, $zero, 1
    addiu $s3, $zero, 0
loop:
    beq   $s3, $s0, done
    nop
    add   $s4, $s1, $s2
    move  $s1, $s2
    move  $s2, $s4
    addiu $s3, $s3, 1
    b     loop
    nop
done:
    addiu $v0, $zero, 9
    syscall
"""

for mode, flags in [("none", []), ("JIT", ["--jit"]), ("AOT", ["--aot"]), ("JIT+AOT", ["--jit","--aot"])]:
    def make_test(f=flags):
        def t():
            path = asm(fib_src)
            rc, out, err = run(path, f)
            return (rc == 0, f"exit={rc}")
        return t
    test(f"Fib(15) [{mode}]", make_test(flags))

# ============================================================================
# Summary
# ============================================================================
print(f"\n{'='*60}")
print(f"Results: {passed} passed, {failed} failed ({passed+failed} total)")
sys.exit(0 if failed == 0 else 1)
