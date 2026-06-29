#include "sagemips.h"

#ifdef SAGE_BARE_METAL
// ============================================================================
// Freestanding libc replacements
// ============================================================================
void* sm_memset(void* s, int c, unsigned long n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}
void* sm_memcpy(void* d, const void* s, unsigned long n) {
    unsigned char* dp = (unsigned char*)d;
    const unsigned char* sp = (const unsigned char*)s;
    while (n--) *dp++ = *sp++;
    return d;
}
int sm_memcmp(const void* s1, const void* s2, unsigned long n) {
    const unsigned char* a = (const unsigned char*)s1;
    const unsigned char* b = (const unsigned char*)s2;
    while (n--) { if (*a != *b) return *a - *b; a++; b++; }
    return 0;
}
unsigned long sm_strlen(const char* s) {
    unsigned long n = 0;
    while (*s++) n++;
    return n;
}
int sm_strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
char* sm_strcpy(char* d, const char* s) {
    char* orig = d;
    while ((*d++ = *s++));
    return orig;
}
char* sm_strncpy(char* d, const char* s, unsigned long n) {
    unsigned long i;
    for (i = 0; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = '\0';
    return d;
}
int sm_snprintf(char* buf, unsigned long sz, const char* fmt, ...) {
    if (!buf || !sz) return 0;
    unsigned long pos = 0;
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    while (*fmt && pos < sz - 1) {
        if (*fmt != '%') { buf[pos++] = *fmt++; continue; }
        fmt++;
        if (*fmt == '%') { buf[pos++] = '%'; fmt++; continue; }
        if (*fmt == 's') {
            const char* s = __builtin_va_arg(args, const char*);
            if (!s) s = "(null)";
            while (*s && pos < sz - 1) buf[pos++] = *s++;
            fmt++;
        } else if (*fmt == 'd' || *fmt == 'i') {
            int val = __builtin_va_arg(args, int);
            if (val < 0) { buf[pos++] = '-'; val = -val; }
            int digs[12], nd = 0;
            if (val == 0) digs[nd++] = 0;
            else { while (val) { digs[nd++] = val % 10; val /= 10; } }
            while (nd-- && pos < sz - 1) buf[pos++] = '0' + digs[nd];
            fmt++;
        } else if (*fmt == 'u') {
            unsigned val = __builtin_va_arg(args, unsigned);
            int digs[12], nd = 0;
            if (val == 0) digs[nd++] = 0;
            else { while (val) { digs[nd++] = val % 10; val /= 10; } }
            while (nd-- && pos < sz - 1) buf[pos++] = '0' + digs[nd];
            fmt++;
        } else if (*fmt == 'c') {
            char c = (char)__builtin_va_arg(args, int);
            buf[pos++] = c;
            fmt++;
        } else if (*fmt == 'x' || *fmt == 'X') {
            unsigned val = __builtin_va_arg(args, unsigned);
            int digs[10], nd = 0;
            if (val == 0) digs[nd++] = 0;
            else { while (val) { digs[nd++] = val & 0xF; val >>= 4; } }
            while (nd-- && pos < sz - 1) {
                int d = digs[nd];
                buf[pos++] = d < 10 ? '0' + d : 'a' + (d - 10);
            }
            fmt++;
        } else {
            buf[pos++] = *fmt++;
        }
    }
    buf[pos] = '\0';
    __builtin_va_end(args);
    return (int)pos;
}
#endif

// ============================================================================
// I/O helpers
// ============================================================================
static void print_str(const char* s) {
#ifdef SAGE_BARE_METAL
    (void)s;
#else
    fputs(s, stdout);
#endif
}

static void print_int(int n) {
#ifndef SAGE_BARE_METAL
    printf("%d", n);
#else
    (void)n;
#endif
}

// ============================================================================
// Syscall output callback (used by VM print syscalls)
// ============================================================================
#ifndef SAGE_BARE_METAL
static void vm_write_str_cb(const char* s) {
    fputs(s, stdout);
}
static void vm_write_char_cb(char c) {
    fputc(c, stdout);
}
#endif
static void usage(const char* prog) {
    print_str("SageMips - MIPS32 VM + Assembler + Compiler\n");
    print_str("Usage:\n");
    print_str("  "); print_str(prog); print_str(" run     <file.mips>     Run MIPS binary\n");
    print_str("  "); print_str(prog); print_str(" asm     <file.s> [-o out] Assemble MIPS assembly\n");
    print_str("  "); print_str(prog); print_str(" dis     <file>          Disassemble MIPS binary\n");
    print_str("  "); print_str(prog); print_str(" emit    <file.sage> [-o out.s]  Emit MIPS assembly\n");
    print_str("  "); print_str(prog); print_str(" compile <file.sage> [-o out.s]  Compile Sage -> MIPS\n");
    print_str("  "); print_str(prog); print_str(" cc      <file.c>        Compile C -> MIPS binary\n");
    print_str("  Optimizations:\n");
    print_str("    --jit                 Enable JIT (instruction cache + basic blocks)\n");
    print_str("    --aot                 Enable AOT (NOP elimination, const fold, branch opt)\n");
    print_str("    --trace               Show VM instruction trace\n");
    print_str("    -o <file>             Output file path\n");
    print_str("  "); print_str(prog); print_str(" --help                 Show this help\n");
    print_str("  "); print_str(prog); print_str(" --version              Show version\n");
}

// ============================================================================
// File I/O Utility
// ============================================================================
#ifndef SAGE_BARE_METAL

static uint8_t* read_file(const char* path, uint32_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    uint8_t* buf = (uint8_t*)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    *out_len = (uint32_t)rd;
    return buf;
}

static char* read_text_file(const char* path, uint32_t* out_len) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    *out_len = (uint32_t)rd;
    return buf;
}

static int write_file(const char* path, const uint8_t* data, uint32_t len) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(data, 1, len, f);
    fclose(f);
    return 0;
}

#endif

// ============================================================================
// Handle Commands
// ============================================================================

static int cmd_run(const char* path, int trace, int use_jit, int use_aot) {
#ifndef SAGE_BARE_METAL
    // Check file extension for common mistakes
    const char* ext = strrchr(path, '.');
    if (ext) {
        if (strcmp(ext, ".sage") == 0) {
            print_str("Error: cannot run .sage source directly\n");
            print_str("  First compile: sagemips emit ");
            print_str(path);
            print_str("\n  Then assemble:  sagemips asm <file.s>\n");
            print_str("  Then run:      sagemips run <file.mips>\n");
            return 1;
        }
        if (strcmp(ext, ".s") == 0 || strcmp(ext, ".asm") == 0) {
            print_str("Error: cannot run assembly source directly\n");
            print_str("  First assemble: sagemips asm ");
            print_str(path);
            print_str("\n  Then run:      sagemips run <file.mips>\n");
            return 1;
        }
        if (strcmp(ext, ".c") == 0) {
            print_str("Error: cannot run C source directly\n");
            print_str("  First compile: sagemips cc ");
            print_str(path);
            print_str("\n  Then run:      sagemips run <file.mips>\n");
            return 1;
        }
    }

    uint32_t len;
    uint8_t* code = read_file(path, &len);
    if (!code) { print_str("Error: cannot read file\n"); return 1; }

    // Sanity check
    if (len >= 4) {
        int is_text = 1;
        for (uint32_t i = 0; i < (len < 256 ? len : 256); i++) {
            if (code[i] == 0) { is_text = 0; break; }
        }
        if (is_text && (code[0] == '.' || code[0] == '#' || code[0] == '/' || code[0] == ';')) {
            print_str("Error: file appears to be assembly source, not a MIPS binary\n");
            print_str("  Assemble first: sagemips asm ");
            print_str(path);
            print_str(" -o <output.mips>\n");
            free(code);
            return 1;
        }
    }

    MipsVM* vm = mips_vm_create();
    if (!vm) { free(code); print_str("Error: failed to create VM\n"); return 1; }
    vm->trace = trace;
    vm->write_str = vm_write_str_cb;
    vm->write_char = vm_write_char_cb;

    // AOT optimization — pre-process code before loading
    MipsAOTState aot_state;
    MipsAOTState* aot_ptr = NULL;
    if (use_aot) {
        mips_aot_init(&aot_state);
        int aot_len = mips_aot_optimize(&aot_state, code, len);
        print_str("AOT: ");
        print_int(aot_len);
        print_str(" bytes optimized (");
        print_int((int)aot_state.nops_removed);
        print_str(" NOPs, ");
        print_int((int)aot_state.consts_folded);
        print_str(" consts, ");
        print_int((int)aot_state.branches_optimized);
        print_str(" branches, ");
        print_int((int)aot_state.dead_removed);
        print_str(" dead)\n");

        vm->aot = &aot_state;
        vm->orig_code = code;
        vm->orig_code_length = len;
        // Use optimized code
        mips_vm_load(vm, aot_state.opt_code, (uint32_t)aot_len);
    } else {
        mips_vm_load(vm, code, len);
    }

    // JIT warmup
    MipsJITState jit_state;
    if (use_jit) {
        mips_jit_init(&jit_state, vm);
        mips_jit_warmup(&jit_state, vm);
        vm->jit = &jit_state;
        int cache_entries, bb_count;
        mips_jit_stats(&jit_state, &cache_entries, &bb_count);
        print_str("JIT: ");
        print_int(cache_entries);
        print_str(" cached instructions, ");
        print_int(bb_count);
        print_str(" basic blocks\n");
    }

    int ret = mips_vm_run(vm);
    if (vm->error) {
        print_str("VM Error: ");
        print_str(vm->error_msg ? vm->error_msg : "unknown");
        print_str("\n");
    }

    // JIT stats on exit
    if (use_jit && vm->jit) {
        int entries, bbs;
        mips_jit_stats(vm->jit, &entries, &bbs);
        print_str("JIT stats: ");
        print_int(entries);
        print_str(" cache entries, ");
        print_int(bbs);
        print_str(" BBs\n");
    }

    free(code);
    mips_vm_destroy(vm);
    return ret;
#else
    (void)path; (void)trace; (void)use_jit; (void)use_aot;
    return 1;
#endif
}

static int cmd_disassemble(const char* path) {
#ifndef SAGE_BARE_METAL
    uint32_t len;
    uint8_t* code = read_file(path, &len);
    if (!code) { print_str("Error: cannot read file\n"); return 1; }

    uint32_t addr = 0;
    while (addr + 4 <= len) {
        uint32_t raw = ((uint32_t)code[addr] << 24) |
                       ((uint32_t)code[addr+1] << 16) |
                       ((uint32_t)code[addr+2] << 8) |
                       (uint32_t)code[addr+3];
        char buf[128];
        mips_disasm(buf, sizeof(buf), raw, addr);
        print_str(buf);
        print_str("\n");
        addr += 4;
    }
    free(code);
    return 0;
#else
    (void)path;
    return 1;
#endif
}

static int cmd_assemble(const char* input, const char* output) {
#ifndef SAGE_BARE_METAL
    uint32_t len;
    char* source = read_text_file(input, &len);
    if (!source) { print_str("Error: cannot read input file\n"); return 1; }

    MipsAsmState st;
    mips_asm_init(&st, 0);

    int ret = mips_asm_assemble(&st, source);
    free(source);

    if (ret < 0) {
        print_str("Error: assembly failed\n");
        return 1;
    }

    mips_asm_finalize(&st);

    char out_buf[256];
    if (!output) {
        const char* base = input;
        const char* ext = strrchr(base, '.');
        int base_len = ext ? (int)(ext - base) : (int)strlen(base);
        memcpy(out_buf, base, base_len);
        memcpy(out_buf + base_len, ".mips", 6);
        output = out_buf;
    }

    if (write_file(output, st.code, st.code_len) < 0) {
        print_str("Error: cannot write output file\n");
        return 1;
    }

    print_str("Assembled: ");
    print_str(output);
    print_str(" (");
    print_int((int)st.code_len);
    print_str(" bytes)\n");
    return 0;
#else
    (void)input; (void)output;
    return 1;
#endif
}

static int assemble_with_mips_tools(const char* asm_path) {
    // Try to assemble MIPS asm -> flat binary using cross-tools
    char cmd[512];
    char obj_path[256];
    char bin_path[256];

    int asm_len = (int)strlen(asm_path);
    const char* ext = strrchr(asm_path, '.');
    int base_len = ext ? (int)(ext - asm_path) : asm_len;
    memcpy(obj_path, asm_path, base_len);
    memcpy(obj_path + base_len, ".o", 3);
    obj_path[base_len + 2] = '\0';
    memcpy(bin_path, asm_path, base_len);
    memcpy(bin_path + base_len, ".mips", 6);
    bin_path[base_len + 5] = '\0';

    // Attempt 1: mips-linux-gnu-as -> objcopy (best for simple asm without relocations)
    snprintf(cmd, sizeof(cmd),
        "mips-linux-gnu-as -march=mips32 -o \"%s\" \"%s\" 2>/dev/null && "
        "mips-linux-gnu-objcopy -O binary \"%s\" \"%s\" 2>/dev/null",
        obj_path, asm_path, obj_path, bin_path);
    if (system(cmd) == 0) return 0;

    // Attempt 2: as -> ld with unresolved symbols ignored (for Sage-generated code
    // that references sage_rt_* functions — they become address 0, VM will halt on them)
    snprintf(cmd, sizeof(cmd),
        "mips-linux-gnu-as -march=mips32 -o \"%s\" \"%s\" 2>/dev/null && "
        "mips-linux-gnu-ld -Ttext=0x0 --unresolved-symbols=ignore-all "
        "-o \"%s.elf\" \"%s\" 2>/dev/null && "
        "mips-linux-gnu-objcopy -O binary \"%s.elf\" \"%s\" 2>/dev/null",
        obj_path, asm_path, obj_path, obj_path, bin_path);
    if (system(cmd) == 0) return 0;

    // Attempt 3: mipsel variant
    snprintf(cmd, sizeof(cmd),
        "mipsel-linux-gnu-as -march=mips32 -o \"%s\" \"%s\" 2>/dev/null && "
        "mipsel-linux-gnu-objcopy -O binary \"%s\" \"%s\" 2>/dev/null",
        obj_path, asm_path, obj_path, bin_path);
    if (system(cmd) == 0) return 0;

    return -1;
}

static int cmd_compile_sage(const char* input, const char* output_asm, int save_asm_only) {
#ifndef SAGE_BARE_METAL
    // Validate input is a .sage file
    const char* ext = strrchr(input, '.');
    if (!ext || (strcmp(ext, ".sage") != 0)) {
        print_str("Error: input must be a .sage file\n");
        print_str("  For .s files, use: sagemips asm <file.s>\n");
        print_str("  For .mips files, use: sagemips run <file.mips>\n");
        return 1;
    }

    char out_buf[256];
    if (output_asm) {
        int out_len = (int)strlen(output_asm);
        if (out_len >= 254) return 1;
        memcpy(out_buf, output_asm, out_len + 1);
    } else if (save_asm_only) {
        const char* base = input;
        int base_len = (int)(ext - base);
        memcpy(out_buf, base, base_len);
        memcpy(out_buf + base_len, ".s", 3);
        out_buf[base_len + 2] = '\0';
    } else {
        memcpy(out_buf, "/tmp/sagemips_asm_", 18);
        const char* bn = input;
        const char* sl = strrchr(bn, '/');
        if (sl) bn = sl + 1;
        int bn_len = (int)(ext - bn);
        memcpy(out_buf + 18, bn, bn_len);
        memcpy(out_buf + 18 + bn_len, ".s", 3);
        out_buf[18 + bn_len + 3] = '\0';
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "sage --emit-asm \"%s\" --target mips -o \"%s\" 2>&1",
             input, out_buf);
    print_str("Compiling Sage -> MIPS asm...\n");
    int ret = system(cmd);
    if (ret != 0) {
        print_str("Error: sage compilation failed\n");
        return 1;
    }
    print_str("  Assembly saved: ");
    print_str(out_buf);
    print_str("\n");

    if (save_asm_only) {
        return 0;
    }

    // Try MIPS cross-assembler first (handles Sage-generated GNU-as syntax)
    print_str("Assembling MIPS -> binary...\n");
    ret = assemble_with_mips_tools(out_buf);
    if (ret == 0) {
        // Build output binary path
        int asm_len = (int)strlen(out_buf);
        const char* asm_ext = strrchr(out_buf, '.');
        int base_len = asm_ext ? (int)(asm_ext - out_buf) : asm_len;
        char bin_path[256];
        memcpy(bin_path, out_buf, base_len);
        memcpy(bin_path + base_len, ".mips", 6);
        bin_path[base_len + 5] = '\0';
        print_str("  Binary saved: ");
        print_str(bin_path);
        print_str("\n");
        return 0;
    }

    // Fall back to our simple assembler (works for hand-written asm)
    ret = cmd_assemble(out_buf, NULL);
    if (ret != 0) {
        print_str("  Note: Sage-generated assembly uses GNU-as syntax (%hi, %lo, .double)\n");
        print_str("  To assemble, install MIPS cross-tools:\n");
        print_str("    sudo apt install binutils-mips-linux-gnu\n");
        print_str("  Then re-run: sagemips compile ");
        print_str(input);
        print_str("\n");
    }
    return ret;
#else
    (void)input; (void)output_asm; (void)save_asm_only;
    return 1;
#endif
}

static int cmd_compile_c(const char* input, int save_asm) {
#ifndef SAGE_BARE_METAL
    char out_buf[256];
    char asm_buf[256];
    const char* base = input;
    const char* ext = strrchr(base, '.');
    if (!ext || strcmp(ext, ".c") != 0) {
        print_str("Error: input must be a .c file\n");
        return 1;
    }

    int base_len = (int)(ext - base);
    memcpy(out_buf, base, base_len);
    memcpy(out_buf + base_len, ".mips", 6);
    memcpy(asm_buf, base, base_len);
    memcpy(asm_buf + base_len, ".s", 3);
    asm_buf[base_len + 2] = '\0';

    // Try MIPS cross-compiler with multiple toolchain prefixes
    const char* prefixes[] = {
        "mips-linux-gnu-",
        "mipsel-linux-gnu-",
        "mips-elf-",
        "mips64-linux-gnu-",
        NULL
    };

    int ok = 0;
    char cmd[512];

    if (save_asm) {
        // Generate assembly first, then binary
        for (int i = 0; prefixes[i] && !ok; i++) {
            snprintf(cmd, sizeof(cmd),
                "%sgcc -S -nostdlib -march=mips32 \"%s\" -o \"%s\" 2>/dev/null",
                prefixes[i], input, asm_buf);
            if (system(cmd) == 0) ok = 1;
        }
        if (!ok) goto cc_fail;

        print_str("  Assembly saved: ");
        print_str(asm_buf);
        print_str("\n");

        // Assemble to binary
        ok = 0;
        for (int i = 0; prefixes[i] && !ok; i++) {
            snprintf(cmd, sizeof(cmd),
                "%sgcc -nostdlib -march=mips32 \"%s\" -o \"%s\" 2>/dev/null",
                prefixes[i], asm_buf, out_buf);
            if (system(cmd) == 0) ok = 1;
        }
        if (!ok) goto cc_fail;

        print_str("  Binary saved: ");
        print_str(out_buf);
        print_str("\n");
        return 0;
    } else {
        // Direct binary compilation
        for (int i = 0; prefixes[i] && !ok; i++) {
            snprintf(cmd, sizeof(cmd),
                "%sgcc -nostdlib -march=mips32 \"%s\" -o \"%s\" 2>/dev/null",
                prefixes[i], input, out_buf);
            if (system(cmd) == 0) ok = 1;
        }
        if (!ok) goto cc_fail;

        print_str("Compiled: ");
        print_str(out_buf);
        print_str("\n");
        return 0;
    }

cc_fail:
    print_str("Error: no MIPS cross-compiler found\n");
    print_str("  Install one of:\n");
    print_str("    sudo apt install gcc-mips-linux-gnu\n");
    print_str("    sudo apt install gcc-mipsel-linux-gnu\n");
    print_str("  Or use Sage: sagemips emit <file.sage>\n");
    return 1;
#else
    (void)input; (void)save_asm;
    return 1;
#endif
}

// ============================================================================
// Main Entry Point
// ============================================================================
#ifndef SAGE_BARE_METAL

int main(int argc, char** argv) {
    const char* prog = argv[0] ? argv[0] : "sagemips";

    if (argc < 2) {
        usage(prog);
        return 1;
    }

    const char* cmd = argv[1];
    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        usage(prog);
        return 0;
    }
    if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "-V") == 0) {
        print_str("SageMips v1.0.0\n");
        return 0;
    }
    if (strcmp(cmd, "run") == 0) {
        if (argc < 3) { print_str("Usage: "); print_str(prog); print_str(" run <file.mips> [--trace] [--jit] [--aot]\n"); return 1; }
        int trace = 0, use_jit = 0, use_aot = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--trace") == 0) trace = 1;
            if (strcmp(argv[i], "--jit") == 0) use_jit = 1;
            if (strcmp(argv[i], "--aot") == 0) use_aot = 1;
        }
        return cmd_run(argv[2], trace, use_jit, use_aot);
    }
    if (strcmp(cmd, "dis") == 0 || strcmp(cmd, "disasm") == 0) {
        if (argc < 3) { print_str("Usage: "); print_str(prog); print_str(" dis <file>\n"); return 1; }
        return cmd_disassemble(argv[2]);
    }
    if (strcmp(cmd, "asm") == 0 || strcmp(cmd, "assemble") == 0) {
        if (argc < 3) { print_str("Usage: "); print_str(prog); print_str(" asm <file.s> [-o <out>]\n"); return 1; }
        const char* out = NULL;
        for (int i = 3; i < argc - 1; i++) {
            if (strcmp(argv[i], "-o") == 0) { out = argv[i + 1]; break; }
        }
        return cmd_assemble(argv[2], out);
    }
    if (strcmp(cmd, "compile") == 0 || strcmp(cmd, "emit") == 0) {
        if (argc < 3) { print_str("Usage: "); print_str(prog); print_str(" "); print_str(cmd); print_str(" <file.sage> [-o <out.s>] [--save-asm]\n"); return 1; }
        int save_asm = (strcmp(cmd, "emit") == 0);
        const char* out = NULL;
        for (int i = 3; i < argc - 1; i++) {
            if (strcmp(argv[i], "-o") == 0) { out = argv[i + 1]; break; }
            if (strcmp(argv[i], "--save-asm") == 0) save_asm = 1;
        }
        return cmd_compile_sage(argv[2], out, save_asm);
    }
    if (strcmp(cmd, "cc") == 0) {
        if (argc < 3) { print_str("Usage: "); print_str(prog); print_str(" cc <file.c> [-o <out>] [--save-asm]\n"); return 1; }
        int save_asm = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--save-asm") == 0) save_asm = 1;
        }
        return cmd_compile_c(argv[2], save_asm);
    }

    print_str("Unknown command: ");
    print_str(cmd);
    print_str("\n");
    usage(prog);
    return 1;
}

#else

void _start(void) {
    MipsVM vm;
    mips_vm_init(&vm);
    while (1) {
        __asm__ volatile ("hlt" ::: "memory");
    }
}

#endif
