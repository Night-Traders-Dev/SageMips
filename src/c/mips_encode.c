#include "sagemips.h"

const char* mips_reg_names[32] = {
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

// ============================================================================
// MIPS32 Instruction Decoder
// ============================================================================

MipsInstr mips_decode(uint32_t raw) {
    MipsInstr i;
    i.raw    = raw;
    i.opcode = (raw >> 26) & 0x3F;
    i.rs     = (raw >> 21) & 0x1F;
    i.rt     = (raw >> 16) & 0x1F;
    i.rd     = (raw >> 11) & 0x1F;
    i.shamt  = (raw >> 6)  & 0x1F;
    i.funct  = raw & 0x3F;

    // 16-bit immediate (sign-extended)
    uint16_t raw_imm = raw & 0xFFFF;
    i.uimm = raw_imm;
    i.imm  = (int16_t)raw_imm;

    // 26-bit jump target
    i.target = raw & 0x03FFFFFF;

    return i;
}

// ============================================================================
// MIPS32 Instruction Encoder
// ============================================================================

// R-type: opcode=0 (SPECIAL), funct determines operation
uint32_t mips_encode_r(uint8_t funct, uint8_t rs, uint8_t rt, uint8_t rd, uint8_t shamt) {
    uint32_t instr = 0;
    instr |= (MIPS_OP_SPECIAL & 0x3F) << 26;
    instr |= (rs    & 0x1F) << 21;
    instr |= (rt    & 0x1F) << 16;
    instr |= (rd    & 0x1F) << 11;
    instr |= (shamt & 0x1F) << 6;
    instr |= (funct & 0x3F);
    return instr;
}

// SPECIAL2 R-type: opcode=0x1C, funct determines operation
uint32_t mips_encode_r_special2(uint8_t funct, uint8_t rs, uint8_t rt, uint8_t rd) {
    uint32_t instr = 0;
    instr |= (MIPS_OP_SPECIAL2 & 0x3F) << 26;
    instr |= (rs    & 0x1F) << 21;
    instr |= (rt    & 0x1F) << 16;
    instr |= (rd    & 0x1F) << 11;
    instr |= (funct & 0x3F);
    return instr;
}

// I-type: opcode varies, 16-bit immediate
uint32_t mips_encode_i(uint8_t opcode, uint8_t rs, uint8_t rt, int16_t imm) {
    uint32_t instr = 0;
    instr |= (opcode & 0x3F) << 26;
    instr |= (rs     & 0x1F) << 21;
    instr |= (rt     & 0x1F) << 16;
    instr |= (imm & 0xFFFF);
    return instr;
}

// J-type: opcode varies, 26-bit target
uint32_t mips_encode_j(uint8_t opcode, uint32_t target) {
    uint32_t instr = 0;
    instr |= (opcode & 0x3F) << 26;
    instr |= (target & 0x03FFFFFF);
    return instr;
}

// REGIMM: opcode=0x01, rt=condition
uint32_t mips_encode_regimm(uint8_t rt, uint8_t rs, int16_t imm) {
    return mips_encode_i(MIPS_OP_REGIMM, rs, rt, imm);
}

// ============================================================================
// Register Name <-> Number Mapping
// ============================================================================

int mips_reg_from_name(const char* name) {
    static const char* names[] = {
        "zero","at","v0","v1","a0","a1","a2","a3",
        "t0","t1","t2","t3","t4","t5","t6","t7",
        "s0","s1","s2","s3","s4","s5","s6","s7",
        "t8","t9","k0","k1","gp","sp","fp","ra"
    };
    // Allow $ prefix
    if (name[0] == '$') name++;
    // Allow numeric
    if (name[0] >= '0' && name[0] <= '9') {
        int n = 0;
        const char* p = name;
        while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; }
        if (*p == '\0' && n >= 0 && n <= 31) return n;
    }
    for (int i = 0; i < 32; i++) {
        // Manual strcmp to avoid libc dependency
        const char* a = name;
        const char* b = names[i];
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') return i;
    }
    return -1;
}

const char* mips_reg_name(int reg) {
    if (reg < 0 || reg > 31) return "?";
    return mips_reg_names[reg];
}

// ============================================================================
// Disassembler
// ============================================================================

// Helper: write signed int to string buffer
static int append_int(char* buf, int pos, int val) {
    if (val < 0) { buf[pos++] = '-'; val = -val; }
    int digits[12], nd = 0;
    if (val == 0) digits[nd++] = 0;
    else { while (val) { digits[nd++] = val % 10; val /= 10; } }
    while (nd--) buf[pos++] = '0' + digits[nd];
    return pos;
}

// Helper: write hex to string buffer
static int append_hex(char* buf, int pos, uint32_t val, int width) {
    buf[pos++] = '0';
    buf[pos++] = 'x';
    int leading = 1;
    for (int s = 28; s >= 0; s -= 4) {
        int nybble = (val >> s) & 0xF;
        if (width && s > (width-1)*4 && leading) continue;
        leading = 0;
        buf[pos++] = nybble < 10 ? '0' + nybble : 'a' + (nybble - 10);
    }
    if (leading) buf[pos++] = '0';
    return pos;
}

// Helper: append string
static int append_str(char* buf, int pos, const char* s) {
    while (*s) buf[pos++] = *s++;
    return pos;
}

// Helper: append register name
static int append_reg(char* buf, int pos, int reg) {
    buf[pos++] = '$';
    const char* name = mips_reg_name(reg);
    return append_str(buf, pos, name);
}

// Helper: get R-type instruction mnemonic
static const char* get_r_mnemonic(uint8_t funct) {
    switch (funct) {
        case MIPS_FN_ADD:   return "add";
        case MIPS_FN_ADDU:  return "addu";
        case MIPS_FN_SUB:   return "sub";
        case MIPS_FN_SUBU:  return "subu";
        case MIPS_FN_AND:   return "and";
        case MIPS_FN_OR:    return "or";
        case MIPS_FN_XOR:   return "xor";
        case MIPS_FN_NOR:   return "nor";
        case MIPS_FN_SLT:   return "slt";
        case MIPS_FN_SLTU:  return "sltu";
        case MIPS_FN_SLL:   return "sll";
        case MIPS_FN_SRL:   return "srl";
        case MIPS_FN_SRA:   return "sra";
        case MIPS_FN_SLLV:  return "sllv";
        case MIPS_FN_SRLV:  return "srlv";
        case MIPS_FN_SRAV:  return "srav";
        case MIPS_FN_JR:    return "jr";
        case MIPS_FN_JALR:  return "jalr";
        case MIPS_FN_MFHI:  return "mfhi";
        case MIPS_FN_MTHI:  return "mthi";
        case MIPS_FN_MFLO:  return "mflo";
        case MIPS_FN_MTLO:  return "mtlo";
        case MIPS_FN_MULT:  return "mult";
        case MIPS_FN_MULTU: return "multu";
        case MIPS_FN_DIV:   return "div";
        case MIPS_FN_DIVU:  return "divu";
        case MIPS_FN_SYSCALL: return "syscall";
        case MIPS_FN_BREAK: return "break";
        case MIPS_FN_MOVZ:  return "movz";
        case MIPS_FN_MOVN:  return "movn";
        case MIPS_FN_TGE:   return "tge";
        case MIPS_FN_TGEU:  return "tgeu";
        case MIPS_FN_TLT:   return "tlt";
        case MIPS_FN_TLTU:  return "tltu";
        case MIPS_FN_TEQ:   return "teq";
        case MIPS_FN_TNE:   return "tne";
        default: return NULL;
    }
}

// Helper: get I-type instruction mnemonic
static const char* get_i_mnemonic(uint8_t opcode) {
    switch (opcode) {
        case MIPS_OP_ADDI:   return "addi";
        case MIPS_OP_ADDIU:  return "addiu";
        case MIPS_OP_SLTI:   return "slti";
        case MIPS_OP_SLTIU:  return "sltiu";
        case MIPS_OP_ANDI:   return "andi";
        case MIPS_OP_ORI:    return "ori";
        case MIPS_OP_XORI:   return "xori";
        case MIPS_OP_LUI:    return "lui";
        case MIPS_OP_LW:     return "lw";
        case MIPS_OP_SW:     return "sw";
        case MIPS_OP_LB:     return "lb";
        case MIPS_OP_LBU:    return "lbu";
        case MIPS_OP_LH:     return "lh";
        case MIPS_OP_LHU:    return "lhu";
        case MIPS_OP_SB:     return "sb";
        case MIPS_OP_SH:     return "sh";
        case MIPS_OP_BEQ:    return "beq";
        case MIPS_OP_BNE:    return "bne";
        case MIPS_OP_BLEZ:   return "blez";
        case MIPS_OP_BGTZ:   return "bgtz";
        default: return NULL;
    }
}

int mips_disasm(char* buf, int buf_sz, uint32_t raw, uint32_t addr) {
    if (!buf || buf_sz <= 0) return 0;
    MipsInstr i = mips_decode(raw);
    int pos = 0;

    // Address
    pos = append_hex(buf, pos, addr, 8);
    pos = append_str(buf, pos, "  ");
    // Raw bytes
    pos = append_hex(buf, pos, raw, 8);
    pos = append_str(buf, pos, "  ");

    if (i.opcode == MIPS_OP_SPECIAL) {
        const char* mn = get_r_mnemonic(i.funct);
        if (!mn) { pos = append_str(buf, pos, "???"); return pos; }

        if (i.funct == MIPS_FN_SLL || i.funct == MIPS_FN_SRL || i.funct == MIPS_FN_SRA) {
            pos = append_str(buf, pos, mn);
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rd);
            buf[pos++] = ',';
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rt);
            buf[pos++] = ',';
            buf[pos++] = ' ';
            pos = append_int(buf, pos, i.shamt);
        } else if (i.funct == MIPS_FN_JR || i.funct == MIPS_FN_MFHI || i.funct == MIPS_FN_MFLO) {
            pos = append_str(buf, pos, mn);
            buf[pos++] = ' ';
            if (i.funct == MIPS_FN_JR) pos = append_reg(buf, pos, i.rs);
            else if (i.funct == MIPS_FN_MFHI) pos = append_reg(buf, pos, i.rd);
            else pos = append_reg(buf, pos, i.rd);
        } else if (i.funct == MIPS_FN_MTHI || i.funct == MIPS_FN_MTLO) {
            pos = append_str(buf, pos, mn);
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rs);
        } else if (i.funct == MIPS_FN_JALR) {
            pos = append_str(buf, pos, mn);
            if (i.rd != 31) {
                buf[pos++] = ' ';
                pos = append_reg(buf, pos, i.rd);
            }
            buf[pos++] = ',';
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rs);
        } else if (i.funct == MIPS_FN_MULT || i.funct == MIPS_FN_MULTU ||
                   i.funct == MIPS_FN_DIV  || i.funct == MIPS_FN_DIVU) {
            pos = append_str(buf, pos, mn);
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rs);
            buf[pos++] = ',';
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rt);
        } else if (i.funct == MIPS_FN_SYSCALL || i.funct == MIPS_FN_BREAK) {
            pos = append_str(buf, pos, mn);
        } else if (i.funct == MIPS_FN_MOVZ || i.funct == MIPS_FN_MOVN) {
            pos = append_str(buf, pos, mn);
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rd);
            buf[pos++] = ',';
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rs);
            buf[pos++] = ',';
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rt);
        } else if (i.funct >= MIPS_FN_TGE && i.funct <= MIPS_FN_TNE) {
            // Trap instructions: rs, rt
            pos = append_str(buf, pos, mn);
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rs);
            buf[pos++] = ',';
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rt);
        } else {
            // Standard R-type: rd, rs, rt
            pos = append_str(buf, pos, mn);
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rd);
            buf[pos++] = ',';
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rs);
            buf[pos++] = ',';
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rt);
        }
    } else if (i.opcode == MIPS_OP_SPECIAL2) {
        if (i.funct == MIPS_FN_MUL) {
            pos = append_str(buf, pos, "mul");
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rd);
            buf[pos++] = ',';
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rs);
            buf[pos++] = ',';
            buf[pos++] = ' ';
            pos = append_reg(buf, pos, i.rt);
        } else {
            pos = append_str(buf, pos, "???");
        }
    } else if (i.opcode == MIPS_OP_REGIMM) {
        switch (i.rt) {
            case MIPS_RT_BLTZ:  pos = append_str(buf, pos, "bltz"); break;
            case MIPS_RT_BGEZ:  pos = append_str(buf, pos, "bgez"); break;
            case MIPS_RT_BLTZL: pos = append_str(buf, pos, "bltzl"); break;
            case MIPS_RT_BGEZL: pos = append_str(buf, pos, "bgezl"); break;
            case MIPS_RT_BLTZAL: pos = append_str(buf, pos, "bltzal"); break;
            case MIPS_RT_BGEZAL: pos = append_str(buf, pos, "bgezal"); break;
            default: pos = append_str(buf, pos, "???"); break;
        }
        buf[pos++] = ' ';
        pos = append_reg(buf, pos, i.rs);
        buf[pos++] = ',';
        buf[pos++] = ' ';
        int target = addr + 4 + ((int32_t)i.imm << 2);
        pos = append_hex(buf, pos, (uint32_t)target, 8);
    } else if (i.opcode == MIPS_OP_J || i.opcode == MIPS_OP_JAL) {
        pos = append_str(buf, pos, i.opcode == MIPS_OP_J ? "j" : "jal");
        buf[pos++] = ' ';
        uint32_t jtarget = (addr & 0xF0000000) | (i.target << 2);
        pos = append_hex(buf, pos, jtarget, 8);
    } else if (i.opcode == MIPS_OP_BEQ || i.opcode == MIPS_OP_BNE) {
        pos = append_str(buf, pos, i.opcode == MIPS_OP_BEQ ? "beq" : "bne");
        buf[pos++] = ' ';
        pos = append_reg(buf, pos, i.rs);
        buf[pos++] = ',';
        buf[pos++] = ' ';
        pos = append_reg(buf, pos, i.rt);
        buf[pos++] = ',';
        buf[pos++] = ' ';
        int btarget = addr + 4 + ((int32_t)i.imm << 2);
        pos = append_hex(buf, pos, (uint32_t)btarget, 8);
    } else if (i.opcode == MIPS_OP_BLEZ || i.opcode == MIPS_OP_BGTZ) {
        pos = append_str(buf, pos, i.opcode == MIPS_OP_BLEZ ? "blez" : "bgtz");
        buf[pos++] = ' ';
        pos = append_reg(buf, pos, i.rs);
        buf[pos++] = ',';
        buf[pos++] = ' ';
        int btarget = addr + 4 + ((int32_t)i.imm << 2);
        pos = append_hex(buf, pos, (uint32_t)btarget, 8);
    } else if (i.opcode == MIPS_OP_LUI) {
        pos = append_str(buf, pos, "lui");
        buf[pos++] = ' ';
        pos = append_reg(buf, pos, i.rt);
        buf[pos++] = ',';
        buf[pos++] = ' ';
        pos = append_hex(buf, pos, i.uimm, 4);
    } else if (i.opcode == MIPS_OP_LW || i.opcode == MIPS_OP_SW ||
               i.opcode == MIPS_OP_LB || i.opcode == MIPS_OP_LBU ||
               i.opcode == MIPS_OP_LH || i.opcode == MIPS_OP_LHU ||
               i.opcode == MIPS_OP_SB || i.opcode == MIPS_OP_SH) {
        const char* mn = get_i_mnemonic(i.opcode);
        pos = append_str(buf, pos, mn ? mn : "???");
        buf[pos++] = ' ';
        pos = append_reg(buf, pos, i.rt);
        buf[pos++] = ',';
        buf[pos++] = ' ';
        pos = append_int(buf, pos, i.imm);
        buf[pos++] = '(';
        pos = append_reg(buf, pos, i.rs);
        buf[pos++] = ')';
    } else if (i.opcode == MIPS_OP_ADDI || i.opcode == MIPS_OP_ADDIU ||
               i.opcode == MIPS_OP_SLTI || i.opcode == MIPS_OP_SLTIU ||
               i.opcode == MIPS_OP_ANDI || i.opcode == MIPS_OP_ORI ||
               i.opcode == MIPS_OP_XORI) {
        const char* mn = get_i_mnemonic(i.opcode);
        pos = append_str(buf, pos, mn ? mn : "???");
        buf[pos++] = ' ';
        pos = append_reg(buf, pos, i.rt);
        buf[pos++] = ',';
        buf[pos++] = ' ';
        pos = append_reg(buf, pos, i.rs);
        buf[pos++] = ',';
        buf[pos++] = ' ';
        pos = append_int(buf, pos, i.imm);
    } else {
        pos = append_str(buf, pos, "???");
    }

    buf[pos] = '\0';
    return pos;
}
