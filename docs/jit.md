# SageMips JIT (Just-In-Time) Optimizer

## Overview

The JIT optimizer accelerates MIPS32 interpretation by caching decoded instructions and tracking basic block execution. When enabled via `--jit`, the VM records which instructions are executed, pre-decodes them once, and chains basic blocks for faster dispatch.

Both the C backend (`src/c/mips_jit.c`) and Sage backend (`src/sage/mips_jit.sage`) implement identical optimization strategies.

## How It Works

### 1. Decoded Instruction Cache

Without JIT, the VM re-decodes every MIPS32 instruction on every execution:
```
Fetch 4 bytes → Extract opcode/rs/rt/rd/funct/imm → Dispatch
```

With JIT, decoding happens once per unique PC address:
```
First visit: Fetch → Decode → Store in cache → Dispatch
Subsequent:  Lookup cache → Dispatch (skip decode)
```

The cache uses a 65536-entry hash table keyed by `(pc >> 2)`. Cache hits eliminate the 6-field bit-extraction for every instruction execution.

### 2. Basic Block Detection

The JIT scans the binary to identify **basic blocks** — sequences of instructions ending with a branch, jump, or syscall. Each block is bounded by:
- **Entry**: Start of program, or a branch target
- **Exit**: Any control flow instruction (branch, jump, JR, JALR, syscall)

Block structure:
```
[addiu $t0,$0,10]  ─┐
[addiu $t1,$0,20]   │ BB #0 (fall-through)
[add  $t2,$t0,$t1] ─┘
[beq  $t2,$0,done] ─── BB #1 (branch)
[nop]              ─── delay slot
[syscall]          ─── BB #2 (terminal)
```

### 3. Hot Path Profiling

Each basic block tracks an execution count (`hit_count`). After a configurable threshold (default: 50 executions), a block is considered "hot". Hot blocks can be:
- Chained directly to their most common successor
- Marked for potential native code generation (future)

### 4. Direct Block Chaining

After profiling identifies the most common path through a branch, the JIT pre-computes:
- `next_bb`: The fall-through basic block address
- `branch_bb`: The branch target basic block address

This enables the VM to jump directly to the known next block without re-evaluating branch conditions for predictable paths.

## Usage

```bash
# Run with JIT enabled
sagemips run program.mips --jit

# Run with JIT + trace
sagemips run program.mips --jit --trace

# JIT + AOT combined
sagemips run program.mips --jit --aot
```

JIT statistics are printed after execution:
```
JIT: 42 cached instructions, 8 basic blocks
JIT stats: 42 cache entries, 8 BBs
```

## C Backend Implementation

**File:** `src/c/mips_jit.c`

- `mips_jit_init()` — Initializes hash table and BB array
- `mips_jit_cache_instr()` — Caches a decoded instruction at a PC
- `mips_jit_warmup()` — Pre-scans code to populate cache and build BBs
- `mips_jit_hit_bb()` — Records that a basic block was executed
- `mips_jit_stats()` — Returns cache size and BB count

Key data structures:
```c
typedef struct {
    MipsInstr decoded;      // pre-decoded instruction
    uint32_t  addr;         // original PC
    uint8_t   is_branch;    // is this a control flow instruction?
    uint8_t   is_target;    // is this a branch target?
} JITCacheEntry;

typedef struct {
    uint32_t start_addr;    // BB entry PC
    uint32_t end_addr;      // BB exit PC (after delay slot)
    uint32_t hit_count;     // execution counter
    uint32_t next_bb;       // fall-through successor
    uint32_t branch_bb;     // branch target successor
} JITBasicBlock;
```

## Sage Backend Implementation

**File:** `src/sage/mips_jit.sage`

The Sage version mirrors the C backend using Sage's native data structures:
- `self.cache` — Dictionary mapping PC strings to `MipsInstr` objects
- `self.blocks` — Array of dictionary-based basic block records
- `self.block_map` — Dictionary mapping start addresses to block indices

## Performance Impact

JIT optimization provides the largest benefit for:
- **Loops**: The loop body's instructions are decoded once, cached, and re-executed from cache
- **Hot functions**: Frequently called functions benefit from pre-built basic block chains
- **Large programs**: Decoding overhead is amortized as cache fills

Expected improvement: **10-30%** reduction in per-instruction overhead for loop-heavy code.

## Build

The JIT is always compiled in. Enable at runtime with `--jit`:
```bash
make JIT=1          # ensure mips_jit.c is compiled
make                # standard build (JIT available at runtime)
```

## Limitations

- Cache is per-execution (not persistent across runs)
- Hot threshold is fixed at 50 (not adaptive)
- Native code generation (true JIT to x86-64) is not yet implemented
- Basic block chaining does not accelerate indirect jumps (JR/JALR)
