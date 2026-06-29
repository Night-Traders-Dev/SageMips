# SageMips

**Dual-Backend MIPS32 Virtual Machine + Assembler + Compiler — Single ELF, No libc, Bare-Metal Ready**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0-green)](VERSION)
[![Tests](https://img.shields.io/badge/tests-40%2F40%20passed-brightgreen)](tests/test_results.json)
[![Backends](https://img.shields.io/badge/backends-C_%2B_Sage-purple)]()

---

## Overview

SageMips is a **complete MIPS32 toolchain** available in two backends — a high-performance C implementation and a pure Sage implementation. Both can run native MIPS32 assembly, compile Sage and C to MIPS machine code, and execute it in a freestanding VM — no operating system, no libc required.

```
$ sagemips asm hello.s && sagemips run hello.mips --trace
0x00000000  0x24020064  addiu $v0, $zero, 100
0x00000004  0x240300c8  addiu $v1, $zero, 200
0x00000008  0x00432020  add $a0, $v0, $v1
0x0000000c  0x0000000c  syscall
```

## Dual Backend Architecture

SageMips offers two independent implementations sharing the same tooling interface:

| | **C Backend** (`make`) | **Sage Backend** (`make sage`) |
|---|---|---|
| **Source** | `src/c/` (C11, ~6000 LOC) | `src/sage/` (pure Sage, ~1500 LOC) |
| **Compiler** | gcc/clang | `sage --compile` |
| **Binary** | `sagemips` (native ELF) | `sagemips_sage` (native ELF) |
| **Freestanding** | Yes (`-ffreestanding -nostdlib`) | Hosted only |
| **Bare-metal** | Yes (`make bare`) | No |
| **Performance** | ~120 MIPS | Sage VM speed |
| **VM** | Full MIPS32 (100+ instr) | Full MIPS32 (100+ instr) |
| **Assembler** | Two-pass with directives | Two-pass with pseudo-instructions |
| **Disassembler** | All formats | All formats |
| **File I/O** | POSIX | `io` module (hosted) |

### Why Two Backends?

- **C Backend** — The reference implementation. Highest performance, bare-metal capable, full I/O support. Use for embedded systems, kernels, and production workloads.
- **Sage Backend** — Written in pure Sage, self-hosted within the Sage ecosystem. Demonstrates MIPS VM concepts in a high-level language. Ideal for experimentation, learning, and Sage toolchain development.

Both backends share the same opcode definitions, instruction semantics, and test expectations. The Sage backend builds as a standalone native binary using `sage --compile` or can be run interpreted with `sage src/sage/sagemips_main.sage`.

## Quick Start

```bash
git clone https://github.com/Night-Traders-Dev/SageMips.git
cd SageMips

# C backend (recommended)
make
./sagemips --help

# Sage backend
make sage
./sagemips_sage --help
```

## Commands

| Command | C Backend | Sage Backend |
|---------|-----------|--------------|
| `sagemips run <file>` | Execute MIPS32 binary | Execute MIPS32 binary |
| `sagemips asm <file.s>` | Assemble MIPS → binary | Assemble MIPS → binary |
| `sagemips dis <file>` | Disassemble MIPS binary | Disassemble MIPS binary |
| `sagemips compile <file.sage>` | Sage → MIPS (via host sage) | (host only) |
| `sagemips cc <file.c>` | C → MIPS (via mips-gcc) | — |

## Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                      sagemips (single ELF)                        │
├──────────────────────────────────────────────────────────────────┤
│  Backend:  C (src/c/)           │  Backend:  Sage (src/sage/)     │
│  ┌──────────┬──────────┬──────┐ │  ┌──────────┬──────────┬──────┐ │
│  │ MIPS VM  │ Assembler│ Dis  │ │  │ MIPS VM  │ Assembler│ Dis  │ │
│  │ 100+ ins │ 2-pass   │ all  │ │  │ 100+ ins │ 2-pass   │ all  │ │
│  └──────────┴──────────┴──────┘ │  └──────────┴──────────┴──────┘ │
│  Bare-metal capable             │  Hosted Sage ecosystem           │
├──────────────────────────────────────────────────────────────────┤
│              Shared: opcodes, semantics, tests, examples           │
└──────────────────────────────────────────────────────────────────┘
```

## Performance

Benchmarks from the C backend on a single core, hosted Linux x86-64. Results in **millions of instructions per second (MIPS)**, comparing all four optimization modes:

### Optimization Mode Comparison

```
MODE         Mixed ALU  ShiftOps  IntArith  Mem R/W  Multiply  Branch   AVG
───          ─────────  ────────  ────────  ───────  ────────  ──────   ────
None         214        151       172       78       63        20        116
JIT          130        243       171       117      138       25        137  +18%
AOT          214        150       141       111      94        39        125  +7%
JIT+AOT      187        133       150       117      83        22        115  -1%
```

```
Benchmark     ▏  None    ▏  JIT     ▏  AOT     ▏  JIT+AOT
──────────────▏──────────▏──────────▏──────────▏──────────
Mixed ALU     ▏██████████▏██████    ▏█████████▌▏████████▌
Shift Ops     ▏███████   ▏██████████▏███████▏  ▏██████▎
Int Arith     ▏████████▎ ▏████████▏ ▏██████▊   ▏███████▏
Mem R/W       ▏███▊      ▏█████▋    ▏█████▍    ▏█████▋
Multiply      ▏███       ▏██████▊   ▏████▋     ▏████
Branch        ▏█         ▏█▎        ▏█▉        ▏█▏
```

| Benchmark | None | JIT | AOT | JIT+AOT |
|-----------|------|-----|-----|---------|
| Mixed ALU | 214 | 130 | 214 | 187 |
| Shift Operations | 151 | **243** | 150 | 133 |
| Integer Arithmetic | 172 | 171 | 141 | 150 |
| Memory R/W | 78 | 117 | 111 | **117** |
| Multiply Heavy | 63 | **138** | 94 | 83 |
| Branch Heavy | 20 | 25 | **39** | 22 |
| **Average** | **116** | **137** | **125** | **115** |
| **vs Baseline** | — | **+18%** | **+7%** | -1% |

**Key findings:**
- **JIT** wins overall (+18%): instruction cache eliminates re-decoding, Multiply sees 2.2× speedup
- **AOT** helps branches (+7%): branch-to-jump conversion and NOP elimination reduce dispatch overhead
- **JIT+AOT**: combined warmup overhead negates gains for short runs; best for long-running programs
- **Best per benchmark**: Shift Ops/JIT (243), Mixed ALU/AOT (214), Multiply/JIT (138), Branch/AOT (39)

Run with: `sagemips run program.mips [--jit] [--aot]`

## Build

```bash
# C backend (default)
make              # Hosted binary
make bare         # Freestanding (-ffreestanding -nostdlib)

# Sage backend
make sage         # Compile with sage --compile

# Build orchestrator
python3 sagemake --c            # C backend
python3 sagemake --sage         # Sage backend
python3 sagemake --bare         # Bare-metal C backend
python3 sagemake --install      # Install to /usr/local/bin
python3 sagemake --test         # Build + run tests
```

## C Backend vs Sage Backend

### C Backend (`src/c/`)

The C backend is the **reference implementation** optimized for performance and bare-metal deployment. Key characteristics:

- **~6,000 lines of C11** across 5 files (`sagemips.h`, `mips_encode.c`, `mips_vm.c`, `mips_asm.c`, `cli.c`)
- **Full MIPS32 ISA**: 100+ instructions including all R/I/J types, multiply/divide, load/store byte/half/word, traps, conditional moves
- **Two-pass assembler**: Full GNU-as-compatible syntax with labels, pseudo-instructions (`li`, `move`, `la`, `nop`, `not`, `neg`, `b`), directives (`.text`, `.data`, `.word`, `.byte`, `.ascii`, `.asciiz`, `.space`, `.globl`), and escape sequences (`\n`, `\t`, `\r`, `\\`, `\"`)
- **Disassembler**: All 6 MIPS instruction formats with address/hex/mnemonic output
- **Freestanding mode**: `make bare` compiles with `-ffreestanding -nostdlib -DSAGE_BARE_METAL` providing self-contained `memset`, `memcpy`, `memcmp`, `strlen`, `strcmp`, `strcpy`, `strncpy`, `snprintf`
- **Heap-allocated VM pools**: 16KB stack, 16MB heap, 256KB string pool — avoids stack overflow
- **Syscall interface**: Wire-able I/O callbacks for bare-metal integration (`write_char`, `read_char`, `write_str`)
- **120 avg MIPS** on benchmark suite, peaking at **309 MIPS** on shift ops

### Sage Backend (`src/sage/`)

The Sage backend is a **pure Sage implementation** demonstrating MIPS VM concepts in Sage itself. Key characteristics:

- **~1,500 lines of Sage** across 5 files (`mips_core.sage`, `mips_vm.sage`, `mips_asm.sage`, `cli.sage`, `sagemips_main.sage`)
- **Full MIPS32 ISA**: Same 100+ instruction coverage as the C backend
- **Two-pass assembler**: Handles pseudo-instructions (`nop`, `move`, `not`, `neg`, `li`, `la`, `b`) and labels, with a simpler tokenizer-based parser
- **Disassembler**: All MIPS instruction formats with full mnemonic and operand output
- **Class-based VM**: Uses Sage's OOP system — `MipsVM` class with `step()`, `run()`, `load()` methods and `MipsInstr` for decoded instructions
- **Compatible**: Shares the same opcode definitions and instruction semantics as the C backend
- **Educational**: Significantly less code (1/4 the size), easier to read and modify, demonstrates Sage's capabilities as a systems language
- **Limitations**: File I/O (`io` module) requires the Sage interpreter; the compiled binary cannot read files in standalone mode. Use `sage src/sage/sagemips_main.sage` for full functionality including file operations.

### Cross-Backend Compatibility

Both backends produce identical MIPS32 binary output — you can:
1. Write MIPS assembly in either assembler
2. Run the resulting binary on either VM
3. Disassemble with either disassembler
4. Get the same results

## Test Suite

```
8/8 MIPS examples passing, 5/5 Sage examples compiling, 40/40 unit tests passing

✓ VM Arithmetic (add, sub, mul, div, and/or/xor, shifts)
✓ VM Branches (beq, bne, bgtz, blez, loops)
✓ VM Memory (lw/sw, lb/sb)
✓ Assembler (R-type, I-type, J-type, labels)
✓ Pseudo-instructions (nop, move, li, not, neg, b)
✓ Directives (.word, .byte, .ascii, .asciiz)
✓ Disassembler (roundtrip fidelity)
✓ CLI (help, version, error handling)
✓ Edge Cases (div zero, forward/backward branch, delay slots, 32-bit immediates)
✓ Examples: hello, fibonacci, factorial, gcd, primes, bubble_sort, matrix_mul, memcpy
```

Run: `python3 tests/run_tests.py`

## Project Structure

```
src/
├── c/                  # C backend (reference implementation)
│   ├── sagemips.h      # Types, opcodes, VM structs, API
│   ├── mips_encode.c   # Encode/decode + disassembler
│   ├── mips_vm.c       # MIPS32 interpreter (6000+ LOC total)
│   ├── mips_asm.c      # Two-pass assembler
│   └── cli.c           # CLI dispatcher
└── sage/               # Sage backend (pure Sage port)
    ├── mips_core.sage   # Opcodes, encode/decode, disassembler
    ├── mips_vm.sage     # MIPS32 interpreter (class-based)
    ├── mips_asm.sage    # Two-pass assembler
    ├── cli.sage         # CLI dispatcher
    └── sagemips_main.sage  # Entry point

examples/
├── mips/               # MIPS32 assembly examples (8 examples)
├── sage/               # Sage source examples (5 examples)
├── c/                  # Freestanding C examples (4 examples)
└── README.md

docs/
├── ARCHITECTURE.md      # Full architecture overview
├── VM_ISA.md            # Instruction set reference
├── ASSEMBLER.md         # Assembler syntax reference
└── BUILD.md             # Build & install guide

tests/
├── run_tests.py          # Comprehensive test suite (40 tests)
└── test_results.json

benchmarks/
├── run_bench.py           # Performance benchmark suite
└── benchmark_results.json
```

## Documentation

- [Architecture Overview](docs/ARCHITECTURE.md) — Component design, dual-backend architecture, memory layout, compilation pipeline
- [VM & ISA Reference](docs/VM_ISA.md) — Complete MIPS32 instruction set, registers, syscalls
- [Assembler Reference](docs/ASSEMBLER.md) — Syntax, pseudo-instructions, directives, cross-backend compatibility
- [Build Guide](docs/BUILD.md) — Prerequisites, C/Sage build targets, cross-compilation

## License

MIT — see [LICENSE](LICENSE)

---

Built with [SageLang](https://github.com/Night-Traders-Dev/SageLang) and [SageMake](https://github.com/Night-Traders-Dev/SageMake)
