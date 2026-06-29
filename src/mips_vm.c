#include "sagemips.h"

// ============================================================================
// SageMips VM — Freestanding MIPS32 Interpreter
// ============================================================================
// Executes MIPS32 big-endian binaries using fixed-size static pools.
// No OS, no libc, no malloc required (when SAGE_BARE_METAL is defined).
//
// Registers:
//   $0  = zero (hardwired)
//   $1  = at   (assembler temporary — available)
//   $2-3= v0-v1 (return values)
//   $4-7= a0-a3 (function arguments)
//   $8-15, 24-25 = t0-t9 (temporaries)
//   $16-23 = s0-s7 (saved)
//   $26-27 = k0-k1 (kernel — available)
//   $28 = gp (global pointer)
//   $29 = sp (stack pointer)
//   $30 = fp (frame pointer)
//   $31 = ra (return address)
//
// Syscall convention: $v0 = syscall number, $a0-$a3 = args
// ============================================================================

// Helper: make a nil value
static MipsValue mv_nil(void) {
    MipsValue v; v.type = VAL_NIL; v.as.raw = 0; return v;
}

// Helper: make an int value
static MipsValue mv_int(int32_t val) {
    MipsValue v; v.type = VAL_INT; v.as.i32 = val; return v;
}

// Helper: make a uint value
static MipsValue mv_uint(uint32_t val) {
    MipsValue v; v.type = VAL_UINT; v.as.u32 = val; return v;
}

// Helper: get as int32
static int32_t mips_signed(MipsValue v) {
    switch (v.type) {
        case VAL_INT:  return v.as.i32;
        case VAL_UINT: return (int32_t)v.as.u32;
        case VAL_NUM:  return (int32_t)v.as.number;
        default: return 0;
    }
}

// Helper: get as uint32
static uint32_t mips_unsigned(MipsValue v) {
    switch (v.type) {
        case VAL_INT:  return (uint32_t)v.as.i32;
        case VAL_UINT: return v.as.u32;
        case VAL_NUM:  return (uint32_t)(int32_t)v.as.number;
        default: return 0;
    }
}

// ============================================================================
// String Pool
// ============================================================================

static int string_intern(MipsVM* vm, const char* s) {
    // Search existing
    int pos = 0;
    while (pos < vm->string_used) {
        const char* existing = &vm->strings[pos];
        const char* a = s;
        const char* b = existing;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') return pos;
        while (vm->strings[pos]) pos++;
        pos++; // skip null
    }
    // Append new
    int len = 0;
    while (s[len]) len++;
    if (vm->string_used + len + 1 > MIPS_STRING_POOL) return -1;
    int idx = vm->string_used;
    for (int i = 0; i <= len; i++)
        vm->strings[idx + i] = s[i];
    vm->string_used += len + 1;
    return idx;
}

static const char* string_get(MipsVM* vm, int idx) {
    if (idx < 0 || idx >= vm->string_used) return "";
    return &vm->strings[idx];
}

// ============================================================================
// Read 32-bit big-endian word from byte buffer
// ============================================================================

static uint32_t read_be32(const uint8_t* buf, uint32_t addr) {
    return ((uint32_t)buf[addr] << 24) |
           ((uint32_t)buf[addr+1] << 16) |
           ((uint32_t)buf[addr+2] << 8) |
           (uint32_t)buf[addr+3];
}

// ============================================================================
// VM Create / Destroy (for hosted platforms)
// ============================================================================

MipsVM* mips_vm_create(void) {
#ifndef SAGE_BARE_METAL
    MipsVM* vm = (MipsVM*)malloc(sizeof(MipsVM));
    if (!vm) return NULL;
    vm->stack = (uint32_t*)malloc(MIPS_STACK_SIZE * sizeof(uint32_t));
    vm->heap  = (uint8_t*)malloc(MIPS_HEAP_SIZE);
    vm->strings = (char*)malloc(MIPS_STRING_POOL);
    if (!vm->stack || !vm->heap || !vm->strings) {
        free(vm->stack); free(vm->heap); free(vm->strings); free(vm);
        return NULL;
    }
    vm->owns_memory = 1;
    mips_vm_init(vm);
    return vm;
#else
    // Bare metal: caller provides static buffers
    return NULL;
#endif
}

void mips_vm_destroy(MipsVM* vm) {
#ifndef SAGE_BARE_METAL
    if (vm && vm->owns_memory) {
        free(vm->stack);
        free(vm->heap);
        free(vm->strings);
        free(vm);
    }
#else
    (void)vm;
#endif
}

// ============================================================================
// VM Init & Load
// ============================================================================

void mips_vm_init(MipsVM* vm) {
    for (int i = 0; i < MIPS_NUM_REGS; i++) {
        vm->regs[i] = mv_int(0);
    }
    vm->pc = 0;
    vm->running = 0;
    vm->halted = 0;
    vm->error = 0;
    vm->hi = 0;
    vm->lo = 0;
    vm->code = NULL;
    vm->code_length = 0;

    // Stack: grows down from top
    if (vm->stack) {
        for (uint32_t i = 0; i < MIPS_STACK_SIZE; i++) vm->stack[i] = 0;
    }
    vm->sp = MIPS_STACK_SIZE;  // Points one past the end (stack is empty)
    vm->regs[29] = mv_uint(MIPS_STACK_SIZE * 4);  // $sp in bytes

    if (vm->heap) {
        vm->heap_used = 0;
    }
    if (vm->strings) {
        vm->string_used = 0;
    }
    vm->csp = 0;
    vm->debug = 0;
    vm->trace = 0;
    vm->error_msg = NULL;
    vm->write_char = NULL;
    vm->read_char = NULL;
    vm->write_str = NULL;
}

int mips_vm_load(MipsVM* vm, const uint8_t* code, uint32_t len) {
    if (!code || len == 0) return -1;
    if (len > MIPS_CODE_MAX) return -2;
    vm->code = code;
    vm->code_length = len;
    vm->pc = 0;
    return 0;
}

// ============================================================================
// Helper: stack push/pop (stack grows down)
// ============================================================================

static void vm_push(MipsVM* vm, uint32_t val) {
    if (vm->sp == 0) {
        vm->error = 1;
        vm->error_msg = "Stack overflow";
        return;
    }
    vm->sp--;
    vm->stack[vm->sp] = val;
    vm->regs[29] = mv_uint(vm->sp * 4);  // Update $sp
}

static uint32_t vm_pop(MipsVM* vm) {
    if (vm->sp >= MIPS_STACK_SIZE) {
        vm->error = 1;
        vm->error_msg = "Stack underflow";
        return 0;
    }
    uint32_t v = vm->stack[vm->sp];
    vm->sp++;
    vm->regs[29] = mv_uint(vm->sp * 4);  // Update $sp
    return v;
}

// ============================================================================
// Syscall Handler
// ============================================================================

static void handle_syscall(MipsVM* vm) {
    int32_t call = mips_signed(vm->regs[2]);  // $v0
    int32_t a0 = mips_signed(vm->regs[4]);
    int32_t a1 = mips_signed(vm->regs[5]);
    int32_t a2 = mips_signed(vm->regs[6]);

    switch (call) {
        case MIPS_SYSCALL_HALT:
        case MIPS_SYSCALL_EXIT:
            vm->halted = 1;
            vm->running = 0;
            vm->regs[2] = mv_int(0);
            break;

        case MIPS_SYSCALL_PRINT_INT:
            if (vm->write_str) {
                char buf[32];
                int pos = 0, val = a0;
                if (val < 0) { buf[pos++] = '-'; val = -val; }
                int digits[12], nd = 0;
                if (val == 0) digits[nd++] = 0;
                else { while (val) { digits[nd++] = val % 10; val /= 10; } }
                while (nd--) buf[pos++] = '0' + digits[nd];
                buf[pos] = '\0';
                vm->write_str(buf);
            }
            break;

        case MIPS_SYSCALL_PRINT_STR: {
            // a0 = address of null-terminated string in code/stack space
            const char* s = NULL;
            uint32_t addr = (uint32_t)a0;
            if (addr < MIPS_STACK_SIZE * 4) {
                // Stack-relative — but we don't have a direct mapping
                // Assume it's in the code segment
            }
            // Try code segment
            if (addr < vm->code_length) {
                s = (const char*)&vm->code[addr];
            }
            if (s && vm->write_str) vm->write_str(s);
            break;
        }

        case MIPS_SYSCALL_PRINT_CHAR:
            if (vm->write_char) vm->write_char((char)a0);
            break;

        case MIPS_SYSCALL_READ_INT:
            vm->regs[2] = mv_int(0); // stub
            break;

        case MIPS_SYSCALL_READ_STR:
            vm->regs[2] = mv_int(0); // stub
            break;

        case MIPS_SYSCALL_SBRK:
            // Simple bump allocator
            if (vm->heap_used + a0 > MIPS_HEAP_SIZE) {
                vm->regs[2] = mv_int(-1);
            } else {
                vm->regs[2] = mv_uint(vm->heap_used);
                vm->heap_used += a0;
            }
            break;

        case MIPS_SYSCALL_TIME:
            vm->regs[2] = mv_int(0); // stub
            break;

        default:
            vm->error = 1;
            vm->error_msg = "Unknown syscall";
            vm->running = 0;
            break;
    }
}

// ============================================================================
// Single Instruction Step
// ============================================================================

int mips_vm_step(MipsVM* vm) {
    if (vm->halted || vm->error || !vm->running) return 0;
    if (vm->pc + 4 > vm->code_length) {
        vm->halted = 1;
        vm->running = 0;
        return 0;
    }

    // Fetch 32-bit big-endian instruction
    uint32_t raw = read_be32(vm->code, vm->pc);
    MipsInstr instr = mips_decode(raw);

    uint32_t next_pc = vm->pc + 4;  // Default next PC

    // Hardwire $zero
    vm->regs[0] = mv_int(0);

    if (vm->trace) {
#ifndef SAGE_BARE_METAL
        char buf[128];
        mips_disasm(buf, sizeof(buf), raw, vm->pc);
        fprintf(stderr, "%s\n", buf);
#else
        if (vm->write_str) {
            char buf[128];
            mips_disasm(buf, sizeof(buf), raw, vm->pc);
            vm->write_str(buf);
            if (vm->write_char) vm->write_char('\n');
        }
#endif
    }

    switch (instr.opcode) {
    // ========================================================================
    // SPECIAL (R-type)
    // ========================================================================
    case MIPS_OP_SPECIAL: {
        int32_t rs_val = mips_signed(vm->regs[instr.rs]);
        int32_t rt_val = mips_signed(vm->regs[instr.rt]);
        uint32_t urs = mips_unsigned(vm->regs[instr.rs]);
        uint32_t urt = mips_unsigned(vm->regs[instr.rt]);

        switch (instr.funct) {
            case MIPS_FN_ADD:
                vm->regs[instr.rd] = mv_int(rs_val + rt_val);
                break;
            case MIPS_FN_ADDU:
                vm->regs[instr.rd] = mv_uint(urs + urt);
                break;
            case MIPS_FN_SUB:
                vm->regs[instr.rd] = mv_int(rs_val - rt_val);
                break;
            case MIPS_FN_SUBU:
                vm->regs[instr.rd] = mv_uint(urs - urt);
                break;
            case MIPS_FN_AND:
                vm->regs[instr.rd] = mv_uint(urs & urt);
                break;
            case MIPS_FN_OR:
                vm->regs[instr.rd] = mv_uint(urs | urt);
                break;
            case MIPS_FN_XOR:
                vm->regs[instr.rd] = mv_uint(urs ^ urt);
                break;
            case MIPS_FN_NOR:
                vm->regs[instr.rd] = mv_uint(~(urs | urt));
                break;
            case MIPS_FN_SLT:
                vm->regs[instr.rd] = mv_int(rs_val < rt_val ? 1 : 0);
                break;
            case MIPS_FN_SLTU:
                vm->regs[instr.rd] = mv_int(urs < urt ? 1 : 0);
                break;

            case MIPS_FN_SLL:
                vm->regs[instr.rd] = mv_uint(urt << instr.shamt);
                break;
            case MIPS_FN_SRL:
                vm->regs[instr.rd] = mv_uint(urt >> instr.shamt);
                break;
            case MIPS_FN_SRA:
                vm->regs[instr.rd] = mv_int(((int32_t)urt) >> instr.shamt);
                break;
            case MIPS_FN_SLLV:
                vm->regs[instr.rd] = mv_uint(urt << (urs & 0x1F));
                break;
            case MIPS_FN_SRLV:
                vm->regs[instr.rd] = mv_uint(urt >> (urs & 0x1F));
                break;
            case MIPS_FN_SRAV:
                vm->regs[instr.rd] = mv_int(((int32_t)urt) >> (urs & 0x1F));
                break;

            case MIPS_FN_JR:
                next_pc = mips_unsigned(vm->regs[instr.rs]);
                break;
            case MIPS_FN_JALR:
                vm->regs[instr.rd] = mv_uint(vm->pc + 8); // return addr
                next_pc = mips_unsigned(vm->regs[instr.rs]);
                break;

            case MIPS_FN_MFHI:
                vm->regs[instr.rd] = mv_int((int32_t)vm->hi);
                break;
            case MIPS_FN_MTHI:
                vm->hi = (int64_t)mips_signed(vm->regs[instr.rs]);
                break;
            case MIPS_FN_MFLO:
                vm->regs[instr.rd] = mv_int((int32_t)vm->lo);
                break;
            case MIPS_FN_MTLO:
                vm->lo = (int64_t)mips_signed(vm->regs[instr.rs]);
                break;

            case MIPS_FN_MULT:
                vm->lo = (int64_t)rs_val * (int64_t)rt_val;
                vm->hi = vm->lo >> 32;
                vm->lo = (int32_t)vm->lo;
                break;
            case MIPS_FN_MULTU: {
                uint64_t a = urs, b = urt;
                uint64_t prod = a * b;
                vm->lo = (int32_t)(prod & 0xFFFFFFFF);
                vm->hi = (int32_t)((prod >> 32) & 0xFFFFFFFF);
                break;
            }
            case MIPS_FN_DIV:
                if (rt_val == 0) {
                    vm->error = 1; vm->error_msg = "Division by zero";
                } else {
                    vm->lo = rs_val / rt_val;
                    vm->hi = rs_val % rt_val;
                }
                break;
            case MIPS_FN_DIVU:
                if (urt == 0) {
                    vm->error = 1; vm->error_msg = "Division by zero";
                } else {
                    vm->lo = (int32_t)(urs / urt);
                    vm->hi = (int32_t)(urs % urt);
                }
                break;

            case MIPS_FN_MOVZ:
                if (rt_val == 0) vm->regs[instr.rd] = vm->regs[instr.rs];
                break;
            case MIPS_FN_MOVN:
                if (rt_val != 0) vm->regs[instr.rd] = vm->regs[instr.rs];
                break;

            case MIPS_FN_SYSCALL:
                handle_syscall(vm);
                break;

            case MIPS_FN_BREAK:
                vm->halted = 1;
                vm->running = 0;
                break;

            case MIPS_FN_TGE:
                if (rs_val >= rt_val) { vm->error = 1; vm->error_msg = "Trap: tge"; }
                break;
            case MIPS_FN_TGEU:
                if (urs >= urt) { vm->error = 1; vm->error_msg = "Trap: tgeu"; }
                break;
            case MIPS_FN_TLT:
                if (rs_val < rt_val) { vm->error = 1; vm->error_msg = "Trap: tlt"; }
                break;
            case MIPS_FN_TLTU:
                if (urs < urt) { vm->error = 1; vm->error_msg = "Trap: tltu"; }
                break;
            case MIPS_FN_TEQ:
                if (rs_val == rt_val) { vm->error = 1; vm->error_msg = "Trap: teq"; }
                break;
            case MIPS_FN_TNE:
                if (rs_val != rt_val) { vm->error = 1; vm->error_msg = "Trap: tne"; }
                break;

            default:
                vm->error = 1;
                vm->error_msg = "Unknown SPECIAL instruction";
                break;
        }
        break;
    }

    // ========================================================================
    // SPECIAL2 (MUL, etc.)
    // ========================================================================
    case MIPS_OP_SPECIAL2: {
        int32_t rs_val = mips_signed(vm->regs[instr.rs]);
        int32_t rt_val = mips_signed(vm->regs[instr.rt]);
        switch (instr.funct) {
            case MIPS_FN_MUL:
                vm->regs[instr.rd] = mv_int(rs_val * rt_val);
                break;
            default:
                vm->error = 1;
                vm->error_msg = "Unknown SPECIAL2 instruction";
                break;
        }
        break;
    }

    // ========================================================================
    // REGIMM
    // ========================================================================
    case MIPS_OP_REGIMM: {
        int32_t rs_val = mips_signed(vm->regs[instr.rs]);
        int take = 0;
        switch (instr.rt) {
            case MIPS_RT_BLTZ:  take = (rs_val < 0); break;
            case MIPS_RT_BGEZ:  take = (rs_val >= 0); break;
            case MIPS_RT_BLTZL: take = (rs_val < 0); break; // likely
            case MIPS_RT_BGEZL: take = (rs_val >= 0); break;
            case MIPS_RT_BLTZAL:
                if (rs_val < 0) { vm->regs[31] = mv_uint(vm->pc + 8); take = 1; }
                break;
            case MIPS_RT_BGEZAL:
                if (rs_val >= 0) { vm->regs[31] = mv_uint(vm->pc + 8); take = 1; }
                break;
        }
        if (take) {
            next_pc = vm->pc + 4 + ((int32_t)instr.imm << 2);
        }
        break;
    }

    // ========================================================================
    // J / JAL
    // ========================================================================
    case MIPS_OP_J:
        next_pc = (vm->pc & 0xF0000000) | (instr.target << 2);
        break;
    case MIPS_OP_JAL:
        vm->regs[31] = mv_uint(vm->pc + 8);
        next_pc = (vm->pc & 0xF0000000) | (instr.target << 2);
        break;

    // ========================================================================
    // Branch Instructions
    // ========================================================================
    case MIPS_OP_BEQ: {
        int32_t rs_val = mips_signed(vm->regs[instr.rs]);
        int32_t rt_val = mips_signed(vm->regs[instr.rt]);
        if (rs_val == rt_val)
            next_pc = vm->pc + 4 + ((int32_t)instr.imm << 2);
        break;
    }
    case MIPS_OP_BNE: {
        int32_t rs_val = mips_signed(vm->regs[instr.rs]);
        int32_t rt_val = mips_signed(vm->regs[instr.rt]);
        if (rs_val != rt_val)
            next_pc = vm->pc + 4 + ((int32_t)instr.imm << 2);
        break;
    }
    case MIPS_OP_BLEZ: {
        int32_t rs_val = mips_signed(vm->regs[instr.rs]);
        if (rs_val <= 0)
            next_pc = vm->pc + 4 + ((int32_t)instr.imm << 2);
        break;
    }
    case MIPS_OP_BGTZ: {
        int32_t rs_val = mips_signed(vm->regs[instr.rs]);
        if (rs_val > 0)
            next_pc = vm->pc + 4 + ((int32_t)instr.imm << 2);
        break;
    }

    // ========================================================================
    // Arithmetic Immediate
    // ========================================================================
    case MIPS_OP_ADDI:
        vm->regs[instr.rt] = mv_int(mips_signed(vm->regs[instr.rs]) + (int32_t)instr.imm);
        break;
    case MIPS_OP_ADDIU:
        vm->regs[instr.rt] = mv_uint(mips_unsigned(vm->regs[instr.rs]) + instr.imm);
        break;
    case MIPS_OP_SLTI:
        vm->regs[instr.rt] = mv_int(mips_signed(vm->regs[instr.rs]) < (int32_t)instr.imm ? 1 : 0);
        break;
    case MIPS_OP_SLTIU:
        vm->regs[instr.rt] = mv_int(mips_unsigned(vm->regs[instr.rs]) < instr.uimm ? 1 : 0);
        break;
    case MIPS_OP_ANDI:
        vm->regs[instr.rt] = mv_uint(mips_unsigned(vm->regs[instr.rs]) & instr.uimm);
        break;
    case MIPS_OP_ORI:
        vm->regs[instr.rt] = mv_uint(mips_unsigned(vm->regs[instr.rs]) | instr.uimm);
        break;
    case MIPS_OP_XORI:
        vm->regs[instr.rt] = mv_uint(mips_unsigned(vm->regs[instr.rs]) ^ instr.uimm);
        break;
    case MIPS_OP_LUI:
        vm->regs[instr.rt] = mv_uint(((uint32_t)instr.imm) << 16);
        break;

    // ========================================================================
    // Load/Store
    // ========================================================================
    case MIPS_OP_LW: {
        int32_t base = mips_signed(vm->regs[instr.rs]);
        int32_t addr = base + (int32_t)instr.imm;
        if (addr < 0 || (uint32_t)(addr + 4) > MIPS_STACK_SIZE * 4) {
            vm->error = 1; vm->error_msg = "LW access violation";
        } else {
            uint32_t word_idx = (uint32_t)addr / 4;
            if (word_idx < MIPS_STACK_SIZE)
                vm->regs[instr.rt] = mv_uint(vm->stack[word_idx]);
            else
                vm->regs[instr.rt] = mv_int(0);
        }
        break;
    }
    case MIPS_OP_SW: {
        int32_t base = mips_signed(vm->regs[instr.rs]);
        int32_t addr = base + (int32_t)instr.imm;
        if (addr < 0 || (uint32_t)(addr + 4) > MIPS_STACK_SIZE * 4) {
            vm->error = 1; vm->error_msg = "SW access violation";
        } else {
            uint32_t word_idx = (uint32_t)addr / 4;
            if (word_idx < MIPS_STACK_SIZE)
                vm->stack[word_idx] = mips_unsigned(vm->regs[instr.rt]);
        }
        break;
    }
    case MIPS_OP_LB: {
        int32_t base = mips_signed(vm->regs[instr.rs]);
        int32_t addr = base + (int32_t)instr.imm;
        if (addr < 0 || (uint32_t)addr >= MIPS_STACK_SIZE * 4) {
            vm->error = 1; vm->error_msg = "LB access violation";
        } else {
            uint32_t byte_idx = (uint32_t)addr / 4;
            uint32_t byte_off = (uint32_t)addr % 4;
            uint8_t b = (uint8_t)(vm->stack[byte_idx] >> (byte_off * 8));
            vm->regs[instr.rt] = mv_int((int32_t)(int8_t)b);
        }
        break;
    }
    case MIPS_OP_LBU: {
        int32_t base = mips_signed(vm->regs[instr.rs]);
        int32_t addr = base + (int32_t)instr.imm;
        if (addr < 0 || (uint32_t)addr >= MIPS_STACK_SIZE * 4) {
            vm->error = 1; vm->error_msg = "LBU access violation";
        } else {
            uint32_t byte_idx = (uint32_t)addr / 4;
            uint32_t byte_off = (uint32_t)addr % 4;
            vm->regs[instr.rt] = mv_uint((vm->stack[byte_idx] >> (byte_off * 8)) & 0xFF);
        }
        break;
    }
    case MIPS_OP_LH: {
        int32_t base = mips_signed(vm->regs[instr.rs]);
        int32_t addr = base + (int32_t)instr.imm;
        if (addr < 0 || (uint32_t)(addr + 2) > MIPS_STACK_SIZE * 4 || (addr & 1)) {
            vm->error = 1; vm->error_msg = "LH access violation";
        } else {
            uint32_t word_idx = (uint32_t)addr / 4;
            uint32_t byte_off = (uint32_t)addr % 4;
            uint16_t h = (uint16_t)(vm->stack[word_idx] >> (byte_off * 8));
            vm->regs[instr.rt] = mv_int((int32_t)(int16_t)h);
        }
        break;
    }
    case MIPS_OP_LHU: {
        int32_t base = mips_signed(vm->regs[instr.rs]);
        int32_t addr = base + (int32_t)instr.imm;
        if (addr < 0 || (uint32_t)(addr + 2) > MIPS_STACK_SIZE * 4 || (addr & 1)) {
            vm->error = 1; vm->error_msg = "LHU access violation";
        } else {
            uint32_t word_idx = (uint32_t)addr / 4;
            uint32_t byte_off = (uint32_t)addr % 4;
            vm->regs[instr.rt] = mv_uint((vm->stack[word_idx] >> (byte_off * 8)) & 0xFFFF);
        }
        break;
    }
    case MIPS_OP_SB: {
        int32_t base = mips_signed(vm->regs[instr.rs]);
        int32_t addr = base + (int32_t)instr.imm;
        if (addr < 0 || (uint32_t)addr >= MIPS_STACK_SIZE * 4) {
            vm->error = 1; vm->error_msg = "SB access violation";
        } else {
            uint32_t byte_idx = (uint32_t)addr / 4;
            uint32_t byte_off = (uint32_t)addr % 4;
            uint32_t mask = ~(0xFFU << (byte_off * 8));
            uint8_t bval = (uint8_t)mips_unsigned(vm->regs[instr.rt]);
            vm->stack[byte_idx] = (vm->stack[byte_idx] & mask) | ((uint32_t)bval << (byte_off * 8));
        }
        break;
    }
    case MIPS_OP_SH: {
        int32_t base = mips_signed(vm->regs[instr.rs]);
        int32_t addr = base + (int32_t)instr.imm;
        if (addr < 0 || (uint32_t)(addr + 2) > MIPS_STACK_SIZE * 4 || (addr & 1)) {
            vm->error = 1; vm->error_msg = "SH access violation";
        } else {
            uint32_t word_idx = (uint32_t)addr / 4;
            uint32_t byte_off = (uint32_t)addr % 4;
            uint32_t mask = ~(0xFFFFU << (byte_off * 8));
            uint16_t hval = (uint16_t)mips_unsigned(vm->regs[instr.rt]);
            vm->stack[word_idx] = (vm->stack[word_idx] & mask) | ((uint32_t)hval << (byte_off * 8));
        }
        break;
    }

    // ========================================================================
    // Unhandled
    // ========================================================================
    default:
        vm->error = 1;
        vm->error_msg = "Unimplemented MIPS opcode";
        vm->running = 0;
        return 0;
    }

    // $zero stays zero
    vm->regs[0] = mv_int(0);
    vm->pc = next_pc;

    return vm->error ? 0 : 1;
}

// ============================================================================
// Run Loop
// ============================================================================

int mips_vm_run(MipsVM* vm) {
    vm->running = 1;
    vm->halted = 0;
    vm->error = 0;

    // Set up stack pointer (points to top of stack, stack grows down)
    vm->sp = MIPS_STACK_SIZE;
    vm->regs[29] = mv_uint(vm->sp * 4);

    while (vm->running && !vm->halted && !vm->error) {
        if (!mips_vm_step(vm)) break;
    }

    return vm->error ? -1 : 0;
}
