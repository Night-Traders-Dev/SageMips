# SageMips Architecture

## Overview

SageMips is a **freestanding MIPS32 Virtual Machine + Assembler + Compiler** delivered as a single ELF executable. It runs on bare-metal (no OS, no libc) or on hosted Linux. It can execute native MIPS32 assembly, compile Sage to MIPS, and compile C to MIPS using a cross-compiler.

## Component Architecture

```
┌─────────────────────────────────────────────────────┐
│                   SageMips CLI                       │
│  (cli.c) — Command dispatcher                        │
│  run | asm | dis | compile | cc                      │
├──────────┬──────────┬──────────┬─────────────────────┤
│ MIPS VM  │ MIPS     │ MIPS     │ Sage/C Compiler     │
│ (mips_   │ Assembler│ Disasm   │ (external: sage +   │
│  vm.c)   │ (mips_   │ (mips_   │  gcc-mips)          │
│          │  asm.c)  │ encode.c)│                     │
├──────────┴──────────┴──────────┴─────────────────────┤
│              sagemips.h — Shared types & opcodes      │
├──────────────────────────────────────────────────────┤
│  Build: Makefile (hosted + bare-metal)                │
│  Orchestrator: sagemake (Python)                      │
└──────────────────────────────────────────────────────┘
```

## MIPS VM (`src/mips_vm.c`)

The VM is a full MIPS32 big-endian interpreter with:
- **32 General-Purpose Registers** ($zero, $at, $v0–$v1, $a0–$a3, $t0–$t9, $s0–$s7, $k0–$k1, $gp, $sp, $fp, $ra)
- **HI/LO Special Registers** for multiply/divide results
- **Memory-Mapped Stack** — 4096 × 32-bit words (16 KB), grows downward from top
- **Heap** — Simple bump allocator via sbrk syscall (16 MB pool)
- **String Pool** — Interning string table (256 KB)
- **Call Stack** — 256-entry deep, stores return address, saved $ra, saved $fp
- **Syscall Interface** — exit, print_int, print_str, read_int, read_str, sbrk, print_char, read_char, time, halt

### Instruction Support (100+ instructions)

| Category | Instructions |
|----------|-------------|
| **Arithmetic** | ADD, ADDU, SUB, SUBU, ADDI, ADDIU, MUL |
| **Logical** | AND, OR, XOR, NOR, ANDI, ORI, XORI |
| **Set** | SLT, SLTU, SLTI, SLTIU |
| **Shift** | SLL, SRL, SRA, SLLV, SRLV, SRAV |
| **Multiply/Divide** | MULT, MULTU, DIV, DIVU, MFHI, MFLO, MTHI, MTLO |
| **Branch** | BEQ, BNE, BLEZ, BGTZ, BLTZ, BGEZ, BLTZAL, BGEZAL |
| **Jump** | J, JAL, JR, JALR |
| **Load/Store** | LW, SW, LB, LBU, LH, LHU, SB, SH |
| **Move** | MOVZ, MOVN (conditional moves) |
| **Trap** | TGE, TGEU, TLT, TLTU, TEQ, TNE |
| **Special** | LUI, SYSCALL, BREAK, NOP |

### Design Properties

- **Freestanding**: $DB \quad$ When `SAGE_BARE_METAL` is defined, uses no libc — all memset/memcpy/strlen/snprintf are self-contained
- **Heap-allocated**: On hosted platforms, VM pools are dynamically allocated to avoid stack overflow (VM is ~16 MB)
- **Big-endian**: Instructions are fetched in MIPS I big-endian byte order
- **Branch delay slots**: Correctly emulated — the instruction after a branch always executes

---

## MIPS Assembler (`src/mips_asm.c`)

Two-pass assembler that converts MIPS32 assembly text to binary machine code.

### Pass 1 (Label Collection)
- Scans source, collects all label definitions
- Validates instruction syntax
- Tracks code size for address calculation

### Pass 2 (Code Emission)
- Emits big-endian 4-byte instructions
- Resolves forward and backward label references
- Supports pseudo-instruction expansion

### Supported Syntax

```
# Pseudo-instructions
li    $reg, imm      →  lui + ori sequence
move  $rd, $rs       →  addu $rd, $rs, $zero
not   $rd, $rs       →  nor $rd, $rs, $zero
neg   $rd, $rs       →  sub $rd, $zero, $rs
nop                   →  sll $zero, $zero, 0
b     label           →  beq $zero, $zero, label
la    $reg, label     →  lui + ori with label address

# Directives
.text                # Code section
.data                # Data section
.word   value        # 32-bit data
.byte   value        # 8-bit data
.ascii  "string"     # ASCII string
.asciiz "string"     # Null-terminated ASCII string
.space  n            # Zero-filled space
.align  n            # Alignment hint
.globl  symbol       # Global symbol
```

### Addressing Modes
- **Register direct**: `add $t0, $t1, $t2`
- **Immediate**: `addiu $t0, $t1, 42`
- **Base+offset**: `lw $t0, 8($sp)`
- **PC-relative**: `beq $t0, $t1, label`
- **Absolute**: `j label`, `jal label`

---

## MIPS Disassembler (`src/mips_encode.c`)

Converts MIPS32 big-endian machine code back to human-readable assembly. Output format:
```
0x00000000  0x24020064  addiu $v0, $zero, 100
0x00000004  0x00432020  add $a0, $v0, $v1
```

- Shows address, raw hex, and decoded mnemonic with operands
- Correctly handles all 6 instruction formats (R, I, J, special R, REGIMM, SPECIAL2)
- Branch targets shown as absolute addresses
- Immediate values shown in decimal

---

## CLI (`src/cli.c`)

Single-binary command dispatcher:

| Command | Description |
|---------|-------------|
| `sagemips run <file> [--trace]` | Execute MIPS32 binary |
| `sagemips asm <file.s> [out]` | Assemble MIPS assembly → binary |
| `sagemips dis <file>` | Disassemble MIPS binary |
| `sagemips compile <file.sage>` | Compile Sage → MIPS → run |
| `sagemips cc <file.c>` | Compile C → MIPS (needs mips-gcc) |
| `sagemips --help` | Show usage |
| `sagemips --version` | Show version |

---

## Sage/C Compilation Pipeline

### Sage → MIPS
```
.sage  →  sage --emit-asm --target mips  →  .s  →  sagemips asm  →  .mips  →  sagemips run
```

### C → MIPS
```
.c  →  mips-linux-gnu-gcc -nostdlib -march=mips32  →  .mips  →  sagemips run
```

---

## Build System

### Makefile Targets

| Target | Description |
|--------|-------------|
| `make` | Build hosted SageMips |
| `make bare` | Build freestanding SageMips (`-ffreestanding -nostdlib`) |
| `make install` | Install to `/usr/local/bin/sagemips` |
| `make test` | Run quick smoke tests |
| `make clean` | Remove build artifacts |

### sagemake (Python Build Orchestrator)

```
python3 sagemake          # Full build pipeline
python3 sagemake bare     # Bare-metal build
python3 sagemake install  # Install system-wide
python3 sagemake test     # Build + run tests
python3 sagemake clean    # Clean build
```

---

## Memory Layout (Bare-Metal)

When built with `-DSAGE_BARE_METAL`, the VM uses static memory pools:

```
┌──────────────────────────────┐  0x00000000
│       Code (up to 2 MB)       │
├──────────────────────────────┤
│    Stack (4096 × 32-bit)     │  ← $sp starts at top
├──────────────────────────────┤
│    Heap (16 MB bump alloc)   │
├──────────────────────────────┤
│    String Pool (256 KB)      │
├──────────────────────────────┤
│    VM State (struct)          │  ← Register file, call stack, etc.
└──────────────────────────────┘
```

## Syscall Reference

| Number | Name | Args | Returns |
|--------|------|------|---------|
| 0 | exit | — | — |
| 1 | print_int | $a0 = value | — |
| 2 | print_str | $a0 = address | — |
| 3 | read_int | — | $v0 = value |
| 4 | read_str | $a0 = buffer | — |
| 5 | sbrk | $a0 = bytes | $v0 = address |
| 6 | print_char | $a0 = char | — |
| 7 | read_char | — | $v0 = char |
| 8 | time | — | $v0 = seconds |
| 9 | halt | — | — |
