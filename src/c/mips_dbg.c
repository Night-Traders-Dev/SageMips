#include "sagemips.h"

// ============================================================================
// SageMips Interactive Debugger
// ============================================================================
// Commands:
//   b <addr>     Set breakpoint at hex address
//   r            Run until breakpoint or halt
//   s [n]        Step n instructions (default 1)
//   i r          Info registers — dump all 32 GP registers
//   i f          Info FPU — dump COP1 registers
//   i s          Info stack — dump top of stack
//   x <addr>     Examine memory at address (hex dump)
//   c            Continue (run until next breakpoint)
//   q            Quit debugger
//   h            Help
// ============================================================================

void mips_dbg_init(MipsDebugger* dbg) {
    for (int i = 0; i < DBG_MAX_BREAKPOINTS; i++) {
        dbg->bps[i].addr = 0xFFFFFFFF;
        dbg->bps[i].enabled = 0;
    }
    dbg->bp_count = 0;
    dbg->stepping = 0;
    dbg->step_count = 0;
    dbg->running = 0;
    dbg->should_break = 0;
}

int mips_dbg_add_bp(MipsDebugger* dbg, uint32_t addr) {
    if (dbg->bp_count >= DBG_MAX_BREAKPOINTS) return -1;
    // Check if already exists
    for (int i = 0; i < dbg->bp_count; i++) {
        if (dbg->bps[i].addr == addr) {
            dbg->bps[i].enabled = 1;
            return 0;
        }
    }
    dbg->bps[dbg->bp_count].addr = addr;
    dbg->bps[dbg->bp_count].enabled = 1;
    dbg->bp_count++;
    return 0;
}

void mips_dbg_del_bp(MipsDebugger* dbg, uint32_t addr) {
    for (int i = 0; i < dbg->bp_count; i++) {
        if (dbg->bps[i].addr == addr) dbg->bps[i].enabled = 0;
    }
}

int mips_dbg_check_bp(MipsDebugger* dbg, uint32_t pc) {
    if (dbg->stepping) {
        dbg->step_count--;
        if (dbg->step_count <= 0) {
            dbg->stepping = 0;
            return 1;
        }
        return 0;
    }
    for (int i = 0; i < dbg->bp_count; i++) {
        if (dbg->bps[i].enabled && dbg->bps[i].addr == pc) return 1;
    }
    return 0;
}

// Print a single register
static void dbg_print_reg(MipsVM* vm, int reg) {
#ifndef SAGE_BARE_METAL
    fprintf(stderr, "  $%-3s = 0x%08x  %d\n",
        mips_reg_name(reg),
        (uint32_t)vm->regs[reg].as.i32,
        (int32_t)vm->regs[reg].as.i32);
#else
    (void)vm; (void)reg;
#endif
}

void mips_dbg_dump_regs(MipsVM* vm) {
#ifndef SAGE_BARE_METAL
    fprintf(stderr, "Registers:\n");
    for (int i = 0; i < 32; i += 4) {
        fprintf(stderr, "  $%-2d-%-2d: %08x %08x %08x %08x\n",
            i, i+3,
            (uint32_t)vm->regs[i].as.i32, (uint32_t)vm->regs[i+1].as.i32,
            (uint32_t)vm->regs[i+2].as.i32, (uint32_t)vm->regs[i+3].as.i32);
    }
    fprintf(stderr, "  PC = 0x%08x  HI=0x%08llx  LO=0x%08llx\n",
        vm->pc, (unsigned long long)vm->hi, (unsigned long long)vm->lo);
#else
    (void)vm;
#endif
}

void mips_dbg_dump_stack(MipsVM* vm, int words) {
#ifndef SAGE_BARE_METAL
    fprintf(stderr, "Stack (sp=0x%x, top 16 words):\n", vm->sp * 4);
    uint32_t base = vm->sp;
    if (base + (uint32_t)words > MIPS_STACK_SIZE) base = MIPS_STACK_SIZE - words;
    for (int i = 0; i < words; i++) {
        fprintf(stderr, "  [%04x] = 0x%08x\n", (base + i) * 4, vm->stack[base + i]);
    }
#else
    (void)vm; (void)words;
#endif
}

void mips_dbg_dump_mem(MipsVM* vm, uint32_t addr, int bytes) {
#ifndef SAGE_BARE_METAL
    if (addr + (uint32_t)bytes > MIPS_STACK_SIZE * 4) bytes = MIPS_STACK_SIZE * 4 - addr;
    for (int i = 0; i < bytes; i += 16) {
        fprintf(stderr, "  0x%08x:", addr + i);
        for (int j = 0; j < 16 && i+j < bytes; j++) {
            uint32_t w = vm->stack[(addr + i + j) / 4];
            int bo = (addr + i + j) % 4;
            fprintf(stderr, " %02x", (w >> (bo*8)) & 0xFF);
        }
        fprintf(stderr, "\n");
    }
#else
    (void)vm; (void)addr; (void)bytes;
#endif
}

void mips_dbg_show_instr(MipsVM* vm) {
#ifndef SAGE_BARE_METAL
    if (vm->pc + 4 <= vm->code_length) {
        char buf[128];
        uint32_t raw = ((uint32_t)vm->code[vm->pc]<<24) | ((uint32_t)vm->code[vm->pc+1]<<16) |
                       ((uint32_t)vm->code[vm->pc+2]<<8) | (uint32_t)vm->code[vm->pc+3];
        mips_disasm(buf, sizeof(buf), raw, vm->pc);
        fprintf(stderr, "%s\n", buf);
    }
#else
    (void)vm;
#endif
}

// Interactive debugger loop
int mips_dbg_interactive(MipsVM* vm, MipsDebugger* dbg) {
#ifndef SAGE_BARE_METAL
    char line[256];
    dbg->running = 1;

    fprintf(stderr, "SageMips Debugger. Type 'h' for help.\n");
    mips_dbg_show_instr(vm);

    while (dbg->running) {
        fprintf(stderr, "(sagemips-dbg) ");
        fflush(stderr);
        if (!fgets(line, sizeof(line), stdin)) break;

        // Trim newline
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (line[0] == 'q' || strcmp(line, "quit") == 0) {
            break;
        }
        else if (line[0] == 'h') {
            fprintf(stderr, "Commands: b <addr>, r, s [n], i r|f|s, x <addr>, c, q\n");
        }
        else if (line[0] == 'b' && line[1] == ' ') {
            uint32_t addr;
            if (sscanf(line+2, "%x", &addr) == 1) {
                mips_dbg_add_bp(dbg, addr);
                fprintf(stderr, "Breakpoint set at 0x%08x\n", addr);
            }
        }
        else if (line[0] == 'r') {
            dbg->should_break = 0;
            return 2; // signal: run until break
        }
        else if (line[0] == 's') {
            int n = 1;
            sscanf(line+1, "%d", &n);
            dbg->stepping = 1;
            dbg->step_count = n;
            return 2; // signal: step
        }
        else if (strcmp(line, "c") == 0) {
            dbg->should_break = 0;
            return 2;
        }
        else if (line[0] == 'i' && line[1] == ' ') {
            if (line[2] == 'r') mips_dbg_dump_regs(vm);
            else if (line[2] == 's') mips_dbg_dump_stack(vm, 16);
        }
        else if (line[0] == 'x' && line[1] == ' ') {
            uint32_t addr;
            if (sscanf(line+2, "%x", &addr) == 1) mips_dbg_dump_mem(vm, addr, 64);
        }
        else if (strlen(line) > 0) {
            fprintf(stderr, "Unknown command: %s\n", line);
        }
    }
#endif
    (void)vm; (void)dbg;
    return 0;
}
