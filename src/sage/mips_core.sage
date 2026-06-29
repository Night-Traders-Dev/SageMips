# SageMips Core — MIPS32 opcodes, instruction encode/decode, disassembler
# src/sage/mips_core.sage

# ============================================================================
# MIPS32 Opcodes (6-bit primary)
# ============================================================================
let OP_SPECIAL  = 0x00
let OP_REGIMM   = 0x01
let OP_J        = 0x02
let OP_JAL      = 0x03
let OP_BEQ      = 0x04
let OP_BNE      = 0x05
let OP_BLEZ     = 0x06
let OP_BGTZ     = 0x07
let OP_ADDI     = 0x08
let OP_ADDIU    = 0x09
let OP_SLTI     = 0x0A
let OP_SLTIU    = 0x0B
let OP_ANDI     = 0x0C
let OP_ORI      = 0x0D
let OP_XORI     = 0x0E
let OP_LUI      = 0x0F
let OP_SPECIAL2 = 0x1C
let OP_LB       = 0x20
let OP_LH       = 0x21
let OP_LW       = 0x23
let OP_LBU      = 0x24
let OP_LHU      = 0x25
let OP_SB       = 0x28
let OP_SH       = 0x29
let OP_SW       = 0x2B

# ============================================================================
# SPECIAL (opcode 0x00) Function Codes
# ============================================================================
let FN_SLL     = 0x00
let FN_SRL     = 0x02
let FN_SRA     = 0x03
let FN_SLLV    = 0x04
let FN_SRLV    = 0x06
let FN_SRAV    = 0x07
let FN_JR      = 0x08
let FN_JALR    = 0x09
let FN_MOVZ    = 0x0A
let FN_MOVN    = 0x0B
let FN_SYSCALL = 0x0C
let FN_BREAK   = 0x0D
let FN_MFHI    = 0x10
let FN_MTHI    = 0x11
let FN_MFLO    = 0x12
let FN_MTLO    = 0x13
let FN_MULT    = 0x18
let FN_MULTU   = 0x19
let FN_DIV     = 0x1A
let FN_DIVU    = 0x1B
let FN_ADD     = 0x20
let FN_ADDU    = 0x21
let FN_SUB     = 0x22
let FN_SUBU    = 0x23
let FN_AND     = 0x24
let FN_OR      = 0x25
let FN_XOR     = 0x26
let FN_NOR     = 0x27
let FN_SLT     = 0x2A
let FN_SLTU    = 0x2B
let FN_TGE     = 0x30
let FN_TGEU    = 0x31
let FN_TLT     = 0x32
let FN_TLTU    = 0x33
let FN_TEQ     = 0x34
let FN_TNE     = 0x36

# SPECIAL2 funct
let FN_MUL     = 0x02

# REGIMM rt values
let RT_BLTZ    = 0x00
let RT_BGEZ    = 0x01
let RT_BLTZAL  = 0x10
let RT_BGEZAL  = 0x11

# ============================================================================
# Register Names
# ============================================================================
let REG_NAMES = [
    "zero","at","v0","v1","a0","a1","a2","a3",
    "t0","t1","t2","t3","t4","t5","t6","t7",
    "s0","s1","s2","s3","s4","s5","s6","s7",
    "t8","t9","k0","k1","gp","sp","fp","ra"
]

proc reg_name(reg):
    if reg < 0 or reg > 31:
        return "?"
    return REG_NAMES[reg]

proc reg_from_name(name):
    if name[0] == "$":
        name = name[1:]
    let i = 0
    while i < 32:
        if name == REG_NAMES[i]:
            return i
        i = i + 1
    return -1

# ============================================================================
# Instruction Decode
# ============================================================================

class MipsInstr:
    proc init(self, raw):
        let v = int(raw)
        self.raw = v
        self.opcode = (v >> 26) & 0x3F
        self.rs     = (v >> 21) & 0x1F
        self.rt     = (v >> 16) & 0x1F
        self.rd     = (v >> 11) & 0x1F
        self.shamt  = (v >> 6)  & 0x1F
        self.funct  = v & 0x3F

        # Immediate (16-bit signed)
        let uimm = v & 0xFFFF
        let sign_bit = 1 << 15
        if (uimm & sign_bit) != 0:
            self.imm = uimm - (1 << 16)
        else:
            self.imm = uimm

        # Jump target (26-bit)
        self.target = v & 0x03FFFFFF

# ============================================================================
# Instruction Encode
# ============================================================================

proc encode_r(funct, rs, rt, rd, shamt):
    let result = OP_SPECIAL & 0x3F
    result = result << 5 | (rs & 0x1F)
    result = result << 5 | (rt & 0x1F)
    result = result << 5 | (rd & 0x1F)
    result = result << 5 | (shamt & 0x1F)
    result = result << 6 | (funct & 0x3F)
    return result

proc encode_r_special2(funct, rs, rt, rd):
    let result = OP_SPECIAL2 & 0x3F
    result = result << 5 | (rs & 0x1F)
    result = result << 5 | (rt & 0x1F)
    result = result << 5 | (rd & 0x1F)
    result = result << 11
    result = result | (funct & 0x3F)
    return result

proc encode_i(opcode, rs, rt, imm):
    let result = opcode & 0x3F
    result = result << 5 | (rs & 0x1F)
    result = result << 5 | (rt & 0x1F)
    result = result << 16 | (int(imm) & 0xFFFF)
    return result

proc encode_j(opcode, target):
    let result = opcode & 0x3F
    result = result << 26 | (target & 0x03FFFFFF)
    return result

proc encode_regimm(rt, rs, imm):
    return encode_i(OP_REGIMM, rs, rt, int(imm))

# ============================================================================
# Read big-endian 32-bit from byte array
# ============================================================================

proc read_be32(data, addr):
    var v = int(data[addr]) << 24
    v = v | (int(data[addr+1]) << 16)
    v = v | (int(data[addr+2]) << 8)
    v = v | int(data[addr+3])
    return v

# ============================================================================
# Disassembler
# ============================================================================

proc get_r_mnemonic(funct):
    if funct == FN_ADD:    return "add"
    elif funct == FN_ADDU:  return "addu"
    elif funct == FN_SUB:   return "sub"
    elif funct == FN_SUBU:  return "subu"
    elif funct == FN_AND:   return "and"
    elif funct == FN_OR:    return "or"
    elif funct == FN_XOR:   return "xor"
    elif funct == FN_NOR:   return "nor"
    elif funct == FN_SLT:   return "slt"
    elif funct == FN_SLTU:  return "sltu"
    elif funct == FN_SLL:   return "sll"
    elif funct == FN_SRL:   return "srl"
    elif funct == FN_SRA:   return "sra"
    elif funct == FN_SLLV:  return "sllv"
    elif funct == FN_SRLV:  return "srlv"
    elif funct == FN_SRAV:  return "srav"
    elif funct == FN_JR:    return "jr"
    elif funct == FN_JALR:  return "jalr"
    elif funct == FN_MFHI:  return "mfhi"
    elif funct == FN_MTHI:  return "mthi"
    elif funct == FN_MFLO:  return "mflo"
    elif funct == FN_MTLO:  return "mtlo"
    elif funct == FN_MULT:  return "mult"
    elif funct == FN_MULTU: return "multu"
    elif funct == FN_DIV:   return "div"
    elif funct == FN_DIVU:  return "divu"
    elif funct == FN_SYSCALL: return "syscall"
    elif funct == FN_BREAK: return "break"
    elif funct == FN_MOVZ:  return "movz"
    elif funct == FN_MOVN:  return "movn"
    elif funct == FN_TGE:   return "tge"
    elif funct == FN_TGEU:  return "tgeu"
    elif funct == FN_TLT:   return "tlt"
    elif funct == FN_TLTU:  return "tltu"
    elif funct == FN_TEQ:   return "teq"
    elif funct == FN_TNE:   return "tne"
    return nil

proc get_i_mnemonic(opcode):
    if opcode == OP_ADDI:   return "addi"
    elif opcode == OP_ADDIU:  return "addiu"
    elif opcode == OP_SLTI:   return "slti"
    elif opcode == OP_SLTIU:  return "sltiu"
    elif opcode == OP_ANDI:   return "andi"
    elif opcode == OP_ORI:    return "ori"
    elif opcode == OP_XORI:   return "xori"
    elif opcode == OP_LUI:    return "lui"
    elif opcode == OP_LW:     return "lw"
    elif opcode == OP_SW:     return "sw"
    elif opcode == OP_LB:     return "lb"
    elif opcode == OP_LBU:    return "lbu"
    elif opcode == OP_LH:     return "lh"
    elif opcode == OP_LHU:    return "lhu"
    elif opcode == OP_SB:     return "sb"
    elif opcode == OP_SH:     return "sh"
    elif opcode == OP_BEQ:    return "beq"
    elif opcode == OP_BNE:    return "bne"
    elif opcode == OP_BLEZ:   return "blez"
    elif opcode == OP_BGTZ:   return "bgtz"
    return nil

proc disasm(raw, addr):
    let i = MipsInstr(raw)
    var result = ""

    # Address
    var h = hex(addr)
    while len(h) < 8:
        h = "0" + h
    result = "0x" + h + "  "

    # Raw hex
    h = hex(int(raw))
    while len(h) < 8:
        h = "0" + h
    result = result + "0x" + h + "  "

    let opcode = i.opcode
    let funct = i.funct

    if opcode == OP_SPECIAL:
        let mn = get_r_mnemonic(funct)
        if mn == nil:
            return result + "???"

        if funct == FN_SLL or funct == FN_SRL or funct == FN_SRA:
            result = result + mn + " $" + reg_name(i.rd) + ", $" + reg_name(i.rt) + ", " + str(i.shamt)
        elif funct == FN_JR:
            result = result + mn + " $" + reg_name(i.rs)
        elif funct == FN_MFHI or funct == FN_MFLO:
            result = result + mn + " $" + reg_name(i.rd)
        elif funct == FN_MTHI or funct == FN_MTLO:
            result = result + mn + " $" + reg_name(i.rs)
        elif funct == FN_JALR:
            result = result + mn + " $" + reg_name(i.rd) + ", $" + reg_name(i.rs)
        elif funct == FN_MULT or funct == FN_MULTU or funct == FN_DIV or funct == FN_DIVU:
            result = result + mn + " $" + reg_name(i.rs) + ", $" + reg_name(i.rt)
        elif funct == FN_SYSCALL or funct == FN_BREAK:
            result = result + mn
        elif funct == FN_MOVZ or funct == FN_MOVN:
            result = result + mn + " $" + reg_name(i.rd) + ", $" + reg_name(i.rs) + ", $" + reg_name(i.rt)
        else:
            result = result + mn + " $" + reg_name(i.rd) + ", $" + reg_name(i.rs) + ", $" + reg_name(i.rt)

    elif opcode == OP_SPECIAL2:
        if funct == FN_MUL:
            result = result + "mul $" + reg_name(i.rd) + ", $" + reg_name(i.rs) + ", $" + reg_name(i.rt)

    elif opcode == OP_REGIMM:
        let rt = i.rt
        if rt == RT_BLTZ:
            result = result + "bltz"
        elif rt == RT_BGEZ:
            result = result + "bgez"
        elif rt == RT_BLTZAL:
            result = result + "bltzal"
        elif rt == RT_BGEZAL:
            result = result + "bgezal"
        result = result + " $" + reg_name(i.rs) + ", " + hex(addr + 4 + (i.imm << 2))

    elif opcode == OP_J or opcode == OP_JAL:
        var jmn = "jal"
        if opcode == OP_J:
            jmn = "j"
        result = result + jmn + " "
        let jtarget = (addr & 0xF0000000) | (i.target << 2)
        result = result + hex(jtarget)

    elif opcode == OP_BEQ or opcode == OP_BNE:
        var bmn = "bne"
        if opcode == OP_BEQ:
            bmn = "beq"
        result = result + bmn + " $" + reg_name(i.rs) + ", $" + reg_name(i.rt) + ", "
        result = result + hex(addr + 4 + (i.imm << 2))

    elif opcode == OP_BLEZ or opcode == OP_BGTZ:
        var bzmn = "bgtz"
        if opcode == OP_BLEZ:
            bzmn = "blez"
        result = result + bzmn + " $" + reg_name(i.rs) + ", "
        result = result + hex(addr + 4 + (i.imm << 2))

    elif opcode == OP_LUI:
        result = result + "lui $" + reg_name(i.rt) + ", " + hex(i.imm & 0xFFFF)

    elif opcode == OP_LW or opcode == OP_SW or opcode == OP_LB or opcode == OP_LBU or opcode == OP_LH or opcode == OP_LHU or opcode == OP_SB or opcode == OP_SH:
        let mn = get_i_mnemonic(opcode)
        result = result + mn + " $" + reg_name(i.rt) + ", " + str(i.imm) + "($" + reg_name(i.rs) + ")"

    elif opcode == OP_ADDI or opcode == OP_ADDIU or opcode == OP_SLTI or opcode == OP_SLTIU or opcode == OP_ANDI or opcode == OP_ORI or opcode == OP_XORI:
        result = result + get_i_mnemonic(opcode) + " $" + reg_name(i.rt) + ", $" + reg_name(i.rs) + ", " + str(i.imm)

    return result

# ============================================================================
# Hex formatter
# ============================================================================
proc hex(n):
    let chars = "0123456789abcdef"
    if n == 0:
        return "0"
    var result = ""
    var v = n
    while v > 0:
        result = chars[v & 0xF] + result
        v = v >> 4
    return result
