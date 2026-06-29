#ifndef SAGEMIPS_H
#define SAGEMIPS_H

// ============================================================================
// SageMips — Freestanding MIPS32 VM + Assembler + Compiler
// ============================================================================
// Single ELF executable, libc-free, bare-metal capable.
// Uses fixed-size static pools for all allocations.
// Compile with: -ffreestanding -nostdlib -std=c11
// ============================================================================

// ---------------------------------------------------------------------------
// Integer types — freestanding safe (no <stdint.h> required on bare metal)
// ---------------------------------------------------------------------------
#ifdef SAGE_BARE_METAL
typedef signed   char      int8_t;
typedef unsigned char      uint8_t;
typedef signed   short     int16_t;
typedef unsigned short     uint16_t;
typedef signed   int       int32_t;
typedef unsigned int       uint32_t;
typedef signed   long long int64_t;
typedef unsigned long long uint64_t;
#else
#include <stdint.h>
#endif

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
#define MIPS_NUM_REGS          32      // $0-$31
#define MIPS_STACK_SIZE        4096    // 32-bit words
#define MIPS_HEAP_SIZE         (16*1024*1024) // 16 MB
#define MIPS_STRING_POOL       (256*1024)     // 256 KB
#define MIPS_MAX_CHUNKS        512
#define MIPS_CALL_DEPTH        256
#define MIPS_LABELS_MAX        1024
#define MIPS_CODE_MAX          (2*1024*1024)  // 2 MB code buffer

// ============================================================================
// MIPS32 Opcodes (6-bit primary)
// ============================================================================
#define MIPS_OP_SPECIAL     0x00    // R-type instructions
#define MIPS_OP_REGIMM      0x01    // BLTZ, BGEZ, etc.
#define MIPS_OP_J           0x02
#define MIPS_OP_JAL         0x03
#define MIPS_OP_BEQ         0x04
#define MIPS_OP_BNE         0x05
#define MIPS_OP_BLEZ        0x06
#define MIPS_OP_BGTZ        0x07
#define MIPS_OP_ADDI        0x08
#define MIPS_OP_ADDIU       0x09
#define MIPS_OP_SLTI        0x0A
#define MIPS_OP_SLTIU       0x0B
#define MIPS_OP_ANDI        0x0C
#define MIPS_OP_ORI         0x0D
#define MIPS_OP_XORI        0x0E
#define MIPS_OP_LUI         0x0F
#define MIPS_OP_COP0        0x10
#define MIPS_OP_COP1        0x11
#define MIPS_OP_COP2        0x12
#define MIPS_OP_BEQL        0x14
#define MIPS_OP_BNEL        0x15
#define MIPS_OP_BLEZL       0x16
#define MIPS_OP_BGTZL       0x17
#define MIPS_OP_SPECIAL2    0x1C    // MUL, MADD, etc.
#define MIPS_OP_JALX        0x1D
#define MIPS_OP_SPECIAL3    0x1F
#define MIPS_OP_LB          0x20
#define MIPS_OP_LH          0x21
#define MIPS_OP_LWL         0x22
#define MIPS_OP_LW          0x23
#define MIPS_OP_LBU         0x24
#define MIPS_OP_LHU         0x25
#define MIPS_OP_LWR         0x26
#define MIPS_OP_SB          0x28
#define MIPS_OP_SH          0x29
#define MIPS_OP_SWL         0x2A
#define MIPS_OP_SW          0x2B
#define MIPS_OP_SWR         0x2E

// ============================================================================
// SPECIAL (opcode 0x00) Function Codes
// ============================================================================
#define MIPS_FN_SLL         0x00
#define MIPS_FN_MOVCI       0x01    // (not implemented)
#define MIPS_FN_SRL         0x02
#define MIPS_FN_SRA         0x03
#define MIPS_FN_SLLV        0x04
#define MIPS_FN_SRLV        0x06
#define MIPS_FN_SRAV        0x07
#define MIPS_FN_JR          0x08
#define MIPS_FN_JALR        0x09
#define MIPS_FN_MOVZ        0x0A
#define MIPS_FN_MOVN        0x0B
#define MIPS_FN_SYSCALL     0x0C
#define MIPS_FN_BREAK       0x0D
#define MIPS_FN_SYNC        0x0F
#define MIPS_FN_MFHI        0x10
#define MIPS_FN_MTHI        0x11
#define MIPS_FN_MFLO        0x12
#define MIPS_FN_MTLO        0x13
#define MIPS_FN_MULT        0x18
#define MIPS_FN_MULTU       0x19
#define MIPS_FN_DIV         0x1A
#define MIPS_FN_DIVU        0x1B
#define MIPS_FN_ADD         0x20
#define MIPS_FN_ADDU        0x21
#define MIPS_FN_SUB         0x22
#define MIPS_FN_SUBU        0x23
#define MIPS_FN_AND         0x24
#define MIPS_FN_OR          0x25
#define MIPS_FN_XOR         0x26
#define MIPS_FN_NOR         0x27
#define MIPS_FN_SLT         0x2A
#define MIPS_FN_SLTU        0x2B
#define MIPS_FN_TGE         0x30
#define MIPS_FN_TGEU        0x31
#define MIPS_FN_TLT         0x32
#define MIPS_FN_TLTU        0x33
#define MIPS_FN_TEQ         0x34
#define MIPS_FN_TNE         0x36

// ============================================================================
// SPECIAL2 (opcode 0x1C) Function Codes
// ============================================================================
#define MIPS_FN_MUL         0x02

// ============================================================================
// REGIMM (opcode 0x01) rt field
// ============================================================================
#define MIPS_RT_BLTZ        0x00
#define MIPS_RT_BGEZ        0x01
#define MIPS_RT_BLTZL       0x02
#define MIPS_RT_BGEZL       0x03
#define MIPS_RT_BLTZAL      0x10
#define MIPS_RT_BGEZAL      0x11

// ============================================================================
// VM Syscall Numbers (used with MIPS SYSCALL instruction)
// ============================================================================
#define MIPS_SYSCALL_EXIT       0
#define MIPS_SYSCALL_PRINT_INT  1
#define MIPS_SYSCALL_PRINT_STR  2
#define MIPS_SYSCALL_READ_INT   3
#define MIPS_SYSCALL_READ_STR   4
#define MIPS_SYSCALL_SBRK       5
#define MIPS_SYSCALL_PRINT_CHAR 6
#define MIPS_SYSCALL_READ_CHAR  7
#define MIPS_SYSCALL_TIME       8
#define MIPS_SYSCALL_HALT       9

// ============================================================================
// Value Types (runtime-tagged values)
// ============================================================================
typedef enum {
    VAL_NIL = 0,
    VAL_NUM,        // 64-bit double
    VAL_INT,        // 32-bit signed integer
    VAL_UINT,       // 32-bit unsigned integer
    VAL_BOOL,
    VAL_PTR,        // Raw pointer
    VAL_STR,        // String (index into string pool)
} MipsValType;

typedef struct {
    MipsValType type;
    union {
        double   number;
        int32_t  i32;
        uint32_t u32;
        int32_t  boolean;
        void*    ptr;
        int32_t  str_idx;
        int64_t  raw;
    } as;
} MipsValue;

// ============================================================================
// Decoded MIPS32 Instruction
// ============================================================================
typedef struct {
    uint32_t raw;
    uint8_t  opcode;      // bits [31:26]
    uint8_t  rs;          // bits [25:21]
    uint8_t  rt;          // bits [20:16]
    uint8_t  rd;          // bits [15:11]
    uint8_t  shamt;       // bits [10:6]
    uint8_t  funct;       // bits [5:0]
    int16_t  imm;         // 16-bit signed immediate
    uint16_t uimm;        // 16-bit unsigned immediate
    uint32_t target;      // 26-bit jump target
} MipsInstr;

// ============================================================================
// VM State (large — must be heap-allocated on hosted platforms)
// ============================================================================
typedef struct {
    // Register file: 32 x 32-bit (values are stored in union form)
    MipsValue regs[MIPS_NUM_REGS];

    // Program counter (byte offset in current code)
    uint32_t pc;
    int      running;
    int      halted;
    int      error;

    // HI/LO special registers (for MULT/DIV)
    int64_t hi;
    int64_t lo;

    // Current code chunk being executed
    const uint8_t* code;
    uint32_t code_length;

    // Stack (32-bit words, grows down from top)
    uint32_t* stack;        // dynamic: MIPS_STACK_SIZE words
    uint32_t  sp;           // Stack pointer (word index)

    // Heap (simple bump allocator)
    uint8_t*  heap;         // dynamic: MIPS_HEAP_SIZE bytes
    uint32_t  heap_used;

    // String pool
    char*     strings;      // dynamic: MIPS_STRING_POOL bytes
    uint32_t  string_used;

    // Call stack
    struct {
        uint32_t return_addr;
        uint32_t saved_ra;
        uint32_t saved_fp;
    } call_stack[MIPS_CALL_DEPTH];
    int csp;

    // I/O callbacks (for bare-metal / embedded use)
    void (*write_char)(char c);
    int  (*read_char)(void);
    void (*write_str)(const char* s);

    // Status
    int debug;
    int trace;
    const char* error_msg;

    // Flag: owns dynamic memory (should free on destroy)
    int owns_memory;
} MipsVM;

// ============================================================================
// Assembler Types
// ============================================================================
typedef struct {
    char    name[64];
    uint32_t addr;
} MipsLabel;

typedef struct {
    uint8_t  code[MIPS_CODE_MAX];   // output binary buffer
    uint32_t code_len;              // bytes written
    MipsLabel labels[MIPS_LABELS_MAX];
    int      label_count;
    int      pass;                  // 0=first pass, 1=second pass
    uint32_t origin;                // base address
} MipsAsmState;

// ============================================================================
// Register Names (declared here, defined in mips_encode.c)
// ============================================================================
extern const char* mips_reg_names[32];

// ============================================================================
// Forward Declarations
// ============================================================================

// VM
MipsVM* mips_vm_create(void);
void    mips_vm_destroy(MipsVM* vm);
void    mips_vm_init(MipsVM* vm);
int     mips_vm_load(MipsVM* vm, const uint8_t* code, uint32_t len);
int     mips_vm_run(MipsVM* vm);
int     mips_vm_step(MipsVM* vm);

// Decode
MipsInstr mips_decode(uint32_t raw);

// Encode
uint32_t mips_encode_r(uint8_t funct, uint8_t rs, uint8_t rt, uint8_t rd, uint8_t shamt);
uint32_t mips_encode_r_special2(uint8_t funct, uint8_t rs, uint8_t rt, uint8_t rd);
uint32_t mips_encode_regimm(uint8_t rt, uint8_t rs, int16_t imm);
uint32_t mips_encode_i(uint8_t opcode, uint8_t rs, uint8_t rt, int16_t imm);
uint32_t mips_encode_j(uint8_t opcode, uint32_t target);

// Assembler
void mips_asm_init(MipsAsmState* st, uint32_t origin);
int  mips_asm_assemble_line(MipsAsmState* st, const char* line);
int  mips_asm_assemble(MipsAsmState* st, const char* source);
int  mips_asm_finalize(MipsAsmState* st);

// Disassembler
int  mips_disasm(char* buf, int buf_sz, uint32_t instr, uint32_t addr);

// Utility
int  mips_reg_from_name(const char* name);
const char* mips_reg_name(int reg);

// Freestanding libc replacements (bare-metal)
#ifndef SAGE_BARE_METAL
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#else
void* sm_memset(void* s, int c, unsigned long n);
void* sm_memcpy(void* d, const void* s, unsigned long n);
int   sm_memcmp(const void* s1, const void* s2, unsigned long n);
unsigned long sm_strlen(const char* s);
int   sm_strcmp(const char* s1, const char* s2);
char* sm_strcpy(char* d, const char* s);
char* sm_strncpy(char* d, const char* s, unsigned long n);
int   sm_snprintf(char* buf, unsigned long sz, const char* fmt, ...);

#define memset   sm_memset
#define memcpy   sm_memcpy
#define memcmp   sm_memcmp
#define strlen   sm_strlen
#define strcmp   sm_strcmp
#define strcpy   sm_strcpy
#define strncpy  sm_strncpy
#define snprintf sm_snprintf
#endif

#endif // SAGEMIPS_H
