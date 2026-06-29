# SageMips ORC — Optimized Reference Counting

## Overview

ORC extends ARC with periodic cycle detection to prevent memory leaks from circular references. It uses a simplified trial-deletion algorithm — periodically scanning the object graph, identifying unreachable cycles, and collecting garbage objects that ARC alone would miss.

Both the C backend (`src/c/mips_orc.c`) and Sage backend (`src/sage/mips_orc.sage`) implement identical ORC semantics.

## How It Works

### Trial Deletion Algorithm

ORC uses a 3-phase cycle detection scan triggered every N allocations (default: 256):

**Phase 1 — Mark White**: All tracked objects are marked as potentially unreachable.

**Phase 2 — Mark Black**: Starting from root references (objects with external references), traverse the graph and mark reachable objects as BLACK.

**Phase 3 — Trial Delete**: For remaining WHITE objects (suspect cycles):
1. Temporarily decrement their reference counts
2. If refcount reaches 0, the object is part of an unreferenced cycle
3. Collect the object (mark as freed)

### Color States

| Color | Meaning |
|-------|---------|
| WHITE (0) | Potentially unreachable — candidate for collection |
| GRAY (1) | Being scanned — in the mark queue |
| BLACK (2) | Definitely reachable — keep |
| PURPLE (3) | Possible cycle root — needs investigation |

### Scan Trigger

```
Allocations since last scan ≥ ORC_CYCLE_THRESHOLD (256) → run cycle detection
```

## Usage

```bash
sagemips run program.mips --orc       # ORC auto-enables ARC
sagemips run program.mips --arc --orc  # explicit both
```

ORC statistics after execution:
```
ORC: cycle detection enabled
ORC stats: 3 cycles, 12 collected
```

## When to Use

- **Complex object graphs**: When strings reference other strings
- **Long-running servers**: Prevent slow memory leaks from cycles
- **With ARC**: ORC requires ARC for reference counting; it auto-enables ARC if not already active

## C Backend Implementation

**File:** `src/c/mips_orc.c`

- `ORCEntry`: maps ARC entries to ORC color/cycle data
- `orc_check_cycle()`: runs the 3-phase scan
- `orc_notify_alloc()`: increments allocation counter, triggers scan at threshold
- `orc_trial_delete()`: attempts to collect white objects

## ARC vs ORC

| | ARC | ORC |
|---|-----|-----|
| Strategy | Reference counting | Trial deletion + ref counting |
| Cycle handling | No (leaks cycles) | Yes |
| Pauses | None | Periodic (every 256 allocs) |
| Overhead | Per-allocation | Per-scan |
| Best for | Simple programs | Complex object graphs |
