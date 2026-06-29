#!/usr/bin/env python3
"""
SageMips Multi-Mode Multi-Backend Benchmarks
Tests: C + Sage × (None, JIT, AOT, JIT+AOT) × 6 benchmark programs
"""
import os, sys, subprocess, time, json

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(SCRIPT_DIR)
C_BIN   = os.path.join(ROOT, "sagemips")
SAGE_BIN = os.path.join(ROOT, "sagemips_sage")
TMP = "/tmp/sagemips_bench_all"
os.makedirs(TMP, exist_ok=True)

BACKENDS = [
    ("C",    C_BIN),
    ("Sage", SAGE_BIN),
]

MODES = [
    ("None",     []),
    ("JIT",      ["--jit"]),
    ("AOT",      ["--aot"]),
    ("JIT+AOT",  ["--jit", "--aot"]),
]

def asm(binary, source):
    h = abs(hash(source))
    path = os.path.join(TMP, f"b_{h}.s")
    out  = os.path.join(TMP, f"b_{h}.mips")
    with open(path, "w") as f: f.write(source)
    r = subprocess.run([binary, "asm", path, "-o", out], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"ASM failed: {r.stderr}")
    return out

def bench(binary, name, source, iterations, flags):
    path = asm(binary, source)
    args = [binary, "run", path] + flags

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
    for i in range(3):  # 3 runs for speed
        start = time.perf_counter()
        r = subprocess.run(args, capture_output=True, text=True, timeout=30)
        elapsed = (time.perf_counter() - start) * 1000
        times.append(elapsed)
    times.sort()
    median_ms = times[1]  # median of 3
    mips_rate = (total_instr / median_ms) * 1000 / 1e6 if median_ms > 0 else 0
    return {"name": name, "time_ms": round(median_ms, 2),
            "mips": round(mips_rate, 1), "instructions": total_instr}

BENCHMARKS = {
    "Int Arith": ("""
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
""", 100000),

    "Branch": ("""
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
""", 50000),

    "Mem R/W": ("""
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
""", 50000),

    "Multiply": ("""
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
""", 50000),

    "Shifts": ("""
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
""", 100000),

    "Mixed": ("""
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
""", 100000),
}

# ============================================================================
# Run all benchmarks
# ============================================================================
print("SageMips: C vs Sage × 4 Optimization Modes")
print("=" * 80)

all_data = {}

for bk_name, bk_bin in BACKENDS:
    if not os.path.exists(bk_bin):
        print(f"\n  SKIP {bk_name}: binary not found at {bk_bin}")
        continue

    for mode_name, mode_flags in MODES:
        label = f"{bk_name}/{mode_name}"
        print(f"\n{'─'*50}")
        print(f"  {label}")
        print(f"{'─'*50}")
        mode_results = []

        for prog_name, (prog_src, iters) in BENCHMARKS.items():
            try:
                r = bench(bk_bin, prog_name, prog_src, iters, mode_flags)
                mode_results.append(r)
                print(f"  {r['name']:<15} {r['time_ms']:>7.1f}ms  {r['mips']:>7.1f} MIPS  {r['instructions']:>9,} instr")
            except Exception as e:
                print(f"  {prog_name:<15} FAIL: {e}")
                mode_results.append({"name": prog_name, "time_ms": 0, "mips": 0, "instructions": 0})

        all_data[label] = mode_results

# ============================================================================
# Summary tables
# ============================================================================
print(f"\n{'='*100}")
print(f"  C vs Sage — MIPS Comparison (all modes)")
print(f"{'='*100}")

bench_names = list(BENCHMARKS.keys())

# Table 1: C backend — all modes
print(f"\n{'─'*70}")
print(f"  C Backend — All Optimization Modes")
print(f"{'─'*70}")
header = f"{'Benchmark':<15}"
for _, mn in MODES: header += f" {mn:>10}"
print(header)
print(f"{'─'*15}{'─'*44}")
for i, name in enumerate(bench_names):
    row = f"{name:<15}"
    for _, mn in MODES:
        key = f"C/{mn}"
        if key in all_data and i < len(all_data[key]):
            row += f" {all_data[key][i]['mips']:>9.1f}"
        else:
            row += f" {'—':>9}"
    print(row)
# Averages
print(f"{'─'*15}{'─'*44}")
row = f"{'AVG':<15}"
for _, mn in MODES:
    key = f"C/{mn}"
    if key in all_data:
        vals = [r["mips"] for r in all_data[key] if r["mips"] > 0]
        avg = sum(vals) / len(vals) if vals else 0
        row += f" {avg:>9.1f}"
    else:
        row += f" {'—':>9}"
print(row)

# Table 2: Sage backend — all modes
print(f"\n{'─'*70}")
print(f"  Sage Backend — All Optimization Modes")
print(f"{'─'*70}")
print(header)
print(f"{'─'*15}{'─'*44}")
for i, name in enumerate(bench_names):
    row = f"{name:<15}"
    for _, mn in MODES:
        key = f"Sage/{mn}"
        if key in all_data and i < len(all_data[key]):
            row += f" {all_data[key][i]['mips']:>9.1f}"
        else:
            row += f" {'—':>9}"
    print(row)
print(f"{'─'*15}{'─'*44}")
row = f"{'AVG':<15}"
for _, mn in MODES:
    key = f"Sage/{mn}"
    if key in all_data:
        vals = [r["mips"] for r in all_data[key] if r["mips"] > 0]
        avg = sum(vals) / len(vals) if vals else 0
        row += f" {avg:>9.1f}"
    else:
        row += f" {'—':>9}"
print(row)

# Table 3: C vs Sage head-to-head (best mode each)
print(f"\n{'─'*70}")
print(f"  C vs Sage Head-to-Head (baseline, no optimizations)")
print(f"{'─'*70}")
print(f"{'Benchmark':<15} {'C (MIPS)':>10} {'Sage (MIPS)':>12} {'Ratio':>8}")
print(f"{'─'*15} {'─'*10} {'─'*12} {'─'*8}")
for i, name in enumerate(bench_names):
    c_val = all_data.get("C/None", [{}])[i].get("mips", 0) if i < len(all_data.get("C/None", [])) else 0
    s_val = all_data.get("Sage/None", [{}])[i].get("mips", 0) if i < len(all_data.get("Sage/None", [])) else 0
    ratio = f"{c_val/s_val:.1f}x" if s_val > 0 else "—"
    print(f"{name:<15} {c_val:>9.1f} {s_val:>11.1f} {ratio:>8}")

# Averages
c_vals = [r["mips"] for r in all_data.get("C/None", []) if r.get("mips", 0) > 0]
s_vals = [r["mips"] for r in all_data.get("Sage/None", []) if r.get("mips", 0) > 0]
c_avg = sum(c_vals) / len(c_vals) if c_vals else 0
s_avg = sum(s_vals) / len(s_vals) if s_vals else 0
ratio = f"{c_avg/s_avg:.1f}x" if s_avg > 0 else "—"
print(f"{'─'*15} {'─'*10} {'─'*12} {'─'*8}")
print(f"{'AVG':<15} {c_avg:>9.1f} {s_avg:>11.1f} {ratio:>8}")

# Summary
print(f"\n{'='*60}")
print(f"  Summary")
print(f"{'='*60}")
print(f"  C backend:   {c_avg:.1f} MIPS avg (baseline)")
print(f"  Sage backend: {s_avg:.1f} MIPS avg")
if s_avg > 0:
    print(f"  C is {c_avg/s_avg:.1f}x faster than Sage (baseline)")

# JSON output
output = {"c_binary": C_BIN, "sage_binary": SAGE_BIN,
          "modes": MODES, "benchmarks": all_data}
json_path = os.path.join(SCRIPT_DIR, "benchmark_c_vs_sage.json")
with open(json_path, "w") as f:
    json.dump(output, f, indent=2, default=str)
print(f"\nResults: {json_path}")
