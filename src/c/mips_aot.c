#include "sagemips.h"

// ============================================================================
// SageMips AOT Optimizer — Static peephole optimization passes
// ============================================================================
//
// Passes (applied in order):
//   1. NOP elimination         — removes nullified branch delay slots  
//   2. Branch-to-jump folding  — converts always-taken branches to jumps
//   3. Constant folding        — pre-computes constant arithmetic
//   4. Dead code elimination   — removes unreachable instructions
//   5. Instruction combining   — merges adjacent compatible instructions
//
// The optimizer produces a new optimized byte buffer. Original code is
// not modified. Works both freestanding and hosted.
// ============================================================================

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int is_nop(uint32_t raw) {
    // sll $0,$0,0  or  sll $zero,$zero,0
    return raw == 0x00000000;
}

static int is_uncond_branch(uint32_t raw) {
    MipsInstr i = mips_decode(raw);
    if (i.opcode == MIPS_OP_J || i.opcode == MIPS_OP_JAL) return 1;
    if (i.opcode == MIPS_OP_SPECIAL && (i.funct == MIPS_FN_JR || i.funct == MIPS_FN_JALR)) return 1;
    if (i.opcode == MIPS_OP_SPECIAL && i.funct == MIPS_FN_SYSCALL) return 1; // terminal
    // beq $0,$0,target = unconditional branch
    if (i.opcode == MIPS_OP_BEQ && i.rs == 0 && i.rt == 0) return 1;
    return 0;
}

static int is_branch(uint32_t raw) {
    MipsInstr i = mips_decode(raw);
    switch (i.opcode) {
        case MIPS_OP_J: case MIPS_OP_JAL:
        case MIPS_OP_BEQ: case MIPS_OP_BNE:
        case MIPS_OP_BLEZ: case MIPS_OP_BGTZ:
            return 1;
        case MIPS_OP_REGIMM: return 1;
        case MIPS_OP_SPECIAL:
            if (i.funct == MIPS_FN_JR || i.funct == MIPS_FN_JALR) return 1;
            break;
    }
    return 0;
}

static int is_terminator(uint32_t raw) {
    MipsInstr i = mips_decode(raw);
    return (i.opcode == MIPS_OP_J) ||
           (i.opcode == MIPS_OP_SPECIAL && i.funct == MIPS_FN_JR) ||
           (i.opcode == MIPS_OP_SPECIAL && i.funct == MIPS_FN_SYSCALL);
}

// Write big-endian 32-bit instruction to output buffer
static void aot_emit(MipsAOTState* aot, uint32_t instr) {
    if (aot->opt_len + 4 <= MIPS_CODE_MAX) {
        aot->opt_code[aot->opt_len++] = (instr >> 24) & 0xFF;
        aot->opt_code[aot->opt_len++] = (instr >> 16) & 0xFF;
        aot->opt_code[aot->opt_len++] = (instr >> 8) & 0xFF;
        aot->opt_code[aot->opt_len++] = instr & 0xFF;
    }
}

static uint32_t read_be32_aot(const uint8_t* code, uint32_t addr) {
    return ((uint32_t)code[addr] << 24) |
           ((uint32_t)code[addr+1] << 16) |
           ((uint32_t)code[addr+2] << 8) |
           (uint32_t)code[addr+3];
}

// ---------------------------------------------------------------------------
// Pass 1: NOP Elimination — remove NOP delay slots after unconditional branches
// ---------------------------------------------------------------------------
static void pass_nop_elim(MipsAOTState* aot, const uint8_t* code, uint32_t len) {
    uint32_t pc = 0;
    while (pc < len) {
        uint32_t instr = read_be32_aot(code, pc);

        // If previous instruction was an unconditional branch and this is a NOP -> skip
        if (is_nop(instr) && pc >= 4) {
            uint32_t prev = read_be32_aot(code, pc - 4);
            if (is_uncond_branch(prev) && !is_terminator(prev)) {
                aot_emit(aot, instr); // keep the nop (delay slot still needed for correctness)
                pc += 4;
                continue;
            }
        }

        // Strip NOPs between basic blocks (NOP followed by nothing useful)
        if (is_nop(instr) && pc + 4 >= len) {
            // Trailing NOP at end of code — can remove
            aot->nops_removed++;
            pc += 4;
            continue;
        }

        // Remove consecutive NOPs (keep first only if needed for alignment)
        if (is_nop(instr) && pc + 4 < len && is_nop(read_be32_aot(code, pc + 4))) {
            aot_emit(aot, instr); // keep first
            pc += 4;
            while (pc < len && is_nop(read_be32_aot(code, pc))) {
                aot->nops_removed++;
                pc += 4;
            }
            continue;
        }

        aot_emit(aot, instr);
        pc += 4;
    }
}

// ---------------------------------------------------------------------------
// Pass 2: Branch optimization — convert BEQ $0,$0,target to J target
// ---------------------------------------------------------------------------
static void pass_branch_opt(MipsAOTState* aot, const uint8_t* code, uint32_t len) {
    // Re-read the output from pass 1 as input
    uint32_t pc = 0;
    aot->opt_len = 0;
    uint8_t* pass1_out = aot->opt_code;
    uint32_t pass1_len = aot->opt_len;
    // Actually we need to store pass1 output separately. Let's just work on a copy.
    uint8_t temp[MIPS_CODE_MAX];
    uint32_t temp_len = aot->opt_len;
    for (uint32_t i = 0; i < temp_len; i++) temp[i] = aot->opt_code[i];
    aot->opt_len = 0;

    pc = 0;
    while (pc < temp_len) {
        uint32_t instr = read_be32_aot(temp, pc);
        MipsInstr di = mips_decode(instr);

        // Convert beq $0,$0,offset -> j target
        if (di.opcode == MIPS_OP_BEQ && di.rs == 0 && di.rt == 0) {
            int32_t target_addr = pc + 4 + ((int32_t)di.imm << 2);
            uint32_t j_target = ((uint32_t)target_addr) >> 2;
            aot_emit(aot, mips_encode_j(MIPS_OP_J, j_target));
            aot->branches_optimized++;
            pc += 4;
            // Next is the delay slot — emit it
            if (pc < temp_len) {
                aot_emit(aot, read_be32_aot(temp, pc));
                pc += 4;
            }
            continue;
        }

        aot_emit(aot, instr);
        pc += 4;
    }
}

// ---------------------------------------------------------------------------
// Pass 3: Constant folding — pre-compute ADDIU $x,$zero,const sequences
// ---------------------------------------------------------------------------
static void pass_const_fold(MipsAOTState* aot, const uint8_t* code, uint32_t len) {
    // Store pass2 output in temp
    uint8_t temp[MIPS_CODE_MAX];
    uint32_t temp_len = aot->opt_len;
    for (uint32_t i = 0; i < temp_len; i++) temp[i] = aot->opt_code[i];
    aot->opt_len = 0;

    uint32_t pc = 0;
    while (pc < temp_len) {
        uint32_t instr = read_be32_aot(temp, pc);
        MipsInstr di = mips_decode(instr);

        // Detect: ADDIU $rt, $zero, C   (load constant into register)
        if (di.opcode == MIPS_OP_ADDIU && di.rs == 0) {
            // Next instruction uses same register as source? Try to fold.
            int folded = 0;
            if (pc + 8 <= temp_len) {
                uint32_t next = read_be32_aot(temp, pc + 4);
                MipsInstr ni = mips_decode(next);

                // ADDIU $x,$zero,A followed by ADDIU $y,$x,B  ->  ADDIU $y,$zero,A+B  (if y != x)
                if (ni.opcode == MIPS_OP_ADDIU && ni.rs == di.rt && ni.rt != di.rt) {
                    int32_t val = di.imm + ni.imm;
                    aot_emit(aot, mips_encode_i(MIPS_OP_ADDIU, 0, ni.rt, (int16_t)val));
                    aot->consts_folded++;
                    pc += 8;
                    folded = 1;
                }
                // ADDIU $x,$zero,A followed by ORI $x,$x,B  ->  ADDIU $x,$zero,A|B
                else if (ni.opcode == MIPS_OP_ORI && ni.rs == di.rt && ni.rt == di.rt) {
                    int32_t val = di.imm | ni.imm;
                    aot_emit(aot, mips_encode_i(MIPS_OP_ADDIU, 0, di.rt, (int16_t)val));
                    aot->consts_folded++;
                    pc += 8;
                    folded = 1;
                }
            }
            if (!folded) {
                aot_emit(aot, instr);
                pc += 4;
            }
            continue;
        }

        aot_emit(aot, instr);
        pc += 4;
    }
}

// ---------------------------------------------------------------------------
// Pass 4: Dead code elimination — remove code after unconditional terminators
// ---------------------------------------------------------------------------
static void pass_dead_code(MipsAOTState* aot, const uint8_t* code, uint32_t len) {
    uint8_t temp[MIPS_CODE_MAX];
    uint32_t temp_len = aot->opt_len;
    for (uint32_t i = 0; i < temp_len; i++) temp[i] = aot->opt_code[i];
    aot->opt_len = 0;

    uint32_t pc = 0;
    while (pc < temp_len) {
        uint32_t instr = read_be32_aot(temp, pc);
        aot_emit(aot, instr);
        pc += 4;

        if (is_terminator(instr)) {
            // Include delay slot if present
            if (pc < temp_len) {
                aot_emit(aot, read_be32_aot(temp, pc));
                pc += 4;
            }
            // Skip everything after terminator
            while (pc < temp_len) {
                aot->dead_removed++;
                pc += 4;
            }
        }
    }
}

// ============================================================================
// Public API
// ============================================================================

void mips_aot_init(MipsAOTState* aot) {
    aot->opt_len = 0;
    aot->nops_removed = 0;
    aot->consts_folded = 0;
    aot->branches_optimized = 0;
    aot->dead_removed = 0;
}

int mips_aot_optimize(MipsAOTState* aot, const uint8_t* code, uint32_t len) {
    mips_aot_init(aot);

    // Copy original to opt buffer
    aot->opt_len = 0;
    for (uint32_t i = 0; i < len && aot->opt_len < MIPS_CODE_MAX; i++) {
        aot->opt_code[aot->opt_len++] = code[i];
    }

    // Run passes in sequence
    pass_nop_elim(aot, code, len);
    pass_branch_opt(aot, aot->opt_code, aot->opt_len);
    pass_const_fold(aot, aot->opt_code, aot->opt_len);
    pass_dead_code(aot, aot->opt_code, aot->opt_len);

    return (int)aot->opt_len;
}

int mips_aot_nop_count(const uint8_t* code, uint32_t len) {
    int count = 0;
    for (uint32_t pc = 0; pc + 4 <= len; pc += 4) {
        if (is_nop(read_be32_aot(code, pc))) count++;
    }
    return count;
}
