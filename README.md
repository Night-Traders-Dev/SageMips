# SageMips

**Freestanding MIPS32 Virtual Machine + Assembler + Compiler — Single ELF, No libc, Bare-Metal Ready**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0-green)](VERSION)
[![Tests](https://img.shields.io/badge/tests-40%2F40%20passed-brightgreen)](tests/test_results.json)

---

## Overview

SageMips is a **complete MIPS32 toolchain in a single ELF binary**. It can run native MIPS32 assembly, compile Sage and C to MIPS machine code, and execute it all in a freestanding VM — no operating system, no libc required. Built for bare-metal, OS development, and embedded systems.

```
$ sagemips asm hello.s && sagemips run hello.mips --trace
0x00000000  0x24020064  addiu $v0, $zero, 100
0x00000004  0x240300c8  addiu $v1, $zero, 200
0x00000008  0x00432020  add $a0, $v0, $v1
0x0000000c  0x0000000c  syscall
```

## Quick Start

```bash
git clone https://github.com/Night-Traders-Dev/SageMips.git
cd SageMips
make
./sagemips --help
```

## Commands

| Command | Description |
|---------|-------------|
| `sagemips run <file>` | Execute a MIPS32 binary |
| `sagemips asm <file.s>` | Assemble MIPS assembly → binary |
| `sagemips dis <file>` | Disassemble MIPS binary |
| `sagemips compile <file.sage>` | Compile Sage → MIPS |
| `sagemips cc <file.c>` | Compile C → MIPS (needs mips-gcc) |

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    sagemips (single ELF)                   │
├────────────┬────────────┬────────────┬────────────────────┤
│  MIPS32 VM │  Assembler │ Disasm     │ Sage/C Compiler     │
│  100+ instr│  2-pass    │ all formats│ (external tools)    │
├────────────┴────────────┴────────────┴────────────────────┤
│              Freestanding C11 — no libc required           │
└──────────────────────────────────────────────────────────┘
```

### Supported MIPS32 Instructions (100+)

**Arithmetic:** `ADD, ADDU, SUB, SUBU, ADDI, ADDIU, MUL`  
**Logical:** `AND, OR, XOR, NOR, ANDI, ORI, XORI`  
**Set:** `SLT, SLTU, SLTI, SLTIU`  
**Shift:** `SLL, SRL, SRA, SLLV, SRLV, SRAV`  
**Mul/Div:** `MULT, MULTU, DIV, DIVU, MFHI, MFLO, MTHI, MTLO`  
**Branch:** `BEQ, BNE, BLEZ, BGTZ, BLTZ, BGEZ, BLTZAL, BGEZAL`  
**Jump:** `J, JAL, JR, JALR`  
**Memory:** `LW, SW, LB, LBU, LH, LHU, SB, SH`  
**Move:** `MOVZ, MOVN` · **Trap:** `TGE, TGEU, TLT, TLTU, TEQ, TNE`  
**Special:** `LUI, SYSCALL, BREAK`

## Performance

Benchmarks run on a single core, hosted Linux x86-64. Results in **millions of instructions per second (MIPS)**:

```
MIXED ALU  ██████████████████████████████████████████████████▎ 1710
SHIFT OPS  █████████████████████████████████████████▌          1375
MEM R/W    ████████████████████████████████████▊               1051
INT ARITH  ████████████████████████████████████▌                920
MULTIPLY   ████████████████████▊                                556
NESTED LP  ████████▉                                            244
BRANCHES   █████▌                                               149
DIVISION   ███                                                  77
FIBONACCI  █▌                                                   42
PRIME SIEVE ▌                                                    1
            └────┬────┴────┬────┴────┬────┴────┬────┴────┬────┘
            0   400   800  1200  1600  2000
                        Million Instructions/sec
```

| Benchmark | Time | MIPS | Instructions |
|-----------|------|------|-------------|
| Mixed ALU (10 ops × 100K) | 0.9ms | **1,710** | 1,500,005 |
| Shift Operations (6 × 100K) | 0.9ms | **1,375** | 1,200,005 |
| Memory Read/Write (8 × 50K) | 0.7ms | **1,051** | 700,005 |
| Integer Arithmetic (7 × 100K) | 1.3ms | **920** | 1,200,005 |
| Multiply (5 × 50K) | 0.9ms | **556** | 500,005 |
| Nested Loops (200×200) | 3.3ms | **244** | 800,005 |
| Branch (3 × 50K) | 1.0ms | **149** | 150,005 |
| Division (2 × 20K) | 3.1ms | **77** | 240,005 |
| Fibonacci (5000 terms) | 0.9ms | **42** | 40,005 |
| Prime Counting (n ≤ 500) | 1.0ms | **1.5** | 1,505 |

**Aggregate:** 6.3M instructions in 14ms = **613 average MIPS**

### Instruction Latency

```
addu  ▏ 0.6 ns/instr
sll   ▏ 0.7 ns/instr
add   ▏ 1.0 ns/instr
and   ▏ 1.0 ns/instr
or    ▏ 1.0 ns/instr
xor   ▏ 1.0 ns/instr
mul   ▏ 1.8 ns/instr
lw    ▏ 0.9 ns/op
sw    ▏ 0.9 ns/op
beq   ▏ 5.0 ns (taken)
div   ▏ 12.9 ns/instr
```

## Build

```bash
make              # Hosted Linux binary (default)
make bare         # Freestanding (-ffreestanding -nostdlib)
make test         # Run test suite
make install      # Install to /usr/local/bin
```

Or use the Python build orchestrator:
```bash
python3 sagemake            # Full build pipeline
python3 sagemake bare       # Bare-metal build
python3 sagemake install    # Install system-wide
```

## Test Suite

```
40/40 tests passed

✓ VM Arithmetic (add, sub, mul, div, and/or/xor, shifts)
✓ VM Branches (beq, bne, bgtz, blez, loops)
✓ VM Memory (lw/sw, lb/sb)
✓ Assembler (R-type, I-type, J-type, labels)
✓ Pseudo-instructions (nop, move, li, not, neg, b)
✓ Directives (.word, .byte, .ascii, .asciiz)
✓ Disassembler (roundtrip fidelity)
✓ CLI (help, version, error handling)
✓ Edge Cases (div zero, forward/backward branch, delay slots, 32-bit immediates)
```

Run: `python3 tests/run_tests.py`

## Sage + C Compilation

```bash
# Sage → MIPS
sagemips compile program.sage   # Uses sage --emit-asm --target mips + assembler

# C → MIPS (requires mips-linux-gnu-gcc)
sagemips cc program.c
```

## Bare-Metal / Freestanding

Build without libc for kernel/embedded use:
```bash
make bare   # -ffreestanding -nostdlib -DSAGE_BARE_METAL
```

The `_start()` entry point provides a complete freestanding runtime:
- Custom `memset`, `memcpy`, `memcmp`, `strlen`, `strcmp`, `strcpy`, `strncpy`, `snprintf`
- Fixed-size static memory pools (stack 16KB, heap 16MB, strings 256KB)
- Syscall interface via function callbacks for I/O

## Project Structure

```
src/
├── sagemips.h      # Types, opcodes, VM structs, API
├── mips_vm.c       # MIPS32 interpreter (100+ instructions)
├── mips_asm.c      # Two-pass assembler
├── mips_encode.c   # Encode/decode + disassembler
└── cli.c           # CLI dispatcher

docs/
├── ARCHITECTURE.md  # Full architecture overview
├── VM_ISA.md        # Instruction set reference
├── ASSEMBLER.md     # Assembler syntax reference
└── BUILD.md         # Build & install guide

tests/
├── run_tests.py     # Comprehensive test suite (40 tests)
└── test_results.json

benchmarks/
├── run_bench.py      # Performance benchmark suite
└── benchmark_results.json
```

## Documentation

- [Architecture Overview](docs/ARCHITECTURE.md) — Component design, memory layout, compilation pipeline
- [VM & ISA Reference](docs/VM_ISA.md) — Complete MIPS32 instruction set, registers, syscalls
- [Assembler Reference](docs/ASSEMBLER.md) — Syntax, pseudo-instructions, directives
- [Build Guide](docs/BUILD.md) — Prerequisites, targets, cross-compilation

## License

MIT — see [LICENSE](LICENSE)

---

Built with [SageLang](https://github.com/Night-Traders-Dev/SageLang) and [SageMake](https://github.com/Night-Traders-Dev/SageMake)
