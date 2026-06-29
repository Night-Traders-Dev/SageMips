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
    print_str("  Common flags:\n");
    print_str("    -o <file>             Output file path\n");
    print_str("    --save-asm            Save intermediate assembly\n");
    print_str("    --trace               Show VM instruction trace\n");
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

static int cmd_run(const char* path, int trace) {
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

    // Sanity check: if file looks like text, warn
    if (len >= 4) {
        int is_text = 1;
        for (uint32_t i = 0; i < (len < 256 ? len : 256); i++) {
            if (code[i] == 0) { is_text = 0; break; }  // binary has nulls
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
#ifndef SAGE_BARE_METAL
    vm->write_str = vm_write_str_cb;
    vm->write_char = vm_write_char_cb;
#endif

    if (mips_vm_load(vm, code, len) < 0) {
        print_str("Error: failed to load binary\n");
        free(code);
        mips_vm_destroy(vm);
        return 1;
    }

    int ret = mips_vm_run(vm);
    if (vm->error) {
        print_str("VM Error: ");
        print_str(vm->error_msg ? vm->error_msg : "unknown");
        print_str("\n");
    }
    free(code);
    mips_vm_destroy(vm);
    return ret;
#else
    (void)path; (void)trace;
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
        // Use explicit output path
        int out_len = (int)strlen(output_asm);
        if (out_len >= 254) return 1;
        memcpy(out_buf, output_asm, out_len + 1);
    } else if (save_asm_only) {
        // Auto-generate: replace .sage with .s in same directory
        const char* base = input;
        int base_len = (int)(ext - base);
        memcpy(out_buf, base, base_len);
        memcpy(out_buf + base_len, ".s", 3);
        out_buf[base_len + 2] = '\0';
    } else {
        // Temporary file in /tmp
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

    print_str("Assembling MIPS -> binary...\n");
    ret = cmd_assemble(out_buf, NULL);
    if (ret != 0) {
        print_str("  Note: Sage-generated asm uses GNU-as syntax.\n");
        print_str("  Assemble externally: mips-linux-gnu-as ");
        print_str(out_buf);
        print_str(" -o output.o\n");
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
    int base_len = ext ? (int)(ext - base) : (int)strlen(base);
    memcpy(out_buf, base, base_len);
    memcpy(out_buf + base_len, ".mips", 6);
    memcpy(asm_buf, base, base_len);
    memcpy(asm_buf + base_len, ".s", 3);
    asm_buf[base_len + 2] = '\0';

    if (save_asm) {
        // Generate MIPS assembly first
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "mips-linux-gnu-gcc -S -nostdlib -march=mips32 \"%s\" -o \"%s\" 2>&1",
                 input, asm_buf);
        print_str("Compiling C -> MIPS asm...\n");
        int ret = system(cmd);
        if (ret == 0) {
            print_str("  Assembly saved: ");
            print_str(asm_buf);
            print_str("\n");

            // Now assemble the .s to .mips
            print_str("Assembling MIPS -> binary...\n");
            ret = cmd_assemble(asm_buf, out_buf);
            if (ret == 0) {
                print_str("  Binary saved: ");
                print_str(out_buf);
                print_str("\n");
            }
            return ret;
        } else {
            print_str("Error: C compilation failed (is a MIPS cross-compiler installed?)\n");
            print_str("Install: apt install gcc-mips-linux-gnu\n");
            return 1;
        }
    } else {
        // Direct binary compilation
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "mips-linux-gnu-gcc -static -nostdlib -march=mips32 \"%s\" -o \"%s\" 2>&1",
                 input, out_buf);
        print_str("Compiling C -> MIPS...\n");
        int ret = system(cmd);
        if (ret != 0) {
            print_str("Error: C compilation failed (is a MIPS cross-compiler installed?)\n");
            print_str("Install: apt install gcc-mips-linux-gnu\n");
            return 1;
        }
        print_str("Compiled: ");
        print_str(out_buf);
        print_str("\n");
        return 0;
    }
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
        if (argc < 3) { print_str("Usage: "); print_str(prog); print_str(" run <file.mips> [--trace]\n"); return 1; }
        int trace = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--trace") == 0) trace = 1;
        }
        return cmd_run(argv[2], trace);
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
