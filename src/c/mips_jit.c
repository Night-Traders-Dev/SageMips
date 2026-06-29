#include "sagemips.h"

// ============================================================================
// SageMips JIT — Instruction Cache + Basic Block Profiler
// ============================================================================
//
// The JIT accelerates interpretation by:
//   1. Caching decoded instructions — each PC decoded once, reused
//   2. Basic block detection — groups sequential instructions into blocks
//   3. Hot block profiling — counts executions to identify hot paths
//   4. Direct block chaining — pre-computed next-BB for fast dispatch
//
// The JIT integrates with the VM's step function. When enabled via --jit,
// the VM uses cached instruction data instead of re-decoding every cycle.
// ============================================================================

#define JIT_HASH_BITS  16
#define JIT_HASH_SIZE  (1 << JIT_HASH_BITS)
#define JIT_HASH_MASK  (JIT_HASH_SIZE - 1)

static inline uint32_t jit_hash_pc(uint32_t pc) {
    return (pc >> 2) & JIT_HASH_MASK;
}

void mips_jit_init(MipsJITState* jit, MipsVM* vm) {
    (void)vm;
    for (uint32_t i = 0; i < JIT_CACHE_SIZE; i++) {
        jit->cache[i].addr = 0xFFFFFFFF;
        jit->cache[i].next_cache = 0xFFFFFFFF;
        jit->cache[i].is_branch = 0;
        jit->cache[i].is_target = 0;
    }
    jit->cache_count = 0;
    for (uint32_t i = 0; i < JIT_BB_MAX; i++) {
        jit->blocks[i].start_addr = 0xFFFFFFFF;
        jit->blocks[i].end_addr = 0xFFFFFFFF;
        jit->blocks[i].hit_count = 0;
        jit->blocks[i].next_bb = 0xFFFFFFFF;
        jit->blocks[i].branch_bb = 0xFFFFFFFF;
    }
    jit->block_count = 0;
    jit->hot_threshold = 50;
    jit->total_steps = 0;
    jit->enabled = 1;
}

// Find or insert a cache entry
static JITCacheEntry* jit_cache_get(JITCacheEntry* cache, uint32_t addr) {
    uint32_t h = jit_hash_pc(addr);
    for (int probe = 0; probe < 8; probe++) {
        if (cache[h].addr == addr || cache[h].addr == 0xFFFFFFFF) {
            return &cache[h];
        }
        h = (h + 1) & JIT_HASH_MASK;
    }
    return NULL;
}

// Pre-decode and cache an instruction at PC
int mips_jit_cache_instr(MipsJITState* jit, MipsVM* vm, uint32_t pc, JITCacheEntry** out) {
    if (!jit || !jit->enabled) return 0;
    if (pc + 4 > vm->code_length) return 0;

    JITCacheEntry* ce = jit_cache_get(jit->cache, pc);
    if (!ce) return 0;

    if (ce->addr != pc) {
        // First time — decode and cache
        uint32_t raw = ((uint32_t)vm->code[pc] << 24) |
                       ((uint32_t)vm->code[pc+1] << 16) |
                       ((uint32_t)vm->code[pc+2] << 8) |
                       (uint32_t)vm->code[pc+3];
        ce->decoded = mips_decode(raw);
        ce->addr = pc;
        ce->is_branch = 0;
        ce->is_target = 0;
        // Check if branch
        int op = ce->decoded.opcode;
        int fn = ce->decoded.funct;
        if (op == MIPS_OP_J || op == MIPS_OP_JAL || op == MIPS_OP_BEQ ||
            op == MIPS_OP_BNE || op == MIPS_OP_BLEZ || op == MIPS_OP_BGTZ ||
            op == MIPS_OP_REGIMM || (op == MIPS_OP_SPECIAL &&
            (fn == MIPS_FN_JR || fn == MIPS_FN_JALR))) {
            ce->is_branch = 1;
        }
        jit->cache_count++;
    }

    if (out) *out = ce;
    return 1; // cache hit or newly cached
}

// Mark an address as a branch target (for basic block detection)
void mips_jit_mark_target(MipsJITState* jit, uint32_t pc) {
    if (!jit || !jit->enabled) return;
    JITCacheEntry* ce = jit_cache_get(jit->cache, pc);
    if (ce) ce->is_target = 1;
}

// Build basic block table by scanning code
void mips_jit_warmup(MipsJITState* jit, MipsVM* vm) {
    if (!jit || !jit->enabled) return;

    // First pass: cache all instructions and mark branch targets
    uint32_t pc = 0;
    while (pc + 4 <= vm->code_length) {
        mips_jit_cache_instr(jit, vm, pc, NULL);

        // If this is a branch, mark the target
        JITCacheEntry* ce = jit_cache_get(jit->cache, pc);
        if (ce && ce->is_branch) {
            int32_t imm = ce->decoded.imm;
            uint32_t target = pc + 4 + ((int32_t)imm << 2);
            if (target < vm->code_length) {
                mips_jit_mark_target(jit, target);
            }
            // For J-type
            if (ce->decoded.opcode == MIPS_OP_J || ce->decoded.opcode == MIPS_OP_JAL) {
                target = (pc & 0xF0000000) | (ce->decoded.target << 2);
                if (target < vm->code_length) {
                    mips_jit_mark_target(jit, target);
                }
            }
        }
        pc += 4;
    }

    // Second pass: build basic blocks (split at branches and targets)
    pc = 0;
    while (pc + 4 <= vm->code_length && jit->block_count < JIT_BB_MAX) {
        JITCacheEntry* ce = jit_cache_get(jit->cache, pc);
        if (!ce) { pc += 4; continue; }

        // Start new BB
        JITBasicBlock* bb = &jit->blocks[jit->block_count];
        bb->start_addr = pc;
        bb->hit_count = 0;
        bb->next_bb = 0xFFFFFFFF;
        bb->branch_bb = 0xFFFFFFFF;

        // Scan forward
        uint32_t end = pc;
        while (end + 4 <= vm->code_length) {
            JITCacheEntry* e = jit_cache_get(jit->cache, end);
            if (e && e->is_target && end != pc) break; // stop at next target
            end += 4;
            if (e && e->is_branch) break; // stop at branch (include it)
        }
        bb->end_addr = end;

        // Chain fall-through
        if (end < vm->code_length) {
            bb->next_bb = end;
        }

        // Chain branch target
        if (end >= 8) {
            uint32_t br_pc = end - 4;
            JITCacheEntry* br = jit_cache_get(jit->cache, br_pc);
            if (br && br->is_branch) {
                int32_t imm = br->decoded.imm;
                uint32_t target = br_pc + 4 + ((int32_t)imm << 2);
                if (target < vm->code_length) {
                    bb->branch_bb = target;
                }
                if (br->decoded.opcode == MIPS_OP_J || br->decoded.opcode == MIPS_OP_JAL) {
                    target = (br_pc & 0xF0000000) | (br->decoded.target << 2);
                    if (target < vm->code_length) bb->branch_bb = target;
                }
            }
        }

        jit->block_count++;
        pc = end;
    }
}

// Record that a BB was executed (profiling)
void mips_jit_hit_bb(MipsJITState* jit, uint32_t pc) {
    if (!jit || !jit->enabled) return;
    for (uint32_t i = 0; i < jit->block_count; i++) {
        if (jit->blocks[i].start_addr == pc) {
            jit->blocks[i].hit_count++;
            return;
        }
    }
}

void mips_jit_stats(MipsJITState* jit, int* cache_entries, int* bb_count) {
    if (jit) {
        *cache_entries = (int)jit->cache_count;
        *bb_count = (int)jit->block_count;
    } else {
        *cache_entries = 0;
        *bb_count = 0;
    }
}
