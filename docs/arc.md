# SageMips ARC — Automatic Reference Counting

## Overview

ARC provides deterministic memory management for the VM's string pool by tracking reference counts on every allocation. When a string's reference count drops to zero, it is immediately freed — no garbage collection pauses, no stop-the-world events.

Both the C backend (`src/c/mips_arc.c`) and Sage backend (`src/sage/mips_arc.sage`) implement identical ARC semantics.

## How It Works

### Reference Counting

Every string allocation in the VM is tracked with a reference count:

```
Allocate "hello" → refcount = 1
Copy/clone       → refcount++ (retain)
Overwrite/free   → refcount-- (release)
Refcount == 0    → free immediately
```

### String Pool Tracking

The ARC state maintains a table of `ARCStringEntry` records:
```
Entry 0: "Hello, SageMips!"  refcount=1  offset=0   len=18
Entry 1: "Answer: "          refcount=1  offset=19  len=8
Entry 2: "42"                refcount=0  offset=27  len=2  (freed)
```

### Operations

| Function | Description |
|----------|-------------|
| `mips_arc_track_string()` | Register a new allocation (refcount=1) or increment existing |
| `mips_arc_retain()` | Increment refcount (retain) |
| `mips_arc_release()` | Decrement refcount, free at 0 |
| `mips_arc_stats()` | Return allocs, frees, live counts |

## Usage

```bash
sagemips run program.mips --arc
```

ARC statistics after execution:
```
ARC: reference counting enabled
ARC stats: 42 allocs, 38 frees, 4 live
```

## When to Use

- **Long-running programs**: ARC prevents unbounded memory growth
- **String-heavy workloads**: Immediate deallocation keeps pool size small
- **Embedded/bare-metal**: No GC pauses, deterministic behavior

## Limitations

- No cycle detection: circular references between strings are not collected (use ORC)
- Overhead: every allocation/deallocation incurs table lookup
- Pool fragmentation: freed entries leave gaps (compaction not implemented)

## C Backend Implementation

**File:** `src/c/mips_arc.c`

- `ARCStringEntry`: tracks offset, length, and refcount per string
- Hash-free linear scan for finding existing entries (simplicity over speed)
- Refcount of -1 marks freed/unused entries
