# SageMips Assembler — Two-pass MIPS32 assembler (Sage)
# src/sage/mips_asm.sage

import mips_core

# ============================================================================
# Assembler State
# ============================================================================

class MipsAsm:
    proc init(self, origin):
        self.origin = origin
        self.code = []
        self.labels = {}     # name -> address
        self.pass_num = 0    # 0 = collect labels, 1 = emit

    proc emit4(self, val):
        let v = int(val)
        push(self.code, (v >> 24) & 0xFF)
        push(self.code, (v >> 16) & 0xFF)
        push(self.code, (v >> 8) & 0xFF)
        push(self.code, v & 0xFF)

    proc add_label(self, name, addr):
        if dict_has(self.labels, name):
            self.labels[name] = addr
        else:
            self.labels[name] = addr

    proc find_label(self, name):
        if dict_has(self.labels, name):
            return self.labels[name]
        return -1

# ============================================================================
# Parse helpers
# ============================================================================

proc tokenize(line):
    # Split line into tokens, respecting commas
    var tokens = []
    var tok = ""
    var i = 0
    while i < len(line):
        let c = line[i]
        if ord(c) == 35 or ord(c) == 59:    # '#' or ';'
            break
        elif ord(c) == 32 or ord(c) == 9:     # space or tab
            if tok != "":
                push(tokens, tok)
                tok = ""
        elif ord(c) == 44:
            if tok != "":
                push(tokens, tok)
                tok = ""
            push(tokens, ",")
        elif ord(c) == 40 or ord(c) == 41:
            if tok != "":
                push(tokens, tok)
                tok = ""
            push(tokens, c)
        elif ord(c) == 58:
            if tok != "":
                push(tokens, tok)
                tok = ""
            push(tokens, ":")
        else:
            tok = tok + c
        i = i + 1
    if tok != "":
        push(tokens, tok)
    return tokens

proc parse_int_str(s):
    if s == "":
        return nil
    # Hex
    if startswith(s, "0x") or startswith(s, "0X"):
        var v = 0
        var i = 2
        while i < len(s):
            let c = s[i]
            v = v * 16
            if ord(c) >= 48 and ord(c) <= 57:
                v = v + (ord(c) - 48)
            elif ord(c) >= 97 and ord(c) <= 102:
                v = v + (ord(c) - 87)
            elif ord(c) >= 65 and ord(c) <= 70:
                v = v + (ord(c) - 55)
            i = i + 1
        return v
    # Decimal
    var neg = false
    var i = 0
    if ord(s[i]) == 45:
        neg = true
        i = i + 1
    var v = 0
    while i < len(s):
        let o = ord(s[i])
        if o < 48 or o > 57:
            break
        v = v * 10 + (o - 48)
        i = i + 1
    if neg:
        return -v
    return v

# reg_from_name is imported from mips_core — use mips_core.reg_from_name
# instead of redefining it

# ============================================================================
# Assemble single line
# ============================================================================

proc assemble_line(asm, tokens):
    if len(tokens) == 0:
        return 0

    let first = tokens[0]

    # Label?
    if len(tokens) >= 2 and tokens[1] == ":":
        asm.add_label(first, len(asm.code) + asm.origin)
        # Remove label + colon
        tokens = tokens[2:]
        if len(tokens) == 0:
            return 0

    let mnemonic = tokens[0]
    let rest = tokens[1:]

    # Remove commas from rest
    var filtered = []
    for tok in rest:
        if tok != ",":
            push(filtered, tok)
    rest = filtered

    # Directives
    if mnemonic == ".text" or mnemonic == ".data" or mnemonic == ".globl" or mnemonic == ".global":
        return 0

    if mnemonic == ".word":
        if len(rest) >= 1:
            let val = parse_int_str(rest[0])
            if val != nil:
                if asm.pass_num == 1:
                    asm.emit4(int(val))
                else:
                    asm.code = asm.code + [0,0,0,0]  # placeholder
                return 0
        return -1

    if mnemonic == ".asciiz" or mnemonic == ".ascii":
        # Not fully implemented in Sage assembler — skip for now
        return 0

    if mnemonic == ".space":
        if len(rest) >= 1:
            let n = parse_int_str(rest[0])
            if n != nil:
                var i = 0
                while i < n:
                    push(asm.code, 0)
                    i = i + 1
        return 0

    # In pass 0, just reserve space for instructions
    if asm.pass_num == 0:
        # Reserve 4 bytes for standard instructions, 8 for pseudo
        if mnemonic == "li" or mnemonic == "la":
            asm.code = asm.code + [0,0,0,0, 0,0,0,0]
        else:
            asm.code = asm.code + [0,0,0,0]
        return 0

    # ====================================================================
    # Pass 1: Emit code
    # ====================================================================

    if mnemonic == "nop":
        asm.emit4(mips_core.encode_r(mips_core.FN_SLL, 0, 0, 0, 0))
        return 0

    if mnemonic == "move":
        if len(rest) >= 2:
            let rd = mips_core.reg_from_name(rest[0])
            let rs = mips_core.reg_from_name(rest[1])
            asm.emit4(mips_core.encode_r(mips_core.FN_ADDU, rs, 0, rd, 0))
        return 0

    if mnemonic == "not":
        if len(rest) >= 2:
            let rd = mips_core.reg_from_name(rest[0])
            let rs = mips_core.reg_from_name(rest[1])
            asm.emit4(mips_core.encode_r(mips_core.FN_NOR, rs, 0, rd, 0))
        return 0

    if mnemonic == "neg":
        if len(rest) >= 2:
            let rd = mips_core.reg_from_name(rest[0])
            let rs = mips_core.reg_from_name(rest[1])
            asm.emit4(mips_core.encode_r(mips_core.FN_SUB, 0, rs, rd, 0))
        return 0

    if mnemonic == "b":
        if len(rest) >= 1:
            let addr = asm.find_label(rest[0])
            let off = int(int(addr - (len(asm.code) + asm.origin + 4)) / 4)
            asm.emit4(mips_core.encode_i(mips_core.OP_BEQ, 0, 0, off))
        return 0

    if mnemonic == "li" or mnemonic == "la":
        if len(rest) >= 2:
            let rt = mips_core.reg_from_name(rest[0])
            var val = parse_int_str(rest[1])
            if val == nil:
                val = asm.find_label(rest[1])
            if val != nil:
                let upper = (int(val) >> 16) & 0xFFFF
                let lower = int(val) & 0xFFFF
                if upper != 0:
                    asm.emit4(mips_core.encode_i(mips_core.OP_LUI, 0, rt, upper))
                if lower != 0 or upper == 0:
                    asm.emit4(mips_core.encode_i(mips_core.OP_ORI, rt, rt, lower))
        return 0

    # Syscall
    if mnemonic == "syscall":
        asm.emit4(mips_core.encode_r(mips_core.FN_SYSCALL, 0, 0, 0, 0))
        return 0

    # R-type: add, addu, sub, subu, and, or, xor, nor, slt, sltu, sllv, srlv, srav
    let r_ops = {
        "add":  mips_core.FN_ADD,  "addu": mips_core.FN_ADDU,
        "sub":  mips_core.FN_SUB,  "subu": mips_core.FN_SUBU,
        "and":  mips_core.FN_AND,  "or":   mips_core.FN_OR,
        "xor":  mips_core.FN_XOR,  "nor":  mips_core.FN_NOR,
        "slt":  mips_core.FN_SLT,  "sltu": mips_core.FN_SLTU,
        "sllv": mips_core.FN_SLLV, "srlv": mips_core.FN_SRLV, "srav": mips_core.FN_SRAV
    }
    if dict_has(r_ops, mnemonic) and len(rest) >= 3:
        let rd = mips_core.reg_from_name(rest[0])
        let rs = mips_core.reg_from_name(rest[1])
        let rt = mips_core.reg_from_name(rest[2])
        asm.emit4(mips_core.encode_r(r_ops[mnemonic], rs, rt, rd, 0))
        return 0

    # Shift: sll, srl, sra (rd, rt, shamt)
    if (mnemonic == "sll" or mnemonic == "srl" or mnemonic == "sra") and len(rest) >= 3:
        let rd = mips_core.reg_from_name(rest[0])
        let rt = mips_core.reg_from_name(rest[1])
        let shamt = parse_int_str(rest[2])
        if shamt != nil:
            var fn = mips_core.FN_SLL
            if mnemonic == "sll":
                fn = mips_core.FN_SLL
            elif mnemonic == "srl":
                fn = mips_core.FN_SRL
            else:
                fn = mips_core.FN_SRA
            asm.emit4(mips_core.encode_r(fn, 0, rt, rd, int(shamt)))
        return 0

    # mult, multu, div, divu (rs, rt)
    if (mnemonic == "mult" or mnemonic == "multu" or mnemonic == "div" or mnemonic == "divu") and len(rest) >= 2:
        let rs = mips_core.reg_from_name(rest[0])
        let rt = mips_core.reg_from_name(rest[1])
        var fn = mips_core.FN_MULT
        if mnemonic == "mult":
            fn = mips_core.FN_MULT
        elif mnemonic == "multu":
            fn = mips_core.FN_MULTU
        elif mnemonic == "div":
            fn = mips_core.FN_DIV
        else:
            fn = mips_core.FN_DIVU
        asm.emit4(mips_core.encode_r(fn, rs, rt, 0, 0))
        return 0

    # mfhi, mflo (rd)
    if (mnemonic == "mfhi" or mnemonic == "mflo") and len(rest) >= 1:
        let rd = mips_core.reg_from_name(rest[0])
        var fn = mips_core.FN_MFHI
        if mnemonic == "mflo":
            fn = mips_core.FN_MFLO
        asm.emit4(mips_core.encode_r(fn, 0, 0, rd, 0))
        return 0

    # mthi, mtlo (rs)
    if (mnemonic == "mthi" or mnemonic == "mtlo") and len(rest) >= 1:
        let rs = mips_core.reg_from_name(rest[0])
        var fn = mips_core.FN_MTHI
        if mnemonic == "mtlo":
            fn = mips_core.FN_MTLO
        asm.emit4(mips_core.encode_r(fn, rs, 0, 0, 0))
        return 0

    # jr, jalr
    if mnemonic == "jr" and len(rest) >= 1:
        let rs = mips_core.reg_from_name(rest[0])
        asm.emit4(mips_core.encode_r(mips_core.FN_JR, rs, 0, 0, 0))
        return 0
    if mnemonic == "jalr" and len(rest) >= 1:
        let rs = mips_core.reg_from_name(rest[0])
        let rd = 31
        if len(rest) >= 2:
            rd = mips_core.reg_from_name(rest[0])
            rs = mips_core.reg_from_name(rest[1])
        asm.emit4(mips_core.encode_r(mips_core.FN_JALR, rs, 0, rd, 0))
        return 0

    # mul (SPECIAL2)
    if mnemonic == "mul" and len(rest) >= 3:
        let rd = mips_core.reg_from_name(rest[0])
        let rs = mips_core.reg_from_name(rest[1])
        let rt = mips_core.reg_from_name(rest[2])
        asm.emit4(mips_core.encode_r_special2(mips_core.FN_MUL, rs, rt, rd))
        return 0

    # I-type arithmetic: addi, addiu, slti, sltiu, andi, ori, xori
    let i_ops = {
        "addi": mips_core.OP_ADDI, "addiu": mips_core.OP_ADDIU,
        "slti": mips_core.OP_SLTI, "sltiu": mips_core.OP_SLTIU,
        "andi": mips_core.OP_ANDI, "ori":   mips_core.OP_ORI, "xori": mips_core.OP_XORI
    }
    if dict_has(i_ops, mnemonic) and len(rest) >= 3:
        let rt = mips_core.reg_from_name(rest[0])
        let rs = mips_core.reg_from_name(rest[1])
        let imm = parse_int_str(rest[2])
        if imm != nil:
            asm.emit4(mips_core.encode_i(i_ops[mnemonic], rs, rt, int(imm)))
        return 0

    # lui
    if mnemonic == "lui" and len(rest) >= 2:
        let rt = mips_core.reg_from_name(rest[0])
        let imm = parse_int_str(rest[1])
        if imm != nil:
            asm.emit4(mips_core.encode_i(mips_core.OP_LUI, 0, rt, int(imm)))
        return 0

    # Load/store: lw, sw, lb, lbu, lh, lhu, sb, sh (rt, offset(base))
    let ls_ops = {
        "lw": mips_core.OP_LW, "sw": mips_core.OP_SW,
        "lb": mips_core.OP_LB, "lbu": mips_core.OP_LBU,
        "lh": mips_core.OP_LH, "lhu": mips_core.OP_LHU,
        "sb": mips_core.OP_SB, "sh": mips_core.OP_SH
    }
    if dict_has(ls_ops, mnemonic) and len(rest) >= 2:
        let rt = mips_core.reg_from_name(rest[0])
        var imm = 0
        var rs = 0
        # Parse offset(base) or offset, base
        if len(rest) >= 3:
            let off = parse_int_str(rest[1])
            rs = mips_core.reg_from_name(rest[2])
            if off != nil:
                imm = int(off)
        asm.emit4(mips_core.encode_i(ls_ops[mnemonic], rs, rt, imm))
        return 0

    # Branch: beq, bne (rs, rt, label)
    if (mnemonic == "beq" or mnemonic == "bne") and len(rest) >= 3:
        let rs = mips_core.reg_from_name(rest[0])
        let rt = mips_core.reg_from_name(rest[1])
        let addr = asm.find_label(rest[2])
        if addr >= 0:
            let off = int(int(addr - (len(asm.code) + asm.origin + 4)) / 4)
            var op = mips_core.OP_BEQ
            if mnemonic == "bne":
                op = mips_core.OP_BNE
            asm.emit4(mips_core.encode_i(op, rs, rt, off))
        return 0

    # Branch-zero: blez, bgtz (rs, label)
    if (mnemonic == "blez" or mnemonic == "bgtz") and len(rest) >= 2:
        let rs = mips_core.reg_from_name(rest[0])
        let addr = asm.find_label(rest[1])
        if addr >= 0:
            let off = int(int(addr - (len(asm.code) + asm.origin + 4)) / 4)
            var op = mips_core.OP_BLEZ
            if mnemonic == "bgtz":
                op = mips_core.OP_BGTZ
            asm.emit4(mips_core.encode_i(op, rs, 0, off))
        return 0

    # bltz, bgez
    if (mnemonic == "bltz" or mnemonic == "bgez") and len(rest) >= 2:
        let rs = mips_core.reg_from_name(rest[0])
        let addr = asm.find_label(rest[1])
        if addr >= 0:
            let off = int(int(addr - (len(asm.code) + asm.origin + 4)) / 4)
            var rt = mips_core.RT_BLTZ
            if mnemonic == "bgez":
                rt = mips_core.RT_BGEZ
            asm.emit4(mips_core.encode_regimm(rt, rs, off))
        return 0

    # j, jal
    if (mnemonic == "j" or mnemonic == "jal") and len(rest) >= 1:
        let addr = asm.find_label(rest[0])
        if addr >= 0:
            let target = int(addr / 4)
            var op = mips_core.OP_J
            if mnemonic == "jal":
                op = mips_core.OP_JAL
            asm.emit4(mips_core.encode_j(op, target))
        return 0

    return 0

# ============================================================================
# Full assembly (two-pass)
# ============================================================================

proc assemble(source):
    let asm = MipsAsm(0)

    # Pass 0: collect labels
    asm.pass_num = 0
    let lines = split(source, "\n")
    for line in lines:
        line = strip(line)
        if line == "" or startswith(line, "#"):
            continue
        let tokens = tokenize(line)
        if len(tokens) == 0:
            continue
        assemble_line(asm, tokens)

    # Pass 1: emit code
    asm.pass_num = 1
    # Reset code but keep labels
    let old_labels = asm.labels
    asm.code = []
    asm.labels = old_labels

    for line in lines:
        line = strip(line)
        if line == "" or startswith(line, "#"):
            continue
        let tokens = tokenize(line)
        if len(tokens) == 0:
            continue
        assemble_line(asm, tokens)

    return asm.code
