# SageMips VM — MIPS32 Interpreter (Sage)
# src/sage/mips_vm.sage

import mips_core

# ============================================================================
# VM State
# ============================================================================
let STACK_SIZE  = 4096
let HEAP_SIZE   = 16 * 1024 * 1024
let STRING_POOL = 256 * 1024
let CALL_DEPTH  = 256

class MipsVM:
    proc init(self):
        self.regs = [0] * 32      # 32 registers (signed int values)
        self.pc = 0
        self.running = false
        self.halted = false
        self.error = false
        self.hi = 0
        self.lo = 0
        self.code = []
        self.code_len = 0
        self.stack = [0] * STACK_SIZE
        self.sp = STACK_SIZE      # stack pointer (word index)
        self.heap = [0] * HEAP_SIZE
        self.heap_used = 0
        self.strings = ""
        self.call_stack = []      # [(chunk_idx, return_pc, saved_ra)]
        self.trace = false
        self.error_msg = ""
        self.write_char = nil
        self.write_str = nil

    proc load(self, code_bytes):
        self.code = code_bytes
        self.code_len = len(code_bytes)
        self.pc = 0
        return 0

    proc _signed(self, reg):
        return int(self.regs[reg])

    proc _unsigned(self, reg):
        let v = int(self.regs[reg])
        if v < 0:
            return v + (1 << 32)
        return v

    proc _syscall(self):
        let call = self._signed(2)     # $v0
        let a0 = self._signed(4)       # $a0

        if call == 0 or call == 9:     # exit / halt
            self.halted = true
            self.running = false
            self.regs[2] = 0
        elif call == 1:                # print_int
            if self.write_str != nil:
                let buf = str(a0)
                self.write_str(buf)
        elif call == 2:                # print_str
            if self.write_str != nil:
                # Find string at address a0 in code
                var pos = a0
                var s = ""
                while pos < self.code_len and self.code[pos] != 0:
                    s = s + chr(self.code[pos])
                    pos = pos + 1
                self.write_str(s)
        elif call == 6:                # print_char
            if self.write_char != nil:
                self.write_char(chr(a0))
        elif call == 5:                # sbrk
            if self.heap_used + a0 > HEAP_SIZE:
                self.regs[2] = -1
            else:
                self.regs[2] = self.heap_used
                self.heap_used = self.heap_used + a0
        else:
            self.regs[2] = 0

    proc _push(self, val):
        if self.sp == 0:
            self.error = true
            self.error_msg = "Stack overflow"
            return
        self.sp = self.sp - 1
        self.stack[self.sp] = int(val)

    proc _pop(self):
        if self.sp >= STACK_SIZE:
            self.error = true
            self.error_msg = "Stack underflow"
            return 0
        let v = self.stack[self.sp]
        self.sp = self.sp + 1
        return v

    proc step(self):
        if self.halted or self.error or not self.running:
            return false
        if self.pc + 4 > self.code_len:
            self.halted = true
            self.running = false
            return false

        # Fetch 32-bit big-endian instruction
        let raw = mips_core.read_be32(self.code, self.pc)
        let instr = mips_core.MipsInstr(raw)

        var next_pc = self.pc + 4

        # Hardwire $zero
        self.regs[0] = 0

        if self.trace:
            let line = mips_core.disasm(raw, self.pc)
            print line

        let opcode = instr.opcode
        let rs = instr.rs
        let rt = instr.rt
        let rd = instr.rd
        let funct = instr.funct
        let imm = instr.imm

        let rs_val = self._signed(rs)
        let rt_val = self._signed(rt)
        let urs = self._unsigned(rs)
        let urt = self._unsigned(rt)

        # === SPECIAL (R-type) ===
        if opcode == mips_core.OP_SPECIAL:
            if funct == mips_core.FN_ADD:
                self.regs[rd] = rs_val + rt_val
            elif funct == mips_core.FN_ADDU:
                self.regs[rd] = int(urs + urt)
            elif funct == mips_core.FN_SUB:
                self.regs[rd] = rs_val - rt_val
            elif funct == mips_core.FN_SUBU:
                self.regs[rd] = int(urs - urt)
            elif funct == mips_core.FN_AND:
                self.regs[rd] = int(urs & urt)
            elif funct == mips_core.FN_OR:
                self.regs[rd] = int(urs | urt)
            elif funct == mips_core.FN_XOR:
                self.regs[rd] = int(urs ^ urt)
            elif funct == mips_core.FN_NOR:
                self.regs[rd] = int(~(urs | urt))
            elif funct == mips_core.FN_SLT:
                var slt_val = 0
                if rs_val < rt_val:
                    slt_val = 1
                self.regs[rd] = slt_val
            elif funct == mips_core.FN_SLTU:
                var sltu_val = 0
                if urs < urt:
                    sltu_val = 1
                self.regs[rd] = sltu_val
            elif funct == mips_core.FN_SLL:
                self.regs[rd] = int(urt << instr.shamt)
            elif funct == mips_core.FN_SRL:
                self.regs[rd] = int(urt >> instr.shamt)
            elif funct == mips_core.FN_SRA:
                self.regs[rd] = int(rt_val >> instr.shamt)
            elif funct == mips_core.FN_SLLV:
                self.regs[rd] = int(urt << (urs & 0x1F))
            elif funct == mips_core.FN_SRLV:
                self.regs[rd] = int(urt >> (urs & 0x1F))
            elif funct == mips_core.FN_SRAV:
                self.regs[rd] = int(rt_val >> (urs & 0x1F))
            elif funct == mips_core.FN_JR:
                next_pc = urs
            elif funct == mips_core.FN_JALR:
                self.regs[rd] = int(self.pc + 8)
                next_pc = urs
            elif funct == mips_core.FN_MFHI:
                self.regs[rd] = int(self.hi)
            elif funct == mips_core.FN_MTHI:
                self.hi = rs_val
            elif funct == mips_core.FN_MFLO:
                self.regs[rd] = int(self.lo)
            elif funct == mips_core.FN_MTLO:
                self.lo = rs_val
            elif funct == mips_core.FN_MULT:
                let prod = rs_val * rt_val
                self.lo = prod
                self.hi = 0
            elif funct == mips_core.FN_MULTU:
                let prod = urs * urt
                self.lo = int(prod)
                self.hi = 0
            elif funct == mips_core.FN_DIV:
                if rt_val == 0:
                    self.error = true
                    self.error_msg = "Division by zero"
                else:
                    self.lo = int(rs_val / rt_val)
                    self.hi = rs_val % rt_val
            elif funct == mips_core.FN_DIVU:
                if urt == 0:
                    self.error = true
                    self.error_msg = "Division by zero"
                else:
                    self.lo = int(urs / urt)
                    self.hi = int(urs % urt)
            elif funct == mips_core.FN_MOVZ:
                if rt_val == 0:
                    self.regs[rd] = self.regs[rs]
            elif funct == mips_core.FN_MOVN:
                if rt_val != 0:
                    self.regs[rd] = self.regs[rs]
            elif funct == mips_core.FN_SYSCALL:
                self._syscall()
            elif funct == mips_core.FN_BREAK:
                self.halted = true
                self.running = false

        # === SPECIAL2 ===
        elif opcode == mips_core.OP_SPECIAL2:
            if funct == mips_core.FN_MUL:
                self.regs[rd] = rs_val * rt_val

        # === REGIMM ===
        elif opcode == mips_core.OP_REGIMM:
            var take = false
            if rt == mips_core.RT_BLTZ:
                take = rs_val < 0
            elif rt == mips_core.RT_BGEZ:
                take = rs_val >= 0
            elif rt == mips_core.RT_BLTZAL:
                if rs_val < 0:
                    self.regs[31] = int(self.pc + 8)
                    take = true
            elif rt == mips_core.RT_BGEZAL:
                if rs_val >= 0:
                    self.regs[31] = int(self.pc + 8)
                    take = true
            if take:
                next_pc = self.pc + 4 + int(imm * 4)

        # === J / JAL ===
        elif opcode == mips_core.OP_J:
            next_pc = (self.pc & 0xF0000000) | (instr.target << 2)
        elif opcode == mips_core.OP_JAL:
            self.regs[31] = int(self.pc + 8)
            next_pc = (self.pc & 0xF0000000) | (instr.target << 2)

        # === Branch ===
        elif opcode == mips_core.OP_BEQ:
            if rs_val == rt_val:
                next_pc = self.pc + 4 + int(imm * 4)
        elif opcode == mips_core.OP_BNE:
            if rs_val != rt_val:
                next_pc = self.pc + 4 + int(imm * 4)
        elif opcode == mips_core.OP_BLEZ:
            if rs_val <= 0:
                next_pc = self.pc + 4 + int(imm * 4)
        elif opcode == mips_core.OP_BGTZ:
            if rs_val > 0:
                next_pc = self.pc + 4 + int(imm * 4)

        # === Arithmetic Immediate ===
        elif opcode == mips_core.OP_ADDI:
            self.regs[rt] = rs_val + int(imm)
        elif opcode == mips_core.OP_ADDIU:
            self.regs[rt] = int(urs + int(imm & 0xFFFF))
        elif opcode == mips_core.OP_SLTI:
            var sl_val = 0
            if rs_val < int(imm):
                sl_val = 1
            self.regs[rt] = sl_val
        elif opcode == mips_core.OP_SLTIU:
            var slu_val = 0
            if urs < int(imm & 0xFFFF):
                slu_val = 1
            self.regs[rt] = slu_val
        elif opcode == mips_core.OP_ANDI:
            self.regs[rt] = int(urs & (int(imm) & 0xFFFF))
        elif opcode == mips_core.OP_ORI:
            self.regs[rt] = int(urs | (int(imm) & 0xFFFF))
        elif opcode == mips_core.OP_XORI:
            self.regs[rt] = int(urs ^ (int(imm) & 0xFFFF))
        elif opcode == mips_core.OP_LUI:
            self.regs[rt] = int((imm & 0xFFFF) << 16)

        # === Load/Store ===
        elif opcode == mips_core.OP_LW:
            let addr = rs_val + int(imm)
            if addr < 0 or addr + 4 > STACK_SIZE * 4:
                self.error = true
                self.error_msg = "LW access violation"
            else:
                let wi = addr / 4
                if wi < STACK_SIZE:
                    self.regs[rt] = self.stack[wi]
                else:
                    self.regs[rt] = 0

        elif opcode == mips_core.OP_SW:
            let addr = rs_val + int(imm)
            if addr < 0 or addr + 4 > STACK_SIZE * 4:
                self.error = true
                self.error_msg = "SW access violation"
            else:
                let wi = addr / 4
                if wi < STACK_SIZE:
                    self.stack[wi] = self._unsigned(rt)

        elif opcode == mips_core.OP_LB:
            let addr = rs_val + int(imm)
            if addr < 0 or addr >= STACK_SIZE * 4:
                self.error = true
            else:
                let wi = addr / 4
                let bo = addr % 4
                let b = (self.stack[wi] >> (bo * 8)) & 0xFF
                if b & 0x80:
                    self.regs[rt] = b - 256
                else:
                    self.regs[rt] = b

        elif opcode == mips_core.OP_LBU:
            let addr = rs_val + int(imm)
            if addr < 0 or addr >= STACK_SIZE * 4:
                self.error = true
            else:
                let wi = addr / 4
                let bo = addr % 4
                self.regs[rt] = (self.stack[wi] >> (bo * 8)) & 0xFF

        elif opcode == mips_core.OP_SB:
            let addr = rs_val + int(imm)
            if addr < 0 or addr >= STACK_SIZE * 4:
                self.error = true
            else:
                let wi = addr / 4
                let bo = addr % 4
                let mask = ~(0xFF << (bo * 8))
                let bval = self._unsigned(rt) & 0xFF
                self.stack[wi] = (self.stack[wi] & mask) | (bval << (bo * 8))

        elif opcode == mips_core.OP_SH:
            let addr = rs_val + int(imm)
            if addr < 0 or addr + 2 > STACK_SIZE * 4:
                self.error = true
            else:
                let wi = addr / 4
                let bo = addr % 4
                let mask = ~(0xFFFF << (bo * 8))
                let hval = self._unsigned(rt) & 0xFFFF
                self.stack[wi] = (self.stack[wi] & mask) | (hval << (bo * 8))

        else:
            self.error = true
            self.error_msg = "Unimplemented MIPS opcode: " + str(opcode)
            self.running = false
            return false

        # $zero hardwired
        self.regs[0] = 0
        self.pc = next_pc

        return not self.error

    proc run(self):
        self.running = true
        self.halted = false
        self.error = false
        self.sp = STACK_SIZE

        let max_steps = 10000000
        var steps = 0
        while self.running and not self.halted and not self.error and steps < max_steps:
            if not self.step():
                break
            steps = steps + 1

        if self.error and self.error_msg != "":
            print "VM Error: " + self.error_msg

        if self.error:
            return -1
        return 0
