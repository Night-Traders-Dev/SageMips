# SageMips Architecture

## Overview

SageMips is a **dual-backend MIPS32 Virtual Machine + Assembler + Compiler** delivered as a single ELF executable. It provides two independent implementations — a high-performance C backend and a pure Sage backend — sharing the same instruction set, assembler syntax, and binary format.

## Dual-Backend Design

```
┌─────────────────────────────────────────────────────────────────────┐
│                     SageMips Toolchain                              │
├──────────────────────────────┬──────────────────────────────────────┤
│       C Backend (src/c/)     │      Sage Backend (src/sage/)        │
│  ┌────────────────────────┐  │  ┌────────────────────────────────┐  │
│  │ sagemips.h — Types     │  │  │ mips_core.sage — Opcodes+Types │  │
│  │ mips_encode.c — Codec  │  │  │   Encode/Decode/Disassembler   │  │
│  │ mips_vm.c — Interpreter│  │  │ mips_vm.sage — Interpreter     │  │
│  │ mips_asm.c — Assembler │  │  │   (class-based MipsVM)         │  │
│  │ cli.c — CLI dispatcher │  │  │ mips_asm.sage — Assembler      │  │
│  └────────────────────────┘  │  │   (two-pass, tokenizer-based)   │  │
│  ~6,000 LOC C11              │  │ cli.sage — CLI dispatcher       │  │
│  gcc/clang compiled          │  │ sagemips_main.sage — Entry      │  │
│  -ffreestanding capable      │  └────────────────────────────────┘  │
│  Bare-metal ready            │  ~1,500 LOC Sage                      │
│                              │  sage --compile compiled              │
├──────────────────────────────┴──────────────────────────────────────┤
│                    Shared: opcodes, semantics, tests                  │
└─────────────────────────────────────────────────────────────────────┘
```

### Why Two Backends?

- **C Backend** — Reference implementation. Maximum performance (613 avg MIPS, 1710 peak), bare-metal capable, full POSIX I/O. Use for embedded systems, kernels, OS development.
- **Sage Backend** — Pure Sage implementation. Self-hosted within the Sage ecosystem, 1/4 the code size, easier to understand and modify. Demonstrates MIPS VM concepts in a high-level language. Ideal for experimentation and learning.

---

## C Backend (`src/c/`)

### File Structure

| File | Lines | Purpose |
|------|-------|---------|
| `sagemips.h` | ~270 | Types, MIPS32 opcodes, VM/assembler structs, API declarations |
| `mips_encode.c` | ~340 | Instruction encode/decode, register names, disassembler |
| `mips_vm.c` | ~730 | Full MIPS32 interpreter with syscall interface |
| `mips_asm.c` | ~550 | Two-pass assembler with directives and pseudo-instructions |
| `cli.c` | ~350 | CLI dispatcher, file I/O, syscall callbacks |

### Key Design Decisions

**Freestanding Compatibility**
```c
#ifdef SAGE_BARE_METAL
    // Self-contained libc replacements
    void* sm_memset(void* s, int c, unsigned long n);
    void* sm_memcpy(void* d, const void* s, unsigned long n);
    // ...
#else
    #include <string.h>
    #include <stdio.h>
    #include <stdlib.h>
#endif
```

**Heap-Allocated VM Pools** — The VM state (~16 MB) is heap-allocated to avoid stack overflow:
```c
MipsVM* mips_vm_create(void) {
    MipsVM* vm = malloc(sizeof(MipsVM));
    vm->stack = malloc(MIPS_STACK_SIZE * sizeof(uint32_t));
    vm->heap  = malloc(MIPS_HEAP_SIZE);
    vm->strings = malloc(MIPS_STRING_POOL);
    // ...
}
```

**Syscall Interface** — Wire-able I/O for bare-metal integration:
```c
vm->write_char = my_uart_putc;   // UART output
vm->read_char  = my_uart_getc;   // UART input
vm->write_str  = my_uart_puts;   // UART string output
```

**Two-Pass Assembler** — Handles forward references:
- Pass 0: Collects labels and validates syntax
- Pass 1: Emits machine code with resolved label addresses

---

## Sage Backend (`src/sage/`)

### File Structure

| File | Lines | Purpose |
|------|-------|---------|
| `mips_core.sage` | ~350 | MIPS32 opcodes, `MipsInstr` class, `encode_*` functions, `disasm()` |
| `mips_vm.sage` | ~390 | `MipsVM` class — full interpreter with `step()`/`run()`/`load()` |
| `mips_asm.sage` | ~430 | `MipsAsm` class — two-pass assembler with tokenizer |
| `cli.sage` | ~140 | CLI dispatcher with `dispatch()` entry point |
| `sagemips_main.sage` | ~15 | Entry point: `import cli; cli.dispatch()` |

### Key Design Decisions

**Class-Based VM**
```sage
class MipsVM:
    proc init(self):
        self.regs = [0] * 32       # 32 GP registers
        self.pc = 0
        self.stack = [0] * 4096    # 16 KB stack
        self.code = []

    proc step(self):
        let raw = mips_core.read_be32(self.code, self.pc)
        let instr = mips_core.MipsInstr(raw)
        # ... dispatch based on opcode ...

    proc run(self):
        self.running = true
        while self.running and not self.error:
            self.step()
```

**Token-Based Assembler**
```sage
proc tokenize(line):
    # Splits "addiu $v0, $zero, 42" into:
    # ["addiu", "$v0", ",", "$zero", ",", "42"]
    var tokens = []
    # ... character-by-character tokenizer ...
    return tokens
```

**Module Organization** — Each component is a separate Sage module imported via `import`:
```sage
import mips_core    # Opcodes, encode/decode, disassembler
import mips_vm      # VM interpreter
import mips_asm     # Assembler
import cli          # CLI dispatcher
```

### Sage Compiler Considerations

The Sage C backend (`sage --compile`) has several limitations that affect the Sage port:

1. **No short-circuit `and`**: The `and` operator evaluates BOTH operands even when the first is false. This requires guard checks to use `if/break` instead of combined `while` conditions:
   ```sage
   # BAD:  while i < len(s) and ord(s[i]) >= 48: ...
   # GOOD:
   while i < len(s):
       if ord(s[i]) < 48: break
       ...
   ```

2. **No nested procedures**: Functions defined inside other functions are not supported. All procedures must be at module level.

3. **No ternary operator**: `x if cond else y` must be written as `if/elif/else` blocks.

4. **No single-quoted strings**: All strings must use double quotes. Character comparisons must use `ord()`:
   ```sage
   # BAD:  if c == '#':
   # GOOD: if ord(c) == 35:
   ```

5. **`io` module limitation**: The `io.readfile()` function works in interpreted mode but may not work in standalone compiled binaries. Use the Sage interpreter (`sage file.sage`) for full I/O functionality.

---

## Cross-Backend Compatibility

Both backends produce identical MIPS32 binary output. They share:
- The same opcode encoding/decoding logic
- The same assembler syntax (labels, pseudo-instructions, directives)
- The same binary format (big-endian, 4-byte instruction words)
- The same test expectations

You can mix and match:
```bash
# Assemble with C backend, run with Sage backend
./sagemips asm examples/mips/hello.s
./sagemips_sage run hello.mips
```

---

## Instruction Dispatch (Both Backends)

Both backends use the same dispatch strategy — a single large `switch`/`elif` chain decoding the 6-bit opcode field:

```
opcode → SPECIAL (R-type)
        ├── ADD, SUB, AND, OR, XOR, NOR, SLT, SLTU
        ├── SLL, SRL, SRA, SLLV, SRLV, SRAV
        ├── JR, JALR, SYSCALL, BREAK
        ├── MFHI, MTHI, MFLO, MTLO
        ├── MULT, MULTU, DIV, DIVU
        └── MOVZ, MOVN, TGE..TNE
       → SPECIAL2 → MUL
       → REGIMM → BLTZ, BGEZ, BLTZAL, BGEZAL
       → J, JAL
       → BEQ, BNE, BLEZ, BGTZ
       → ADDI, ADDIU, SLTI, SLTIU, ANDI, ORI, XORI, LUI
       → LW, SW, LB, LBU, LH, LHU, SB, SH
```

### Branch Delay Slots

Both VMs correctly emulate MIPS branch delay slots — the instruction immediately following a branch is always executed before the branch takes effect:

```asm
    beq   $t0, $t1, target
    nop              # ← this always executes (delay slot)
```

---

## Binary Format

Both backends emit the same flat binary format:

```
┌──────────────────┐  0x0000
│  .text section    │  MIPS32 instructions (big-endian, 4-byte aligned)
│  (code)           │
├──────────────────┤  N bytes
│  .data section    │  Strings, word data, byte data
│  (data)           │
└──────────────────┘  N + M bytes
```

No ELF headers — just raw MIPS32 machine code. The assembler places `.text` first (at offset 0) followed by `.data`. Labels in the data section are resolved relative to the total code size.

## Syscall Reference

Both VMs share the same syscall interface:

| Number | Name | $a0 | $v0 (return) |
|--------|------|-----|-------------|
| 0 | exit | — | — |
| 1 | print_int | value | — |
| 2 | print_str | address | — |
| 3 | read_int | — | value |
| 4 | read_str | buffer | — |
| 5 | sbrk | bytes | address |
| 6 | print_char | char | — |
| 7 | read_char | — | char |
| 8 | time | — | seconds |
| 9 | halt | — | — |

---

## Build System

### Makefile

```
make          → C backend (hosted, gcc/clang)
make bare     → C backend (freestanding, -nostdlib)
make sage     → Sage backend (sage --compile)
make test     → Quick smoke tests
make install  → Install C backend to /usr/local/bin
```

### sagemake (Python orchestrator)

```
python3 sagemake --c       → C backend
python3 sagemake --sage    → Sage backend
python3 sagemake --bare    → C bare-metal
python3 sagemake --native  → Emit MIPS asm from Sage source
python3 sagemake --install → Install
python3 sagemake --test    → Build + test
```
