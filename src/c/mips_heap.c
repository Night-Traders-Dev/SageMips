#include "sagemips.h"

// ============================================================================
// SageMips Heap Manager — malloc/free with free-list
// ============================================================================
// Replaces the simple bump allocator with a proper free-list allocator.
// Each block has a header (size, next pointer, free flag) followed by data.
// free() adds blocks to the free list. malloc() searches the free list first,
// then expands the heap. Adjacent free blocks are coalesced.
// ============================================================================

typedef struct HeapBlock {
    uint32_t size;              // total block size including header
    uint32_t next;              // offset to next block (0 = end)
    uint8_t  free;              // 1 = free, 0 = allocated
    uint8_t  pad[3];
} HeapBlock;

#define HEAP_BLOCK_HEADER 16    // sizeof(HeapBlock) rounded up

void mips_heap_init(MipsVM* vm) {
    vm->heap_used = HEAP_BLOCK_HEADER; // first block header
    HeapBlock* first = (HeapBlock*)vm->heap;
    first->size = MIPS_HEAP_SIZE - HEAP_BLOCK_HEADER;
    first->next = 0;
    first->free = 1;
}

uint32_t mips_malloc(MipsVM* vm, uint32_t size) {
    if (size == 0) return 0;
    // Align to 8 bytes
    size = (size + 7) & ~7;

    uint32_t offset = HEAP_BLOCK_HEADER;
    uint32_t prev = 0;

    while (offset < vm->heap_used) {
        HeapBlock* blk = (HeapBlock*)(vm->heap + offset);
        if (blk->free && blk->size >= size + HEAP_BLOCK_HEADER) {
            // Found a fitting free block
            uint32_t remaining = blk->size - size - HEAP_BLOCK_HEADER;
            if (remaining >= HEAP_BLOCK_HEADER + 8) {
                // Split: allocate first part, keep remainder as free
                uint32_t next_off = offset + HEAP_BLOCK_HEADER + size;
                HeapBlock* next_blk = (HeapBlock*)(vm->heap + next_off);
                next_blk->size = remaining;
                next_blk->next = blk->next;
                next_blk->free = 1;
                blk->size = size + HEAP_BLOCK_HEADER;
                blk->next = next_off;
            }
            blk->free = 0;
            return offset + HEAP_BLOCK_HEADER;
        }
        prev = offset;
        offset = blk->next;
        if (offset == 0) break;
    }

    // No free block found — expand heap
    if (vm->heap_used + size + HEAP_BLOCK_HEADER > MIPS_HEAP_SIZE) return 0;
    uint32_t new_off = vm->heap_used;
    HeapBlock* blk = (HeapBlock*)(vm->heap + new_off);
    blk->size = size + HEAP_BLOCK_HEADER;
    blk->next = 0;
    blk->free = 0;
    // Link previous block
    if (prev > 0) {
        HeapBlock* pblk = (HeapBlock*)(vm->heap + prev);
        pblk->next = new_off;
    }
    vm->heap_used = new_off + size + HEAP_BLOCK_HEADER;
    return new_off + HEAP_BLOCK_HEADER;
}

void mips_free(MipsVM* vm, uint32_t ptr) {
    if (ptr < HEAP_BLOCK_HEADER) return;
    uint32_t off = ptr - HEAP_BLOCK_HEADER;
    if (off >= vm->heap_used) return;
    HeapBlock* blk = (HeapBlock*)(vm->heap + off);
    blk->free = 1;

    // Coalesce with next block if free
    if (blk->next > 0 && blk->next < MIPS_HEAP_SIZE) {
        HeapBlock* next_blk = (HeapBlock*)(vm->heap + blk->next);
        if (next_blk->free) {
            blk->size += next_blk->size;
            blk->next = next_blk->next;
        }
    }

    // Coalesce with previous block if free (scan backward)
    uint32_t scan = HEAP_BLOCK_HEADER;
    while (scan < off) {
        HeapBlock* cur = (HeapBlock*)(vm->heap + scan);
        uint32_t next = cur->next ? cur->next : vm->heap_used;
        if (next == off && cur->free) {
            cur->size += blk->size;
            cur->next = blk->next;
            break;
        }
        if (cur->next == 0) break;
        scan = cur->next;
    }
}
