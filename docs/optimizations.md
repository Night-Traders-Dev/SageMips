# SageMips Optimizations

## Overview

SageMips provides two complementary optimization strategies that can be used independently or together:

| Strategy | Stage | What it does | Enable with |
|----------|-------|-------------|-------------|
| **AOT** (Ahead-of-Time) | Before execution | Static peephole optimizations on the binary | `--aot` |
| **JIT** (Just-in-Time) | During execution | Instruction cache + basic block profiling | `--jit` |

Both strategies are implemented identically in the C backend (`src/c/`) and Sage backend (`src/sage/`).

## Optimization Pipeline

```
                    ┌──────────┐
    .mips binary ──→│   AOT    │──→ optimized binary
                    │ (static) │
                    └──────────┘
                          │
                          ▼
                    ┌──────────┐
                    │    VM    │←── JIT cache (dynamic)
                    │ (runtime)│    basic block profiling
                    └──────────┘
```

### When to Use Which

| Scenario | Recommendation |
|----------|---------------|
| One-shot execution | `--aot` (reduce code size, NOPs) |
| Loop-heavy code | `--jit` (cache loop body instructions) |
| Repeated execution | `--aot --jit` (both) |
| Compiler-generated code | `--aot` (many NOPs, constant sequences) |
| Hand-written assembly | `--jit` (fewer static optimization opportunities) |
| Benchmarking | Test all four modes: none, AOT, JIT, AOT+JIT |

## AOT Optimizations

Four-pass static optimizer. See [docs/aot.md](aot.md) for full details.

| Pass | Description | Impact |
|------|-------------|--------|
| 1. NOP Elimination | Remove redundant `sll $0,$0,0` | Code size |
| 2. Branch-to-Jump | Convert `beq $0,$0,target` to `j target` | Dispatch speed |
| 3. Constant Folding | Merge `addiu+addiu` and `addiu+ori` pairs | Instruction count |
| 4. Dead Code | Remove code after terminators | Code size |

```
Before AOT:  20 instructions (84 bytes)
After AOT:   14 instructions (56 bytes) — 30% reduction
```

## JIT Optimizations

Three-phase runtime optimizer. See [docs/jit.md](jit.md) for full details.

| Phase | Description | Impact |
|-------|-------------|--------|
| 1. Warmup | Pre-decode all instructions, build BB table | One-time cost |
| 2. Cache | Reuse decoded instructions | Per-instruction speed |
| 3. Profiling | Track BB execution counts | Future native code |

```
Without JIT:  decode 1,000,000 instructions (1M decodes)
With JIT:     decode 100 unique instructions (100 decodes + cache lookups)
```

## Cross-Backend Consistency

Both the C and Sage backends implement the same optimization logic:

| Optimization | C Backend | Sage Backend |
|-------------|-----------|--------------|
| NOP elimination | `pass_nop_elim()` in `mips_aot.c` | `_pass_nop_elim()` in `mips_aot.sage` |
| Branch-to-jump | `pass_branch_opt()` in `mips_aot.c` | `_pass_branch_opt()` in `mips_aot.sage` |
| Constant folding | `pass_const_fold()` in `mips_aot.c` | `_pass_const_fold()` in `mips_aot.sage` |
| Dead code | `pass_dead_code()` in `mips_aot.c` | (not yet in Sage — coming soon) |
| Inst cache | `mips_jit_cache_instr()` in `mips_jit.c` | `MipsJIT.warmup()` in `mips_jit.sage` |
| BB profiling | `mips_jit_warmup()` in `mips_jit.c` | `MipsJIT.warmup()` in `mips_jit.sage` |

## Benchmarking Optimizations

```bash
# Baseline (no optimizations)
sagemips run program.mips

# AOT only
sagemips run program.mips --aot

# JIT only
sagemips run program.mips --jit

# Both
sagemips run program.mips --aot --jit
```

Compare execution time with `time`:
```bash
time sagemips run program.mips           # baseline
time sagemips run program.mips --aot     # AOT overhead + optimized code
time sagemips run program.mips --jit     # JIT warmup + cached execution
time sagemips run program.mips --aot --jit  # both
```

## Future Optimizations

Planned but not yet implemented:

- **Native code generation**: Compile hot basic blocks to x86-64 machine code and execute directly (true JIT)
- **Adaptive hot threshold**: Dynamically adjust the hot-path threshold based on execution profile
- **Register allocation**: Map frequently-used MIPS registers to x86-64 registers in native code
- **Indirect jump prediction**: Predict targets of `JR` instructions based on call history
- **Loop unrolling**: Duplicate loop bodies to reduce branch overhead
- **Function inlining**: Inline small leaf functions at the MIPS level (AOT pass)

## Building with Optimizations

Optimizations are always compiled in — they're enabled at runtime via flags:
```bash
make              # Standard build (--jit and --aot available)
make JIT=1 AOT=1  # Explicit build with optimization modules
```
