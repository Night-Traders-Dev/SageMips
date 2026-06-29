# SageMips CLI — Command dispatcher (Sage)
# src/sage/cli.sage

import mips_core
import mips_vm
import mips_asm
import mips_aot
import mips_jit
import sys
import io

proc write_fn(s):
    print s

proc write_ch(s):
    print s

proc read_binary_file(path):
    let content = io.readfile(path)
    if content == nil:
        return []
    var data = []
    var i = 0
    while i < len(content):
        push(data, ord(content[i]))
        i = i + 1
    return data

proc read_text_file(path):
    let content = io.readfile(path)
    if content == nil:
        return ""
    return content

# ============================================================================
# Commands
# ============================================================================

proc cmd_run(args):
    if len(args) < 1:
        print "Usage: run <file> [--trace] [--jit] [--aot]"
        return 1

    let path = args[0]
    var trace = false
    var use_jit = false
    var use_aot = false
    var i = 1
    while i < len(args):
        if args[i] == "--trace":
            trace = true
        elif args[i] == "--jit":
            use_jit = true
        elif args[i] == "--aot":
            use_aot = true
        i = i + 1

    let code = read_binary_file(path)
    if len(code) == 0:
        print "Error: cannot read file: " + path
        return 1

    # AOT optimization
    if use_aot:
        let aot = mips_aot.MipsAOT()
        let opt_code = aot.optimize(code)
        var aot_msg = "AOT: " + str(len(opt_code)) + " bytes ("
        aot_msg = aot_msg + str(aot.nops_removed) + " NOPs, "
        aot_msg = aot_msg + str(aot.consts_folded) + " consts, "
        aot_msg = aot_msg + str(aot.branches_optimized) + " branches)"
        print aot_msg
        code = opt_code

    # Create VM
    let vm = mips_vm.MipsVM()
    vm.load(code)
    vm.trace = trace
    vm.write_str = write_fn
    vm.write_char = write_ch

    # JIT warmup
    var jit = nil
    if use_jit:
        jit = mips_jit.MipsJIT()
        jit.warmup(vm)
        let bbs = jit.stats()
        print "JIT: " + str(bbs) + " basic blocks"

    let ret = vm.run()

    # JIT stats on exit
    if jit != nil:
        let bbs = jit.stats()
        print "JIT stats: " + str(bbs) + " BBs"

    return ret

proc cmd_disassemble(args):
    if len(args) < 1:
        print "Usage: dis <file>"
        return 1

    let path = args[0]
    let code = read_binary_file(path)
    if len(code) == 0:
        print "Error: cannot read file: " + path
        return 1

    var addr = 0
    while addr + 4 <= len(code):
        let raw = mips_core.read_be32(code, addr)
        let line = mips_core.disasm(raw, addr)
        print line
        addr = addr + 4
    return 0

proc cmd_assemble(args):
    if len(args) < 1:
        print "Usage: asm <file.s>"
        return 1

    let path = args[0]
    let source = read_text_file(path)
    if source == "":
        print "Error: cannot read file: " + path
        return 1

    let code = mips_asm.assemble(source)
    if len(code) == 0:
        print "Error: assembly failed"
        return 1

    print "Assembled: " + str(len(code)) + " bytes"
    return 0

proc cmd_disasm(args):
    return cmd_disassemble(args)

proc cmd_help(args):
    print "SageMips (Sage) - MIPS32 VM + Assembler"
    print ""
    print "Commands:"
    print "  run     <file>         Run MIPS binary"
    print "  asm     <file.s>       Assemble MIPS assembly"
    print "  dis     <file>         Disassemble MIPS binary"
    print "  compile <file.sage>    Compile Sage -> MIPS (host only)"
    print "  emit    <file.sage>    Emit MIPS assembly from Sage"
    print "  Optimizations:"
    print "    --jit                 Enable JIT (instruction cache + basic blocks)"
    print "    --aot                 Enable AOT (NOP elimination, const fold)"
    print "    --trace               Show VM instruction trace"
    print "  --help                  Show this help"
    print "  --version               Show version"

proc cmd_version(args):
    print "SageMips (Sage) v1.0.0"

# ============================================================================
# Entry Point
# ============================================================================

proc dispatch():
    let args = sys.args()
    if len(args) < 2:
        cmd_help([])
        return 1

    let cmd = args[1]
    var rest = args[2:]
    var save_asm = false

    var i = 0
    while i < len(rest):
        if rest[i] == "--save-asm":
            save_asm = true
            rest = rest[0:i] + rest[i+1:]
        elif rest[i] == "--trace" or rest[i] == "--jit" or rest[i] == "--aot":
            i = i + 1
        else:
            i = i + 1

    if cmd == "run":
        return cmd_run(rest)
    elif cmd == "asm" or cmd == "assemble":
        return cmd_assemble(rest)
    elif cmd == "dis" or cmd == "disasm":
        return cmd_disasm(rest)
    elif cmd == "compile" or cmd == "emit":
        print "Sage compile uses the host compiler:"
        if cmd == "emit" or save_asm:
            print "  sage --emit-asm --target mips <file.sage> -o <file.s>"
        else:
            print "  sage --emit-asm --target mips <file.sage> -o <file.s>"
            print "  Then assemble with: sagemips asm <file.s>"
        return 0
    elif cmd == "--help" or cmd == "-h":
        cmd_help([])
        return 0
    elif cmd == "--version" or cmd == "-V":
        cmd_version([])
        return 0
    else:
        print "Unknown command: " + cmd
        cmd_help([])
        return 1
