#!/usr/bin/env python3
"""
SageMips Multi-Mode Benchmarks — None vs JIT vs AOT vs JIT+AOT
"""
import os, sys, subprocess, time, json

SAGEMIPS = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "sagemips"))
TMP = "/tmp/sagemips_bench_multi"
os.makedirs(TMP, exist_ok=True)

def asm(source):
    path = os.path.join(TMP, f"b_{abs(hash(source))}.s")
    out = os.path.join(TMP, f"b_{abs(hash(source))}.mips")
    with open(path, "w") as f: f.write(source)
    r = subprocess.run([SAGEMIPS, "asm", path, "-o", out], capture_output=True, text=True)
    assert r.returncode == 0, f"ASM failed: {r.stderr}"
    return out

def bench(name, source, iterations, flags=None):
    path = asm(source)
    if flags is None: flags = []
    args = [SAGEMIPS, "run", path] + flags + ["2>/dev/null"]

    # Estimate instruction count
    lines = [l.strip() for l in source.split("\n") if l.strip() and not l.strip().startswith(".")]
    loop_inst = 0
    in_loop = False
    for line in lines:
        if "loop" in line.lower() and ":" in line: in_loop = True; continue
        if in_loop and ":" in line: break
        if in_loop and line and not line.startswith("#"): loop_inst += 1
    if loop_inst == 0: loop_inst = len([l for l in lines if l and not l.startswith("#")])
    total_instr = loop_inst * iterations + 5

    times = []
    for i in range(5):
        start = time.perf_counter()
        r = subprocess.run(args, shell=True, capture_output=True, text=True, timeout=30)
        elapsed = (time.perf_counter() - start) * 1000
        if r.returncode != 0 and r.returncode != 255:
            print(f"  WARN: {name} exit={r.returncode}")
        times.append(elapsed)
    times.sort()
    median_ms = times[2]
    mips_rate = (total_instr / median_ms) * 1000 / 1e6 if median_ms > 0 else 0

    return {"name": name, "iterations": iterations, "instructions": total_instr,
            "time_ms": round(median_ms, 2), "mips": round(mips_rate, 2),
            "ns_per_instr": round((median_ms * 1e6) / total_instr, 1) if total_instr > 0 else 0}

# ============================================================================
# Benchmark Programs
# ============================================================================

BENCHMARKS = {
    "Integer Arithmetic": """
.text
main:
    addiu $s0, $zero, 100000
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
""",

    "Branch Heavy": """
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
""",

    "Memory R/W": """
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
""",

    "Multiply Heavy": """
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
""",

    "Shift Operations": """
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
""",

    "Mixed ALU": """
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
""",
}

MODES = [
    ("None",     []),
    ("JIT",      ["--jit"]),
    ("AOT",      ["--aot"]),
    ("JIT+AOT",  ["--jit", "--aot"]),
]

# ============================================================================
# Run Benchmarks
# ============================================================================

print("SageMips Multi-Mode Benchmark Suite")
print("=" * 70)

all_results = {}

for mode_name, mode_flags in MODES:
    print(f"\n{'─'*50}")
    print(f"  Mode: {mode_name}")
    print(f"{'─'*50}")
    mode_results = []
    for bench_name, bench_src in BENCHMARKS.items():
        iters = 100000 if "100000" in bench_src else 50000
        if "50000" in bench_src: iters = 50000
        r = bench(bench_name, bench_src, iters, mode_flags)
        mode_results.append(r)
        print(f"  {r['name']:<25} {r['time_ms']:>7.1f}ms  {r['mips']:>8.1f} MIPS  {r['instructions']:>10,} instr")
    all_results[mode_name] = mode_results

# ============================================================================
# Comparison Table
# ============================================================================
print(f"\n{'='*90}")
print(f"  Multi-Mode Comparison")
print(f"{'='*90}")
print(f"{'Benchmark':<25} {'None':>8} {'JIT':>8} {'AOT':>8} {'JIT+AOT':>8}")
print(f"{'─'*25} {'─'*8} {'─'*8} {'─'*8} {'─'*8}")

for i, name in enumerate(BENCHMARKS.keys()):
    vals = []
    for mode_name, _ in MODES:
        vals.append(f"{all_results[mode_name][i]['mips']:>7.1f}")
    print(f"{name:<25} {vals[0]} {vals[1]} {vals[2]} {vals[3]}")

# Averages
print(f"{'─'*25} {'─'*8} {'─'*8} {'─'*8} {'─'*8}")
for mode_name, _ in MODES:
    avg = sum(r["mips"] for r in all_results[mode_name]) / len(all_results[mode_name])
    all_results[mode_name + "_avg"] = round(avg, 1)

avgs = [f"{all_results[m+'_avg']:>7.1f}" for m, _ in MODES]
print(f"{'AVERAGE MIPS':<25} {avgs[0]} {avgs[1]} {avgs[2]} {avgs[3]}")

# Speedup vs baseline
print(f"{'─'*25} {'─'*8} {'─'*8} {'─'*8} {'─'*8}")
base_avg = all_results["None_avg"]
for mode_name, _ in MODES:
    if mode_name == "None": continue
    speedup = (all_results[mode_name + "_avg"] / base_avg - 1) * 100
    all_results[mode_name + "_speedup"] = round(speedup, 1)

print(f"{'SPEEDUP':<25} {'1.00x':>8}", end="")
for mode_name, _ in MODES:
    if mode_name == "None": continue
    sp = all_results[mode_name + "_speedup"]
    print(f" {sp:>+6.1f}%", end="")
print()

print(f"\n{'='*50}")
print(f"  Summary")
print(f"{'='*50}")
for mode_name, _ in MODES:
    avg = all_results[mode_name + "_avg"]
    si = all_results[mode_name][0]["instructions"] * len(BENCHMARKS)
    tt = sum(r["time_ms"] for r in all_results[mode_name])
    print(f"  {mode_name:<10} {avg:>7.1f} MIPS avg, {tt:>7.1f}ms total")
    if mode_name != "None":
        sp = all_results[mode_name + "_speedup"]
        tag = "faster" if sp > 0 else "slower"
        print(f"             {abs(sp):.1f}% {tag} than baseline")

# ============================================================================
# Write JSON
# ============================================================================
output = {"modes": MODES, "benchmarks": all_results}
json_path = os.path.join(os.path.dirname(__file__), "benchmark_multi_results.json")
with open(json_path, "w") as f:
    json.dump(output, f, indent=2, default=str)
print(f"\nResults: {json_path}")
