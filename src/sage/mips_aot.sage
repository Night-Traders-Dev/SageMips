# SageMips AOT Optimizer (Sage) — Static peephole optimization
# src/sage/mips_aot.sage

import mips_core

class MipsAOT:
    proc init(self):
        self.opt_code = []
        self.nops_removed = 0
        self.consts_folded = 0
        self.branches_optimized = 0
        self.dead_removed = 0

    proc optimize(self, code):
        self.opt_code = []
        self.nops_removed = 0
        self.consts_folded = 0
        self.branches_optimized = 0
        self.dead_removed = 0

        # Copy original
        var i = 0
        while i < len(code):
            push(self.opt_code, code[i])
            i = i + 1

        # Run passes
        self._pass_nop_elim(code)
        self._pass_const_fold()
        self._pass_branch_opt()
        return self.opt_code

    proc _is_nop(self, raw):
        return int(raw) == 0

    proc _is_branch(self, raw):
        let i = mips_core.MipsInstr(raw)
        let op = i.opcode
        if op == mips_core.OP_J or op == mips_core.OP_JAL:
            return true
        if op == mips_core.OP_REGIMM:
            return true
        if op == mips_core.OP_BEQ or op == mips_core.OP_BNE:
            return true
        if op == mips_core.OP_BLEZ or op == mips_core.OP_BGTZ:
            return true
        if op == mips_core.OP_SPECIAL:
            let fn = i.funct
            if fn == mips_core.FN_JR or fn == mips_core.FN_JALR:
                return true
        return false

    proc _is_terminator(self, raw):
        let i = mips_core.MipsInstr(raw)
        if i.opcode == mips_core.OP_J:
            return true
        if i.opcode == mips_core.OP_SPECIAL:
            let fn = i.funct
            if fn == mips_core.FN_JR or fn == mips_core.FN_SYSCALL:
                return true
        return false

    proc _pass_nop_elim(self, orig):
        var result = []
        var pc = 0
        while pc < len(self.opt_code):
            let instr = self.opt_code[pc]
            if self._is_nop(instr):
                self.nops_removed = self.nops_removed + 1
                pc = pc + 1
            else:
                push(result, instr)
                pc = pc + 1
        self.opt_code = result

    proc _pass_const_fold(self):
        var result = []
        var i = 0
        let code = self.opt_code
        while i < len(code):
            if i + 1 < len(code):
                let a = mips_core.MipsInstr(code[i])
                let b = mips_core.MipsInstr(code[i+1])
                # ADDIU $x,$0,A ; ADDIU $y,$x,B  ->  ADDIU $y,$0,A+B
                if a.opcode == mips_core.OP_ADDIU and a.rs == 0:
                    if b.opcode == mips_core.OP_ADDIU and b.rs == a.rt and b.rt != a.rt:
                        let val = int(a.imm) + int(b.imm)
                        push(result, mips_core.encode_i(mips_core.OP_ADDIU, 0, b.rt, val))
                        self.consts_folded = self.consts_folded + 1
                        i = i + 2
                        continue
                    # ADDIU $x,$0,A ; ORI $x,$x,B  ->  ADDIU $x,$0,A|B
                    if b.opcode == mips_core.OP_ORI and b.rs == a.rt and b.rt == a.rt:
                        let val = int(a.imm) | int(b.imm)
                        push(result, mips_core.encode_i(mips_core.OP_ADDIU, 0, a.rt, val))
                        self.consts_folded = self.consts_folded + 1
                        i = i + 2
                        continue
            push(result, code[i])
            i = i + 1
        self.opt_code = result

    proc _pass_branch_opt(self):
        var result = []
        var i = 0
        let code = self.opt_code
        while i < len(code):
            let instr = code[i]
            let di = mips_core.MipsInstr(instr)
            # beq $0,$0,offset -> j target
            if di.opcode == mips_core.OP_BEQ and di.rs == 0 and di.rt == 0:
                let target_addr = i + 4 + int(di.imm * 4)
                let j_target = int(target_addr / 4)
                push(result, mips_core.encode_j(mips_core.OP_J, j_target))
                self.branches_optimized = self.branches_optimized + 1
                i = i + 1
            else:
                push(result, instr)
                i = i + 1
        self.opt_code = result
