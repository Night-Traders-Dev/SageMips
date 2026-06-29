# SageMips Build & Install Guide

## Quick Start

```bash
# Clone
git clone https://github.com/Night-Traders-Dev/SageMips.git
cd SageMips

# Build (hosted)
make

# Or use the build orchestrator
python3 sagemake

# Install
sudo make install
# or
python3 sagemake install
```

## Prerequisites

- **C Compiler**: gcc or clang (C11)
- **Python 3**: for the `sagemake` build orchestrator
- **Optional: Sage**: for Sage → MIPS compilation (`/usr/local/bin/sage`)
- **Optional: MIPS cross-compiler**: for C → MIPS compilation (`mips-linux-gnu-gcc`)

Install optional dependencies on Ubuntu/Debian:
```bash
sudo apt install gcc-mips-linux-gnu
```

## Build Targets

### make (hosted)
```bash
make              # Build sagemips binary (hosted Linux)
make test         # Run smoke tests
make install      # Install to /usr/local/bin
make clean        # Remove build artifacts
```

### make bare (freestanding)
```bash
make bare         # Build with -ffreestanding -nostdlib -DSAGE_BARE_METAL
```

### sagemake (Python orchestrator)
```bash
python3 sagemake            # Full build (checks deps, builds SageLang if needed, builds sagemips)
python3 sagemake bare       # Bare-metal build
python3 sagemake test       # Build + run tests
python3 sagemake install    # Install to /usr/local/bin
python3 sagemake clean      # Clean build
python3 sagemake help       # Show all commands
```

## Compiler Flags

### Hosted Build
```
CFLAGS  = -std=c11 -Wall -Wextra -O2
LDFLAGS = (default)
```

### Bare-Metal Build
```
CFLAGS  = -std=c11 -Wall -Wextra -O2 -ffreestanding -nostdlib -DSAGE_BARE_METAL -fno-stack-protector
LDFLAGS = -nostdlib -static
```

## Project Structure

```
SageMips/
├── src/                  # Source code
│   ├── sagemips.h        # Main header (types, opcodes, API)
│   ├── mips_vm.c         # MIPS32 interpreter
│   ├── mips_asm.c        # Two-pass assembler
│   ├── mips_encode.c     # Encode/decode + disassembler
│   └── cli.c             # CLI entry point
├── tests/                # Test suite
├── benchmarks/           # Performance benchmarks
├── docs/                 # Documentation
├── Makefile              # Build system
├── sagemake              # Python build orchestrator
└── README.md
```

## Installing from Source

```bash
git clone https://github.com/Night-Traders-Dev/SageMips.git
cd SageMips
make
sudo make install
```

This installs `sagemips` to `/usr/local/bin/sagemips`.

## Cross-Compilation

### For MIPS target (to run on actual MIPS hardware)

```bash
# Build the VM for MIPS
CC=mips-linux-gnu-gcc make
```

### For ARM bare-metal

```bash
CC=arm-none-eabi-gcc make bare
```

## Verification

```bash
# Check version
sagemips --version

# Quick smoke test
make test

# Run the benchmark suite
cd benchmarks && python3 run_bench.py
```
