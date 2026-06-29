#include "sagemips.h"

// ============================================================================
// SageMips Assembler — MIPS32 Assembly Text → Machine Code
// ============================================================================
// Two-pass assembler:
//   Pass 1: collect labels, check syntax
//   Pass 2: emit machine code, resolve labels
//
// Supports:
//   - All standard MIPS32 instructions (R/I/J types)
//   - Pseudo-instructions: li, move, la, nop, not, neg, b
//   - Labels: name:
//   - Directives: .word, .byte, .ascii, .asciiz, .space, .align, .text, .data
//   - Comments: # to end of line
// ============================================================================

// Helpers
static int is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static int is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.'; }
static int is_alnum(char c) { return is_alpha(c) || (c >= '0' && c <= '9'); }
static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_xdigit(char c) { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static int to_lower(char c) { return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c; }

static void skip_spaces(const char** p) {
    while (is_space(**p)) (*p)++;
}

// Parse an integer from a C string (supports 0x hex prefixes)
static int parse_int_str(const char* s, int* ok) {
    const char* p = s;
    skip_spaces(&p);
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    if (*p == '0' && (*(p+1) == 'x' || *(p+1) == 'X')) {
        p += 2;
        unsigned val = 0;
        while (is_xdigit(*p)) {
            char c = to_lower(*p++);
            val = val * 16 + (c >= 'a' ? c - 'a' + 10 : c - '0');
        }
        *ok = 1; return neg ? -(int)val : (int)val;
    }
    int val = 0;
    if (!is_digit(*p)) { *ok = 0; return 0; }
    while (is_digit(*p)) { val = val * 10 + (*p++ - '0'); }
    *ok = 1; return neg ? -val : val;
}

// Parse an integer advancing a pointer
static int parse_int(const char** p, int* ok) {
    return parse_int_str(*p, ok);
}

static int parse_token(const char** p, char* buf, int sz) {
    skip_spaces(p);
    int i = 0;
    while (i < sz - 1 && !is_space(**p) && **p != ',' && **p != '#' && **p != '(' && **p != ')' && **p != ':' && **p != '\0') {
        buf[i++] = *(*p)++;
    }
    buf[i] = '\0';
    return i;
}

// Emit 4 bytes big-endian into output buffer
static void emit4(MipsAsmState* st, uint32_t val) {
    if (st->code_len + 4 <= MIPS_CODE_MAX) {
        st->code[st->code_len++] = (val >> 24) & 0xFF;
        st->code[st->code_len++] = (val >> 16) & 0xFF;
        st->code[st->code_len++] = (val >> 8) & 0xFF;
        st->code[st->code_len++] = val & 0xFF;
    }
}

// Find label address
static int find_label(MipsAsmState* st, const char* name) {
    for (int i = 0; i < st->label_count; i++) {
        const char* a = name;
        const char* b = st->labels[i].name;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') return (int)st->labels[i].addr;
    }
    return -1;
}

// Add/update a label
static void add_label(MipsAsmState* st, const char* name, uint32_t addr) {
    // Check if already exists
    for (int i = 0; i < st->label_count; i++) {
        const char* a = name;
        const char* b = st->labels[i].name;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') {
            st->labels[i].addr = addr;
            return;
        }
    }
    if (st->label_count < MIPS_LABELS_MAX) {
        const char* s = name;
        int j = 0;
        while (*s && j < 63) st->labels[st->label_count].name[j++] = *s++;
        st->labels[st->label_count].name[j] = '\0';
        st->labels[st->label_count].addr = addr;
        st->label_count++;
    }
}

// String comparison helper
static int str_eq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == '\0' && *b == '\0';
}

// ============================================================================
// Single Line Assembly (Pass 1: check only; Pass 2: emit)
// ============================================================================

int mips_asm_assemble_line(MipsAsmState* st, const char* line) {
    const char* p = line;

    // Skip leading whitespace
    skip_spaces(&p);
    if (*p == '\0' || *p == '#') return 0; // Empty line or comment

    // Check for label definition (name:)
    const char* q = p;
    while (is_alnum(*q)) q++;
    if (*q == ':' && !is_space(*(q-1 > p ? q-1 : p))) {
        // It's a label
        char label[64]; int i = 0;
        while (p < q && i < 63) label[i++] = *p++;
        label[i] = '\0';
        p++; // skip ':'
        add_label(st, label, st->code_len + st->origin);
        // Rest of line after label
        skip_spaces(&p);
        if (*p == '\0' || *p == '#') return 0;
        // Fall through to parse instruction after label
    }

    // Parse mnemonic
    char mnemonic[16];
    int mn_len = parse_token(&p, mnemonic, sizeof(mnemonic));
    if (mn_len == 0) return 0;

    // Convert to lowercase
    for (int i = 0; i < mn_len; i++) mnemonic[i] = to_lower(mnemonic[i]);
    mnemonic[mn_len] = '\0';

    // ========================================================================
    // Directives
    // ========================================================================
    if (mnemonic[0] == '.') {
        if (str_eq(mnemonic, ".word")) {
            int ok; int val = parse_int(&p, &ok);
            if (ok && st->pass == 1) emit4(st, (uint32_t)val);
            return ok ? 0 : -1;
        }
        if (str_eq(mnemonic, ".byte")) {
            int ok; int val = parse_int(&p, &ok);
            if (ok && st->pass == 1) {
                if (st->code_len < MIPS_CODE_MAX)
                    st->code[st->code_len++] = (uint8_t)val;
            }
            return ok ? 0 : -1;
        }
        if (str_eq(mnemonic, ".ascii")) {
            skip_spaces(&p);
            if (*p == '"') {
                p++; int count = 0;
                while (*p && *p != '"' && count < 4096) {
                    if (st->pass == 1 && st->code_len < MIPS_CODE_MAX)
                        st->code[st->code_len++] = (uint8_t)*p;
                    p++; count++;
                }
                if (*p == '"') p++;
            }
            return 0;
        }
        if (str_eq(mnemonic, ".asciiz")) {
            skip_spaces(&p);
            if (*p == '"') {
                p++; int count = 0;
                while (*p && *p != '"' && count < 4096) {
                    if (st->pass == 1 && st->code_len < MIPS_CODE_MAX)
                        st->code[st->code_len++] = (uint8_t)*p;
                    p++; count++;
                }
                if (*p == '"') p++;
                if (st->pass == 1 && st->code_len < MIPS_CODE_MAX)
                    st->code[st->code_len++] = 0;
            }
            return 0;
        }
        if (str_eq(mnemonic, ".space")) {
            int ok; int n = parse_int(&p, &ok);
            if (ok && st->pass == 1) {
                for (int i = 0; i < n && st->code_len < MIPS_CODE_MAX; i++)
                    st->code[st->code_len++] = 0;
            }
            return ok ? 0 : -1;
        }
        if (str_eq(mnemonic, ".align")) {
            // Skip — we handle alignment by zero-padding
        }
        if (str_eq(mnemonic, ".text") || str_eq(mnemonic, ".data") ||
            str_eq(mnemonic, ".globl") || str_eq(mnemonic, ".global")) {
            return 0; // Ignore section directives
        }
        return 0;
    }

    // ========================================================================
    // Parse operands into tokens
    // ========================================================================
    char tok1[64], tok2[64], tok3[64];
    tok1[0] = tok2[0] = tok3[0] = '\0';
    parse_token(&p, tok1, sizeof(tok1));
    skip_spaces(&p); if (*p == ',') p++;
    parse_token(&p, tok2, sizeof(tok2));
    skip_spaces(&p); if (*p == ',') p++;
    parse_token(&p, tok3, sizeof(tok3));

    // If nothing to emit (pass 0), skip
    if (st->pass == 0) {
        // Still need to account for instruction size
        st->code_len += 4;
        return 0;
    }

    // ========================================================================
    // Pseudo-instructions
    // ========================================================================
    if (str_eq(mnemonic, "nop")) {
        emit4(st, mips_encode_r(MIPS_FN_SLL, 0, 0, 0, 0)); // sll $0, $0, 0
        return 0;
    }
    if (str_eq(mnemonic, "move")) {
        int rd = mips_reg_from_name(tok1);
        int rs = mips_reg_from_name(tok2);
        if (rd < 0 || rs < 0) return -1;
        emit4(st, mips_encode_r(MIPS_FN_ADDU, rs, 0, rd, 0)); // addu rd, rs, $0
        return 0;
    }
    if (str_eq(mnemonic, "not")) {
        int rd = mips_reg_from_name(tok1);
        int rs = mips_reg_from_name(tok2);
        if (rd < 0 || rs < 0) return -1;
        emit4(st, mips_encode_r(MIPS_FN_NOR, rs, 0, rd, 0)); // nor rd, rs, $0
        return 0;
    }
    if (str_eq(mnemonic, "neg")) {
        int rd = mips_reg_from_name(tok1);
        int rs = mips_reg_from_name(tok2);
        if (rd < 0 || rs < 0) return -1;
        emit4(st, mips_encode_r(MIPS_FN_SUB, 0, rs, rd, 0)); // sub rd, $0, rs
        return 0;
    }
    if (str_eq(mnemonic, "b")) {
        int addr = find_label(st, tok1);
        if (addr < 0) return -1;
        int32_t offset = ((int32_t)addr - (int32_t)(st->code_len + st->origin + 4)) >> 2;
        emit4(st, mips_encode_i(MIPS_OP_BEQ, 0, 0, (int16_t)offset)); // beq $0, $0, label
        return 0;
    }
    if (str_eq(mnemonic, "li") || str_eq(mnemonic, "la")) {
        int rt = mips_reg_from_name(tok1);
        if (rt < 0) return -1;
        const char* ip = tok2[0] ? tok2 : (p);
        int ok, val = parse_int(&ip, &ok);
        if (!ok) {
            int lbl = find_label(st, tok2);
            if (lbl >= 0) val = lbl;
            else return -1;
        }
        // lui + ori sequence
        int upper = (val >> 16) & 0xFFFF;
        int lower = val & 0xFFFF;
        if (upper) emit4(st, mips_encode_i(MIPS_OP_LUI, 0, rt, (int16_t)upper));
        if (lower || !upper) emit4(st, mips_encode_i(MIPS_OP_ORI, rt, rt, (int16_t)lower));
        return 0;
    }

    // ========================================================================
    // R-type Instructions
    // ========================================================================
    #define R_3REG(fn) do { \
        int rd=mips_reg_from_name(tok1), rs=mips_reg_from_name(tok2), rt=mips_reg_from_name(tok3); \
        if (rd<0||rs<0||rt<0) return -1; \
        emit4(st, mips_encode_r(fn, rs, rt, rd, 0)); \
    } while(0)

    if (str_eq(mnemonic, "add"))  { R_3REG(MIPS_FN_ADD); return 0; }
    if (str_eq(mnemonic, "addu")) { R_3REG(MIPS_FN_ADDU); return 0; }
    if (str_eq(mnemonic, "sub"))  { R_3REG(MIPS_FN_SUB); return 0; }
    if (str_eq(mnemonic, "subu")) { R_3REG(MIPS_FN_SUBU); return 0; }
    if (str_eq(mnemonic, "and"))  { R_3REG(MIPS_FN_AND); return 0; }
    if (str_eq(mnemonic, "or"))   { R_3REG(MIPS_FN_OR); return 0; }
    if (str_eq(mnemonic, "xor"))  { R_3REG(MIPS_FN_XOR); return 0; }
    if (str_eq(mnemonic, "nor"))  { R_3REG(MIPS_FN_NOR); return 0; }
    if (str_eq(mnemonic, "slt"))  { R_3REG(MIPS_FN_SLT); return 0; }
    if (str_eq(mnemonic, "sltu")) { R_3REG(MIPS_FN_SLTU); return 0; }
    if (str_eq(mnemonic, "sllv")) { R_3REG(MIPS_FN_SLLV); return 0; }
    if (str_eq(mnemonic, "srlv")) { R_3REG(MIPS_FN_SRLV); return 0; }
    if (str_eq(mnemonic, "srav")) { R_3REG(MIPS_FN_SRAV); return 0; }
    if (str_eq(mnemonic, "mul"))  { emit4(st, mips_encode_r_special2(MIPS_FN_MUL, mips_reg_from_name(tok2), mips_reg_from_name(tok3), mips_reg_from_name(tok1))); return 0; }

    // Shift instructions (rd, rt, shamt)
    if (str_eq(mnemonic, "sll") || str_eq(mnemonic, "srl") || str_eq(mnemonic, "sra")) {
        int rd = mips_reg_from_name(tok1), rt = mips_reg_from_name(tok2);
        const char* sp = tok3[0] ? tok3 : p;
        int ok, shamt = parse_int(&sp, &ok);
        if (rd < 0 || rt < 0 || !ok) return -1;
        uint8_t fn = str_eq(mnemonic, "sll") ? MIPS_FN_SLL : str_eq(mnemonic, "srl") ? MIPS_FN_SRL : MIPS_FN_SRA;
        emit4(st, mips_encode_r(fn, 0, rt, rd, shamt & 0x1F));
        return 0;
    }

    // mult/div (rs, rt)
    if (str_eq(mnemonic, "mult") || str_eq(mnemonic, "multu") ||
        str_eq(mnemonic, "div")  || str_eq(mnemonic, "divu")) {
        int rs = mips_reg_from_name(tok1), rt = mips_reg_from_name(tok2);
        if (rs < 0 || rt < 0) return -1;
        uint8_t fn = str_eq(mnemonic, "mult") ? MIPS_FN_MULT : str_eq(mnemonic, "multu") ? MIPS_FN_MULTU :
                     str_eq(mnemonic, "div")  ? MIPS_FN_DIV  : MIPS_FN_DIVU;
        emit4(st, mips_encode_r(fn, rs, rt, 0, 0));
        return 0;
    }

    // mfhi, mflo, mthi, mtlo
    if (str_eq(mnemonic, "mfhi") || str_eq(mnemonic, "mflo")) {
        int rd = mips_reg_from_name(tok1);
        if (rd < 0) return -1;
        emit4(st, mips_encode_r(str_eq(mnemonic, "mfhi") ? MIPS_FN_MFHI : MIPS_FN_MFLO, 0, 0, rd, 0));
        return 0;
    }
    if (str_eq(mnemonic, "mthi") || str_eq(mnemonic, "mtlo")) {
        int rs = mips_reg_from_name(tok1);
        if (rs < 0) return -1;
        emit4(st, mips_encode_r(str_eq(mnemonic, "mthi") ? MIPS_FN_MTHI : MIPS_FN_MTLO, rs, 0, 0, 0));
        return 0;
    }

    // jr, jalr
    if (str_eq(mnemonic, "jr")) {
        int rs = mips_reg_from_name(tok1);
        if (rs < 0) return -1;
        emit4(st, mips_encode_r(MIPS_FN_JR, rs, 0, 0, 0));
        return 0;
    }
    if (str_eq(mnemonic, "jalr")) {
        int rd = tok1[0] ? mips_reg_from_name(tok1) : 31;
        int rs = mips_reg_from_name(tok2[0] ? tok2 : tok1);
        if (rs < 0) return -1;
        if (rd < 0) rd = 31;
        emit4(st, mips_encode_r(MIPS_FN_JALR, rs, 0, rd, 0));
        return 0;
    }

    // syscall
    if (str_eq(mnemonic, "syscall")) {
        emit4(st, mips_encode_r(MIPS_FN_SYSCALL, 0, 0, 0, 0));
        return 0;
    }

    // ========================================================================
    // I-type Arithmetic
    // ========================================================================
    #define I_ARI(op, fn) do { \
        int rt=mips_reg_from_name(tok1), rs=mips_reg_from_name(tok2); \
        const char* _ip = tok3[0] ? tok3 : p; \
        int ok, imm = parse_int(&_ip, &ok); \
        if (rt<0||rs<0||!ok) return -1; \
        emit4(st, mips_encode_i(op, rs, rt, (int16_t)imm)); \
    } while(0)

    if (str_eq(mnemonic, "addi"))  { I_ARI(MIPS_OP_ADDI,  MIPS_FN_ADD); return 0; }
    if (str_eq(mnemonic, "addiu")) { I_ARI(MIPS_OP_ADDIU, MIPS_FN_ADDU); return 0; }
    if (str_eq(mnemonic, "slti"))  { I_ARI(MIPS_OP_SLTI,  MIPS_FN_SLT); return 0; }
    if (str_eq(mnemonic, "sltiu")) { I_ARI(MIPS_OP_SLTIU, MIPS_FN_SLTU); return 0; }
    if (str_eq(mnemonic, "andi"))  { I_ARI(MIPS_OP_ANDI,  MIPS_FN_AND); return 0; }
    if (str_eq(mnemonic, "ori"))   { I_ARI(MIPS_OP_ORI,   MIPS_FN_OR); return 0; }
    if (str_eq(mnemonic, "xori"))  { I_ARI(MIPS_OP_XORI,  MIPS_FN_XOR); return 0; }

    // lui
    if (str_eq(mnemonic, "lui")) {
        int rt = mips_reg_from_name(tok1);
        const char* ip2 = tok2[0] ? tok2 : p;
        int ok, imm = parse_int(&ip2, &ok);
        if (rt < 0 || !ok) return -1;
        emit4(st, mips_encode_i(MIPS_OP_LUI, 0, rt, (int16_t)imm));
        return 0;
    }

    // ========================================================================
    // Load/Store
    // ========================================================================
    if (str_eq(mnemonic, "lw") || str_eq(mnemonic, "sw") ||
        str_eq(mnemonic, "lb") || str_eq(mnemonic, "lbu") ||
        str_eq(mnemonic, "lh") || str_eq(mnemonic, "lhu") ||
        str_eq(mnemonic, "sb") || str_eq(mnemonic, "sh")) {
        int rt = mips_reg_from_name(tok1);
        if (rt < 0) return -1;

        // Parse offset(base) syntax
        int imm = 0, rs = 0;
        // Try "offset(base)" or "offset" and base in tok3
        const char* paren = tok2;
        while (*paren && *paren != '(') paren++;
        if (*paren == '(') {
            // offset is before (
            char off_buf[16]; int oi = 0;
            const char* op = tok2;
            while (op < paren && oi < 15) off_buf[oi++] = *op++;
            off_buf[oi] = '\0';
            const char* rp = off_buf;
            int ok; imm = parse_int(&rp, &ok);
            if (!ok) return -1;
            // base is in (base)
            paren++;
            const char* ep = paren;
            while (*ep && *ep != ')') ep++;
            char base_buf[8]; int bi = 0;
            while (paren < ep && bi < 7) base_buf[bi++] = *paren++;
            base_buf[bi] = '\0';
            rs = mips_reg_from_name(base_buf);
        } else if (tok3[0]) {
            // imm in tok2, base in tok3
            int ok;
            const char* ip = tok2;
            imm = parse_int(&ip, &ok);
            if (!ok) return -1;
            rs = mips_reg_from_name(tok3);
        }
        if (rs < 0) return -1;

        uint8_t op = 0;
        if (str_eq(mnemonic, "lw")) op = MIPS_OP_LW;
        else if (str_eq(mnemonic, "sw")) op = MIPS_OP_SW;
        else if (str_eq(mnemonic, "lb")) op = MIPS_OP_LB;
        else if (str_eq(mnemonic, "lbu")) op = MIPS_OP_LBU;
        else if (str_eq(mnemonic, "lh")) op = MIPS_OP_LH;
        else if (str_eq(mnemonic, "lhu")) op = MIPS_OP_LHU;
        else if (str_eq(mnemonic, "sb")) op = MIPS_OP_SB;
        else if (str_eq(mnemonic, "sh")) op = MIPS_OP_SH;

        emit4(st, mips_encode_i(op, rs, rt, (int16_t)imm));
        return 0;
    }

    // ========================================================================
    // Branch Instructions
    // ========================================================================
    #define BRANCH(op) do { \
        int rs=mips_reg_from_name(tok1), rt=mips_reg_from_name(tok2); \
        int addr=find_label(st, tok3); \
        if (rs<0||rt<0||addr<0) return -1; \
        int32_t off=((int32_t)addr-(int32_t)(st->code_len+st->origin+4))>>2; \
        emit4(st, mips_encode_i(op, rs, rt, (int16_t)off)); \
    } while(0)

    #define BRANCH_Z(op) do { \
        int rs=mips_reg_from_name(tok1); \
        int addr=find_label(st, tok2); \
        if (rs<0||addr<0) return -1; \
        int32_t off=((int32_t)addr-(int32_t)(st->code_len+st->origin+4))>>2; \
        emit4(st, mips_encode_i(op, rs, 0, (int16_t)off)); \
    } while(0)

    if (str_eq(mnemonic, "beq"))  { BRANCH(MIPS_OP_BEQ);  return 0; }
    if (str_eq(mnemonic, "bne"))  { BRANCH(MIPS_OP_BNE);  return 0; }
    if (str_eq(mnemonic, "blez")) { BRANCH_Z(MIPS_OP_BLEZ); return 0; }
    if (str_eq(mnemonic, "bgtz")) { BRANCH_Z(MIPS_OP_BGTZ); return 0; }

    if (str_eq(mnemonic, "bltz") || str_eq(mnemonic, "bgez")) {
        int rs = mips_reg_from_name(tok1);
        int addr = find_label(st, tok2);
        if (rs < 0 || addr < 0) return -1;
        int32_t off = ((int32_t)addr - (int32_t)(st->code_len + st->origin + 4)) >> 2;
        uint8_t rt = str_eq(mnemonic, "bltz") ? MIPS_RT_BLTZ : MIPS_RT_BGEZ;
        emit4(st, mips_encode_regimm(rt, rs, (int16_t)off));
        return 0;
    }

    // ========================================================================
    // Jump Instructions
    // ========================================================================
    if (str_eq(mnemonic, "j") || str_eq(mnemonic, "jal")) {
        int addr = find_label(st, tok1);
        if (addr < 0) return -1;
        uint32_t target = ((uint32_t)addr) >> 2;
        emit4(st, mips_encode_j(str_eq(mnemonic, "j") ? MIPS_OP_J : MIPS_OP_JAL, target));
        return 0;
    }

    // Unknown instruction
    return -1;
}

// ============================================================================
// Full Assembly (two-pass)
// ============================================================================

void mips_asm_init(MipsAsmState* st, uint32_t origin) {
    st->code_len = 0;
    st->label_count = 0;
    st->pass = 0;
    st->origin = origin;
}

int mips_asm_assemble(MipsAsmState* st, const char* source) {
    // Pass 0: collect labels
    st->pass = 0;
    st->code_len = 0;

    const char* p = source;
    while (*p) {
        // Extract one line
        char line[256];
        int li = 0;
        while (*p && *p != '\n' && li < 254) line[li++] = *p++;
        if (*p == '\n') p++;
        line[li] = '\0';
        if (li == 0) continue;

        int r = mips_asm_assemble_line(st, line);
        if (r < 0) return r;
    }

    // Pass 1: emit code
    st->pass = 1;
    st->code_len = 0;

    p = source;
    while (*p) {
        char line[256];
        int li = 0;
        while (*p && *p != '\n' && li < 254) line[li++] = *p++;
        if (*p == '\n') p++;
        line[li] = '\0';
        if (li == 0) continue;

        int r = mips_asm_assemble_line(st, line);
        if (r < 0) return r;
    }

    return 0;
}

int mips_asm_finalize(MipsAsmState* st) {
    return (int)st->code_len;
}
