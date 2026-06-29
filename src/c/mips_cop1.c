#include "sagemips.h"
#include <math.h>

// ============================================================================
// MIPS COP1 — Floating-Point Coprocessor
// ============================================================================
// Adds 32 32-bit floating-point registers ($f0-$f31) and implements:
//   - add.s, sub.s, mul.s, div.s (single-precision arithmetic)
//   - add.d, sub.d, mul.d, div.d (double-precision arithmetic)  
//   - lwc1, swc1 (load/store word to/from FP register)
//   - mfc1, mtc1 (move between GP and FP registers)
//   - c.eq.s, c.lt.s, c.le.s (floating-point comparisons)
//   - bc1t, bc1f (branch on FP condition)
//   - cvt.s.w, cvt.w.s (conversion)
//   - sqrt.s, sqrt.d (square root)
//   - abs.s, neg.s, mov.s
// ============================================================================

#define COP1_NUM_REGS 32

// Helper: read single-precision float from GP register
static float cop1_get_float(MipsVM* vm, int reg) {
    union { float f; uint32_t u; } u;
    u.u = (uint32_t)vm->regs[reg].as.i32;
    return u.f;
}

// Helper: write single-precision float to GP register
static void cop1_set_float(MipsVM* vm, int reg, float val) {
    union { float f; uint32_t u; } u;
    u.f = val;
    vm->regs[reg] = mv_int((int32_t)u.u);
}

// Get double from two consecutive GP registers
static double cop1_get_double(MipsVM* vm, int reg) {
    union { double d; uint32_t u[2]; } u;
    u.u[1] = (uint32_t)vm->regs[reg].as.i32;
    u.u[0] = (uint32_t)vm->regs[reg+1].as.i32;
    return u.d;
}

// Execute COP1 instruction. Returns 1 if instruction handled, 0 if not.
int mips_cop1_execute(MipsVM* vm, uint32_t raw, MipsCOP1State* cop1) {
    if (!cop1) return 0;
    uint32_t fmt = (raw >> 21) & 0x1F;  // format: 16=single, 17=double, 20=word
    uint32_t ft  = (raw >> 16) & 0x1F;
    uint32_t fs  = (raw >> 11) & 0x1F;
    uint32_t fd  = (raw >> 6)  & 0x1F;
    uint32_t funct = raw & 0x3F;
    uint32_t opcode = (raw >> 26) & 0x3F;

    // LWC1 / SWC1: opcode=0x31/0x39
    if (opcode == 0x31) { // LWC1
        int base = (raw >> 21) & 0x1F;
        int16_t imm = raw & 0xFFFF;
        int32_t addr = mips_signed(vm->regs[base]) + imm;
        if (addr >= 0 && (uint32_t)(addr+4) <= MIPS_STACK_SIZE*4) {
            uint32_t wi = (uint32_t)addr / 4;
            union { float f; uint32_t u; } u;
            u.u = vm->stack[wi];
            cop1->f[ft] = u.f;
        }
        return 1;
    }
    if (opcode == 0x39) { // SWC1
        int base = (raw >> 21) & 0x1F;
        int16_t imm = raw & 0xFFFF;
        int32_t addr = mips_signed(vm->regs[base]) + imm;
        if (addr >= 0 && (uint32_t)(addr+4) <= MIPS_STACK_SIZE*4) {
            uint32_t wi = (uint32_t)addr / 4;
            union { float f; uint32_t u; } u;
            u.f = cop1->f[ft];
            vm->stack[wi] = u.u;
        }
        return 1;
    }

    // COP1 arithmetic (opcode 0x11)
    if (opcode != 0x11) return 0;

    if (fmt == 16) { // Single precision
        switch (funct) {
            case 0x00: cop1->f[fd] = cop1->f[fs] + cop1->f[ft]; break; // add.s
            case 0x01: cop1->f[fd] = cop1->f[fs] - cop1->f[ft]; break; // sub.s
            case 0x02: cop1->f[fd] = cop1->f[fs] * cop1->f[ft]; break; // mul.s
            case 0x03: cop1->f[fd] = cop1->f[fs] / cop1->f[ft]; break; // div.s
            case 0x04: cop1->f[fd] = sqrtf(cop1->f[fs]); break;        // sqrt.s
            case 0x05: cop1->f[fd] = fabsf(cop1->f[fs]); break;        // abs.s
            case 0x06: cop1->f[fd] = (float)(int32_t)cop1->f[fs]; break; // cvt.w.s -> stored as float
            case 0x07: cop1->f[fd] = -cop1->f[fs]; break;              // neg.s
            case 0x20: cop1->f[fd] = (float)((int)cop1->f[fs]); break; // cvt.s.w
            case 0x30: cop1->cond = (cop1->f[fs] == cop1->f[ft]); break; // c.eq.s
            case 0x32: cop1->cond = (cop1->f[fs] <  cop1->f[ft]); break; // c.lt.s
            case 0x3C: cop1->cond = (cop1->f[fs] <= cop1->f[ft]); break; // c.le.s
        }
    } else if (fmt == 17) { // Double precision
        switch (funct) {
            case 0x00: cop1->d[fd/2] = cop1->d[fs/2] + cop1->d[ft/2]; break;
            case 0x01: cop1->d[fd/2] = cop1->d[fs/2] - cop1->d[ft/2]; break;
            case 0x02: cop1->d[fd/2] = cop1->d[fs/2] * cop1->d[ft/2]; break;
            case 0x03: cop1->d[fd/2] = cop1->d[fs/2] / cop1->d[ft/2]; break;
            case 0x04: cop1->d[fd/2] = sqrt(cop1->d[fs/2]); break;
            case 0x05: cop1->d[fd/2] = fabs(cop1->d[fs/2]); break;
            case 0x07: cop1->d[fd/2] = -cop1->d[fs/2]; break;
            case 0x30: cop1->cond = (cop1->d[fs/2] == cop1->d[ft/2]); break;
            case 0x32: cop1->cond = (cop1->d[fs/2] <  cop1->d[ft/2]); break;
            case 0x3C: cop1->cond = (cop1->d[fs/2] <= cop1->d[ft/2]); break;
        }
    } else if (fmt == 0) { // MFC1/CFC1
        if (funct == 0x00) cop1_set_float(vm, ft, cop1->f[fs]); // MFC1
        if (funct == 0x02) vm->regs[ft] = mv_int(cop1->cond);   // CFC1
    } else if (fmt == 4) { // MTC1
        if (funct == 0x00) cop1->f[fs] = cop1_get_float(vm, ft);
    }
    return 1;
}

// Handle BC1T/BC1F branch instructions (opcode 0x11, sub-op)
int mips_cop1_branch(MipsVM* vm, uint32_t raw, MipsCOP1State* cop1) {
    if (!cop1) return 0;
    uint32_t opcode = (raw >> 26) & 0x3F;
    if (opcode != 0x11) return 0;
    uint32_t nd = (raw >> 17) & 0x01; // 0=BC1F, 1=BC1T
    uint32_t tf = (raw >> 16) & 0x01;
    int16_t imm = raw & 0xFFFF;
    if (nd == 0 && tf == 0) { // BC1F
        if (!cop1->cond) vm->pc = vm->pc + 4 + ((int32_t)imm << 2);
        else vm->pc += 4;
        return 1;
    }
    if (nd == 0 && tf == 1) { // BC1T
        if (cop1->cond) vm->pc = vm->pc + 4 + ((int32_t)imm << 2);
        else vm->pc += 4;
        return 1;
    }
    return 0;
}
