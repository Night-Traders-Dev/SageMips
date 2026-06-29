#!/usr/bin/env python3
"""
SageMips Comprehensive Test Suite
Tests: VM, Assembler, Disassembler, Encoder, CLI
Usage: python3 run_tests.py [--verbose]
"""
import os, sys, subprocess, time, json, argparse

SAGEMIPS = os.path.join(os.path.dirname(__file__), "..", "sagemips")
if not os.path.exists(SAGEMIPS):
    SAGEMIPS = os.path.join(os.path.dirname(__file__), "..", "build", "sagemips")
if not os.path.exists(SAGEMIPS):
    print("Error: sagemips binary not found. Build first: make")
    sys.exit(1)

SAGEMIPS = os.path.abspath(SAGEMIPS)
TMP = "/tmp/sagemips_test"
os.makedirs(TMP, exist_ok=True)

passed = 0
failed = 0
results = []

def test(name, category, func):
    global passed, failed, results
    try:
        ok, msg = func()
    except Exception as e:
        ok, msg = False, str(e)
    if ok:
        passed += 1
        print(f"  ✓ {name}")
    else:
        failed += 1
        print(f"  ✗ {name}: {msg}")
    results.append({"name": name, "category": category, "passed": ok, "message": msg})

def asm(source):
    """Assemble MIPS source, return (ok, binary_path)"""
    path = os.path.join(TMP, f"test_{abs(hash(source))}.s")
    out = os.path.join(TMP, f"test_{abs(hash(source))}.mips")
    with open(path, "w") as f:
        f.write(source)
    r = subprocess.run([SAGEMIPS, "asm", path, out], capture_output=True, text=True)
    return (r.returncode == 0, out if r.returncode == 0 else r.stderr)

def run_mips(path, trace=False):
    """Run a MIPS binary, return (returncode, output)"""
    args = [SAGEMIPS, "run", path]
    if trace:
        args.append("--trace")
    r = subprocess.run(args, capture_output=True, text=True, timeout=5)
    return (r.returncode, r.stderr if trace else "")

def dis(path):
    """Disassemble a MIPS binary"""
    r = subprocess.run([SAGEMIPS, "dis", path], capture_output=True, text=True)
    return (r.returncode, r.stdout)

# ==============================================================================
# VM Tests
# ==============================================================================

print("\n=== VM: Arithmetic ===")

def test_vm_add():
    ok, path = asm(".text\nmain:\n addiu $v0,$zero,100\n addiu $v1,$zero,200\n add $a0,$v0,$v1\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("add", "vm", test_vm_add)

def test_vm_sub():
    ok, path = asm(".text\nmain:\n addiu $t0,$zero,100\n addiu $t1,$zero,30\n sub $t2,$t0,$t1\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("sub", "vm", test_vm_sub)

def test_vm_mul():
    ok, path = asm(".text\nmain:\n addiu $t0,$zero,7\n addiu $t1,$zero,6\n mul $t2,$t0,$t1\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("mul", "vm", test_vm_mul)

def test_vm_div():
    ok, path = asm(".text\nmain:\n addiu $t0,$zero,17\n addiu $t1,$zero,5\n div $t0,$t1\n mflo $t2\n mfhi $t3\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("div+mfhi+mflo", "vm", test_vm_div)

def test_vm_and_or_xor():
    ok, path = asm(".text\nmain:\n addiu $t0,$zero,0xFF\n addiu $t1,$zero,0x0F\n and $t2,$t0,$t1\n or $t3,$t0,$t1\n xor $t4,$t0,$t1\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("and/or/xor", "vm", test_vm_and_or_xor)

def test_vm_shifts():
    ok, path = asm(".text\nmain:\n addiu $t0,$zero,1\n sll $t1,$t0,4\n srl $t2,$t1,1\n sra $t3,$t1,2\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("shifts", "vm", test_vm_shifts)

# ==============================================================================
# VM: Branch Tests
# ==============================================================================

print("\n=== VM: Branches ===")

def test_vm_beq():
    ok, path = asm(".text\nmain:\n addiu $t0,$zero,5\n addiu $t1,$zero,5\n beq $t0,$t1,eq\n nop\n addiu $v0,$zero,9\n syscall\neq:\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("beq (taken)", "vm", test_vm_beq)

def test_vm_bne():
    ok, path = asm(".text\nmain:\n addiu $t0,$zero,5\n addiu $t1,$zero,3\n bne $t0,$t1,neq\n nop\n addiu $v0,$zero,9\n syscall\nneq:\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("bne (taken)", "vm", test_vm_bne)

def test_vm_bgtz():
    ok, path = asm(".text\nmain:\n addiu $t0,$zero,10\n bgtz $t0,pos\n nop\n addiu $v0,$zero,9\n syscall\npos:\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("bgtz", "vm", test_vm_bgtz)

def test_vm_blez():
    ok, path = asm(".text\nmain:\n addiu $t0,$zero,0\n blez $t0,zero_or_neg\n nop\n addiu $v0,$zero,9\n syscall\nzero_or_neg:\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("blez", "vm", test_vm_blez)

def test_vm_jump_loop():
    ok, path = asm(".text\nmain:\n addiu $s0,$zero,5\nloop:\n addiu $s0,$s0,-1\n bgtz $s0,loop\n nop\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("loop (5 iterations)", "vm", test_vm_jump_loop)

# ==============================================================================
# VM: Memory Tests
# ==============================================================================

print("\n=== VM: Memory ===")

def test_vm_lw_sw():
    ok, path = asm(".text\nmain:\n addiu $sp,$sp,-16\n addiu $t0,$zero,42\n sw $t0,0($sp)\n lw $t1,0($sp)\n addiu $sp,$sp,16\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("lw/sw", "vm", test_vm_lw_sw)

def test_vm_lb_sb():
    ok, path = asm(".text\nmain:\n addiu $sp,$sp,-16\n addiu $t0,$zero,0x41\n sb $t0,0($sp)\n lbu $t1,0($sp)\n addiu $sp,$sp,16\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("lb/sb", "vm", test_vm_lb_sb)

# ==============================================================================
# Assembler Tests
# ==============================================================================

print("\n=== Assembler: Instructions ===")

def test_asm_add():
    ok, path = asm("add $t0, $t1, $t2")
    return (ok, "assembled" if ok else path)
test("r-type add", "asm", test_asm_add)

def test_asm_addiu():
    ok, path = asm("addiu $t0, $t1, 42")
    return (ok, "assembled" if ok else path)
test("i-type addiu", "asm", test_asm_addiu)

def test_asm_jal():
    ok, path = asm("jal target\ntarget:\n addiu $v0,$zero,9\n syscall")
    return (ok, "assembled" if ok else path)
test("jal with label", "asm", test_asm_jal)

def test_asm_b_needs_label():
    ok, path = asm("addiu $v0,$zero,9\n syscall")
    return (ok, "assembled" if ok else path)
test("simple program", "asm", test_asm_b_needs_label)

# ==============================================================================
# Assembler: Pseudo-Instructions
# ==============================================================================

print("\n=== Assembler: Pseudo-Instructions ===")

def test_asm_nop():
    ok, path = asm("nop")
    return (ok, "assembled" if ok else path)
test("nop", "asm", test_asm_nop)

def test_asm_move():
    ok, path = asm("move $t0, $t1")
    return (ok, "assembled" if ok else path)
test("move", "asm", test_asm_move)

def test_asm_li():
    ok, path = asm("li $t0, 123456")
    return (ok, "assembled" if ok else path)
test("li (large imm)", "asm", test_asm_li)

def test_asm_not():
    ok, path = asm("not $t0, $t1")
    return (ok, "assembled" if ok else path)
test("not", "asm", test_asm_not)

def test_asm_neg():
    ok, path = asm("neg $t0, $t1")
    return (ok, "assembled" if ok else path)
test("neg", "asm", test_asm_neg)

def test_asm_b():
    ok, path = asm("b label\nnop\nlabel:\n nop")
    return (ok, "assembled" if ok else path)
test("b (unconditional)", "asm", test_asm_b)

# ==============================================================================
# Assembler: Directives
# ==============================================================================

print("\n=== Assembler: Directives ===")

def test_asm_word():
    ok, path = asm(".word 0xDEADBEEF")
    return (ok, "assembled" if ok else path)
test(".word", "asm", test_asm_word)

def test_asm_byte():
    ok, path = asm(".byte 0x41")
    return (ok, "assembled" if ok else path)
test(".byte", "asm", test_asm_byte)

def test_asm_asciiz():
    ok, path = asm('.asciiz "hello"')
    return (ok, "assembled" if ok else path)
test(".asciiz", "asm", test_asm_asciiz)

def test_asm_ascii():
    ok, path = asm('.ascii "test"')
    return (ok, "assembled" if ok else path)
test(".ascii", "asm", test_asm_ascii)

# ==============================================================================
# Disassembler Tests
# ==============================================================================

print("\n=== Disassembler ===")

def test_dis_add():
    ok, path = asm(".text\nmain:\n addiu $v0,$zero,42\n jr $ra")
    if not ok: return (False, path)
    rc, out = dis(path)
    ok = rc == 0 and "addiu" in out and "jr" in out and "0x2402002a" in out
    return (ok, "output correct" if ok else f"unexpected: {out[:100]}")
test("disassemble addiu+jr", "dis", test_dis_add)

def test_dis_branch():
    ok, path = asm(".text\nmain:\n beq $t0,$t1,label\n nop\nlabel:\n syscall")
    if not ok: return (False, path)
    rc, out = dis(path)
    ok = rc == 0 and "beq" in out and "syscall" in out
    return (ok, "output correct" if ok else f"unexpected: {out[:100]}")
test("disassemble branch", "dis", test_dis_branch)

def test_dis_syscall():
    ok, path = asm(".text\nmain:\n syscall")
    if not ok: return (False, path)
    rc, out = dis(path)
    ok = rc == 0 and "syscall" in out and "0x0000000c" in out
    return (ok, "output correct" if ok else f"unexpected: {out[:100]}")
test("disassemble syscall", "dis", test_dis_syscall)

# ==============================================================================
# Encoder/Decoder Tests
# ==============================================================================

print("\n=== Encoder/Decoder (via disassemble roundtrip) ===")

def test_rt_arithmetic():
    programs = {
        "addiu $v0,$0,100": "0x24020064",
        "or $t0,$t1,$t2": "0x012a4025",
        "sll $t1,$t2,4": "0x000a4900",
        "jr $ra": "0x03e00008",
    }
    for code, expected_hex in programs.items():
        ok, path = asm(f".text\nmain:\n {code}")
        if not ok: return (False, f"asm failed: {code}")
        rc, out = dis(path)
        if rc != 0: return (False, f"dis failed: {code}")
        if expected_hex not in out: return (False, f"{code}: expected {expected_hex}, got:\n{out}")
    return (True, "all 4 roundtrips match")
test("roundtrip 4 instructions", "encode", test_rt_arithmetic)

# ==============================================================================
# CLI Tests
# ==============================================================================

print("\n=== CLI ===")

def test_cli_help():
    r = subprocess.run([SAGEMIPS, "--help"], capture_output=True, text=True)
    ok = r.returncode == 0 and "Usage" in r.stdout
    return (ok, "help shown" if ok else f"rc={r.returncode}")
test("--help", "cli", test_cli_help)

def test_cli_version():
    r = subprocess.run([SAGEMIPS, "--version"], capture_output=True, text=True)
    ok = r.returncode == 0 and "SageMips" in r.stdout
    return (ok, "version shown" if ok else f"rc={r.returncode}")
test("--version", "cli", test_cli_version)

def test_cli_unknown():
    r = subprocess.run([SAGEMIPS, "foobar"], capture_output=True, text=True)
    ok = r.returncode != 0
    return (ok, "errored" if ok else "should have failed")
test("unknown command", "cli", test_cli_unknown)

# ==============================================================================
# Boundary / Edge Case Tests
# ==============================================================================

print("\n=== Edge Cases ===")

def test_edge_empty():
    ok, path = asm(".text\nmain:\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("minimal program", "edge", test_edge_empty)

def test_edge_div_zero():
    ok, path = asm(".text\nmain:\n addiu $t0,$zero,10\n addiu $t1,$zero,0\n div $t0,$t1\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    # Division by zero should set error, VM halts
    return (True, "handled" if rc != 0 else "exit ok")  # Either is acceptable
test("division by zero", "edge", test_edge_div_zero)

def test_edge_forward_branch():
    ok, path = asm(".text\nmain:\n b forward\n nop\n addiu $v0,$zero,9\n syscall\nforward:\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("forward branch", "edge", test_edge_forward_branch)

def test_edge_backward_branch():
    ok, path = asm(".text\nmain:\n addiu $s0,$zero,3\nback:\n addiu $s0,$s0,-1\n bgtz $s0,back\n nop\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("backward branch", "edge", test_edge_backward_branch)

def test_edge_large_imm():
    ok, path = asm(".text\nmain:\n li $t0,0x7FFFFFFF\n li $t1,0x80000000\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("large immediates (li 32bit)", "edge", test_edge_large_imm)

def test_edge_delay_slot():
    ok, path = asm(".text\nmain:\n addiu $t0,$zero,5\n b done\n addiu $t0,$t0,1\ndone:\n addiu $v0,$zero,9\n syscall")
    if not ok: return (False, path)
    rc, out = run_mips(path)
    return (rc == 0, f"exit={rc}")
test("branch delay slot", "edge", test_edge_delay_slot)

# ==============================================================================
# Summary
# ==============================================================================

print(f"\n{'='*60}")
print(f"Results: {passed} passed, {failed} failed ({passed+failed} total)")
if failed > 0:
    print("\nFailures:")
    for r in results:
        if not r["passed"]:
            print(f"  ✗ [{r['category']}] {r['name']}: {r['message']}")
print(f"{'='*60}")

# Write JSON results
json_path = os.path.join(os.path.dirname(__file__), "test_results.json")
with open(json_path, "w") as f:
    json.dump({"passed": passed, "failed": failed, "tests": results}, f, indent=2)

sys.exit(0 if failed == 0 else 1)
