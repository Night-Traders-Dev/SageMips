# SageMips JIT Optimizer (Sage) — Instruction cache + basic block profiling
# src/sage/mips_jit.sage

import mips_core

class MipsJIT:
    proc init(self):
        self.cache = {}         # pc -> decoded MipsInstr
        self.blocks = []        # list of basic blocks
        self.block_map = {}     # start_addr -> block index
        self.hot_threshold = 50
        self.total_steps = 0
        self.enabled = true

    proc warmup(self, vm):
        if not self.enabled:
            return
        let code = vm.code
        var pc = 0
        var current_bb_start = 0

        # First pass: cache all instructions
        while pc + 4 <= vm.code_len:
            let raw = mips_core.read_be32(vm.code, pc)
            let instr = mips_core.MipsInstr(raw)
            self.cache[str(pc)] = instr
            pc = pc + 4

        # Second pass: build basic blocks
        pc = 0
        while pc + 4 <= vm.code_len:
            var bb_start = pc
            var bb_end = pc
            # Scan to end of basic block
            while pc + 4 <= vm.code_len:
                let raw = mips_core.read_be32(vm.code, pc)
                let instr = mips_core.MipsInstr(raw)
                let op = instr.opcode
                let fn = instr.funct
                pc = pc + 4
                # Check if this is a branch
                var is_br = false
                if op == mips_core.OP_J or op == mips_core.OP_JAL:
                    is_br = true
                elif op == mips_core.OP_BEQ or op == mips_core.OP_BNE:
                    is_br = true
                elif op == mips_core.OP_BLEZ or op == mips_core.OP_BGTZ:
                    is_br = true
                elif op == mips_core.OP_REGIMM:
                    is_br = true
                elif op == mips_core.OP_SPECIAL:
                    if fn == mips_core.FN_JR or fn == mips_core.FN_JALR:
                        is_br = true
                if is_br:
                    # Include delay slot
                    if pc + 4 <= vm.code_len:
                        pc = pc + 4
                    break
            bb_end = pc
            let bb = {"start": bb_start, "end": bb_end, "hits": 0,
                       "next": bb_end, "branch": -1}
            push(self.blocks, bb)
            self.block_map[str(bb_start)] = len(self.blocks) - 1

    proc hit(self, pc):
        # Record a basic block execution
        if dict_has(self.block_map, str(pc)):
            let idx = self.block_map[str(pc)]
            self.blocks[idx]["hits"] = self.blocks[idx]["hits"] + 1

    proc stats(self):
        return len(self.blocks)
