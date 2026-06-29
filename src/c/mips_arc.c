#include "sagemips.h"

// ============================================================================
// SageMips ARC — Automatic Reference Counting for VM string pool
// ============================================================================
//
// Tracks reference counts on string pool entries. When a string's refcount
// drops to 0, it can be freed from the pool. This provides deterministic
// memory management for strings without garbage collection pauses.
//
// ARC operates transparently — string operations in the VM automatically
// retain/release references through the ARC state.
// ============================================================================

void mips_arc_init(MipsARCState* arc) {
    for (uint32_t i = 0; i < ARC_STRING_POOL_MAX; i++) {
        arc->entries[i].refcount = -1;  // unused
        arc->entries[i].str_offset = 0;
        arc->entries[i].str_len = 0;
    }
    arc->entry_count = 0;
    arc->total_allocs = 0;
    arc->total_frees = 0;
    arc->leak_count = 0;
    arc->enabled = 1;
}

// Find an existing entry by string offset
static int arc_find_by_offset(MipsARCState* arc, uint32_t offset) {
    for (uint32_t i = 0; i < arc->entry_count; i++) {
        if (arc->entries[i].refcount >= 0 && arc->entries[i].str_offset == offset)
            return (int)i;
    }
    return -1;
}

// Track a new string allocation. Returns entry index.
int mips_arc_track_string(MipsARCState* arc, uint32_t str_offset, uint32_t str_len) {
    if (!arc || !arc->enabled) return -1;
    if (arc->entry_count >= ARC_STRING_POOL_MAX) return -1;

    // Check if already tracked
    int existing = arc_find_by_offset(arc, str_offset);
    if (existing >= 0) {
        arc->entries[existing].refcount++;
        arc->total_allocs++;
        return existing;
    }

    uint32_t idx = arc->entry_count++;
    arc->entries[idx].refcount = 1;
    arc->entries[idx].str_offset = str_offset;
    arc->entries[idx].str_len = str_len;
    arc->total_allocs++;
    return (int)idx;
}

// Increment reference count (retain)
void mips_arc_retain(MipsARCState* arc, int entry_idx) {
    if (!arc || !arc->enabled) return;
    if (entry_idx < 0 || (uint32_t)entry_idx >= arc->entry_count) return;
    if (arc->entries[entry_idx].refcount > 0)
        arc->entries[entry_idx].refcount++;
}

// Decrement reference count (release). Frees string at refcount 0.
void mips_arc_release(MipsARCState* arc, int entry_idx, MipsVM* vm) {
    if (!arc || !arc->enabled) return;
    if (entry_idx < 0 || (uint32_t)entry_idx >= arc->entry_count) return;

    ARCStringEntry* e = &arc->entries[entry_idx];
    if (e->refcount <= 0) return; // already freed or static

    e->refcount--;
    if (e->refcount == 0) {
        arc->total_frees++;
        // Mark as freed (refcount = -1)
        e->refcount = -1;
        (void)vm; // VM may compact the pool later
    }
}

void mips_arc_stats(MipsARCState* arc, int* allocs, int* frees, int* leaks) {
    if (arc) {
        *allocs = (int)arc->total_allocs;
        *frees  = (int)arc->total_frees;
        *leaks  = 0;
        for (uint32_t i = 0; i < arc->entry_count; i++) {
            if (arc->entries[i].refcount > 0) (*leaks)++;
        }
    } else {
        *allocs = *frees = *leaks = 0;
    }
}
