# SageMips Build & Install Guide

## Quick Start

```bash
git clone https://github.com/Night-Traders-Dev/SageMips.git
cd SageMips

# C backend (recommended, full functionality)
make
./sagemips --help

# Sage backend (pure Sage implementation)
make sage
./sagemips_sage --help
```

## Dual Backend Build Targets

SageMips provides two build backends — choose based on your needs:

| Target | Command | Output | Dependencies |
|--------|---------|--------|-------------|
| C hosted | `make` | `sagemips` | gcc/clang |
| C bare-metal | `make bare` | `sagemips` | gcc/clang |
| Sage compiled | `make sage` | `sagemips_sage` | sage + gcc |

## Prerequisites

### C Backend
- **C Compiler**: gcc or clang (C11)
- **Python 3**: for the `sagemake` build orchestrator

### Sage Backend
- **SageLang**: `/usr/local/bin/sage` (or in `deps/SageLang/core/sage`)
- **C Compiler**: gcc (sage --compile generates C code)

### Optional
- **MIPS cross-compiler**: `mips-linux-gnu-gcc` for C → MIPS (`sagemips cc`)
- **QEMU**: for running MIPS binaries without real hardware

Install on Ubuntu/Debian:
```bash
sudo apt install gcc-mips-linux-gnu qemu-user
```

## C Backend Build

### Hosted Build

```bash
make
# Compiles with: gcc -std=c11 -Wall -Wextra -O2 -Isrc/c src/c/*.c -o sagemips
```

### Bare-Metal Build

```bash
make bare
# Compiles with: -ffreestanding -nostdlib -DSAGE_BARE_METAL -fno-stack-protector
```

The bare-metal build is a freestanding ELF with no libc dependency. All string/memory functions (`memset`, `memcpy`, `strlen`, `strcmp`, `snprintf`) are self-contained. The `_start` entry point replaces `main`.

### C Compiler Flags

```
CFLAGS  = -std=c11 -Wall -Wextra -O2 -Isrc/c
LDFLAGS = (default, links with libc)

# Bare-metal
CFLAGS  += -ffreestanding -nostdlib -DSAGE_BARE_METAL -fno-stack-protector
LDFLAGS += -nostdlib -static
```

## Sage Backend Build

### Compiled Binary

```bash
make sage
# Equivalent to:
# SAGE_PATH=src/sage sage --compile src/sage/sagemips_main.sage -o sagemips_sage
```

The Sage compiler (`sage --compile`) translates Sage source to C, then compiles with `cc -O2`. The resulting `sagemips_sage` is a native ELF binary.

### Interpreted Mode

```bash
# Run directly with the Sage interpreter (full I/O support)
SAGE_PATH=src/sage sage src/sage/sagemips_main.sage asm examples/mips/hello.s
```

**Note**: The compiled Sage binary may have limited file I/O support due to how the Sage C backend handles the `io` module. For full file operations, use the C backend or run in interpreted mode.

### Emit MIPS Assembly from Sage Source

```bash
# Cross-compile: Sage source → MIPS assembly
make sage-native
# or
SAGE_PATH=src/sage sage --emit-asm src/sage/sagemips_main.sage --target mips -o build/sagemips_sage.asm
```

This generates MIPS32 assembly that can be assembled and run on the SageMips VM:
```bash
./sagemips asm build/sagemips_sage.asm
./sagemips run build/sagemips_sage.mips
```

## sagemake Build Orchestrator

The Python `sagemake` script provides a unified interface:

```bash
python3 sagemake --c              # C backend (default)
python3 sagemake --sage           # Sage backend
python3 sagemake --bare           # C bare-metal backend
python3 sagemake --native         # Emit MIPS assembly from Sage
python3 sagemake --install        # Install to /usr/local/bin
python3 sagemake --test           # Build + run tests
python3 sagemake --clean          # Clean all artifacts
```

## Project Structure

```
src/
├── c/                      # C backend
│   ├── sagemips.h          # Shared header
│   ├── mips_encode.c       # Encode/decode + disassembler
│   ├── mips_vm.c           # MIPS32 interpreter
│   ├── mips_asm.c          # Two-pass assembler
│   └── cli.c               # CLI dispatcher
└── sage/                   # Sage backend
    ├── mips_core.sage       # Opcodes, encode/decode, disasm
    ├── mips_vm.sage         # MIPS32 interpreter (class-based)
    ├── mips_asm.sage        # Two-pass assembler
    ├── cli.sage             # CLI dispatcher
    └── sagemips_main.sage   # Entry point

tests/                      # Test suite (40 tests)
benchmarks/                 # Performance benchmarks
examples/                   # Example programs (mips/sage/c)
docs/                       # Documentation
```

## Installation

```bash
# Via make
sudo make install

# Via sagemake
python3 sagemake --c --install

# Manual
sudo cp sagemips /usr/local/bin/
sudo chmod 755 /usr/local/bin/sagemips
```

## Cross-Compilation for Real MIPS Hardware

```bash
# Build the C VM for MIPS target
CC=mips-linux-gnu-gcc make

# Or create a MIPS binary that runs on the VM
./sagemips cc examples/c/hello.c  # needs mips-gcc
```

## Verification

```bash
# C backend
./sagemips --version
make test

# Sage backend
./sagemips_sage --version

# Full test suite
python3 tests/run_tests.py

# Performance benchmarks
python3 benchmarks/run_bench.py
```

