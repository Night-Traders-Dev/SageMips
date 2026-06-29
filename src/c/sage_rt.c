#include "sagemips.h"

// ============================================================================
// Sage Runtime Stubs — Implement sage_rt_* functions as VM builtins
// ============================================================================
// The Sage compiler generates MIPS calls to sage_rt_print, sage_rt_string,
// sage_rt_number, sage_rt_add/sub/mul/div, sage_rt_eq/neq/lt/gt etc.
// These operate on tagged Sage Value types (double-precision numbers with
// type tags). We implement simplified stubs that handle basic cases.
// ============================================================================

// Sage runtime Value type tags (simplified)
#define SAGE_VAL_NUMBER  1
#define SAGE_VAL_STRING  3
#define SAGE_VAL_BOOL    2
#define SAGE_VAL_NIL     0

// A Sage Value is stored as two 32-bit words (upper, lower bits of double)
// We store them in the VM stack as consecutive words at a given offset.

#ifndef SAGE_BARE_METAL
#include <stdio.h>
#endif

// Extract Sage value type from 2-word stack entry
static int sage_get_type(MipsVM* vm, uint32_t sp_offset) {
    // Type is encoded in the upper bits of the value
    // Simplified: check if it looks like a number (has exponent bits set)
    uint32_t hi = vm->stack[sp_offset / 4];
    // NaN-boxing detection for strings: if upper 16 bits are 0x7FF8
    if ((hi >> 16) == 0x7FF8) return SAGE_VAL_STRING;
    if (hi == 0 && vm->stack[sp_offset/4 + 1] == 0) return SAGE_VAL_NIL;
    return SAGE_VAL_NUMBER;
}

// Print a Sage value at sp_offset
void sage_rt_print(MipsVM* vm, uint32_t sp_offset) {
#ifndef SAGE_BARE_METAL
    int type = sage_get_type(vm, sp_offset);
    if (type == SAGE_VAL_NUMBER) {
        union { double d; uint32_t w[2]; } u;
        u.w[1] = vm->stack[sp_offset / 4];     // high word
        u.w[0] = vm->stack[sp_offset / 4 + 1]; // low word
        if (u.d == (double)(long long)u.d && u.d > -1e15 && u.d < 1e15) {
            fprintf(stdout, "%lld", (long long)u.d);
        } else {
            fprintf(stdout, "%g", u.d);
        }
    } else if (type == SAGE_VAL_STRING) {
        uint32_t idx = vm->stack[sp_offset / 4 + 1] & 0xFFFF;
        if (idx < vm->string_used) {
            fprintf(stdout, "%s", &vm->strings[idx]);
        }
    } else {
        fprintf(stdout, "nil");
    }
#else
    (void)vm; (void)sp_offset;
#endif
}

// Create a string Value from a string constant
void sage_rt_string(MipsVM* vm, uint32_t dest_offset, const char* str, int len) {
    // Intern the string
    int idx = 0;
    // Search
    while (idx < (int)vm->string_used) {
        const char* existing = &vm->strings[idx];
        int elen = 0; while (existing[elen]) elen++;
        if (elen == len) {
            int match = 1;
            for (int i = 0; i < len; i++) if (existing[i] != str[i]) { match = 0; break; }
            if (match) break;
        }
        idx += elen + 1;
    }
    if (idx >= (int)vm->string_used) {
        // Append new string
        if (vm->string_used + len + 1 <= MIPS_STRING_POOL) {
            int pos = vm->string_used;
            for (int i = 0; i < len; i++) vm->strings[pos + i] = str[i];
            vm->strings[pos + len] = '\0';
            vm->string_used += len + 1;
            idx = pos;
        }
    }
    // Write NaN-boxed string value
    vm->stack[dest_offset / 4]     = 0x7FF80000; // NaN box
    vm->stack[dest_offset / 4 + 1] = (uint32_t)idx;
}

// Create a number Value
void sage_rt_number(MipsVM* vm, uint32_t dest_offset, double val) {
    union { double d; uint32_t w[2]; } u;
    u.d = val;
    vm->stack[dest_offset / 4]     = u.w[1]; // high word
    vm->stack[dest_offset / 4 + 1] = u.w[0]; // low word
}

// Arithmetic operations on Sage Values
static double sage_get_number(MipsVM* vm, uint32_t offset) {
    union { double d; uint32_t w[2]; } u;
    u.w[1] = vm->stack[offset / 4];
    u.w[0] = vm->stack[offset / 4 + 1];
    return u.d;
}

void sage_rt_add(MipsVM* vm, uint32_t dest, uint32_t a_off, uint32_t b_off) {
    sage_rt_number(vm, dest, sage_get_number(vm, a_off) + sage_get_number(vm, b_off));
}
void sage_rt_sub(MipsVM* vm, uint32_t dest, uint32_t a_off, uint32_t b_off) {
    sage_rt_number(vm, dest, sage_get_number(vm, a_off) - sage_get_number(vm, b_off));
}
void sage_rt_mul(MipsVM* vm, uint32_t dest, uint32_t a_off, uint32_t b_off) {
    sage_rt_number(vm, dest, sage_get_number(vm, a_off) * sage_get_number(vm, b_off));
}
void sage_rt_div(MipsVM* vm, uint32_t dest, uint32_t a_off, uint32_t b_off) {
    double b = sage_get_number(vm, b_off);
    sage_rt_number(vm, dest, b != 0.0 ? sage_get_number(vm, a_off) / b : 0.0);
}

// Comparison helpers
int sage_rt_eq(MipsVM* vm,  uint32_t a_off, uint32_t b_off)  { return sage_get_number(vm,a_off) == sage_get_number(vm,b_off); }
int sage_rt_neq(MipsVM* vm, uint32_t a_off, uint32_t b_off)  { return sage_get_number(vm,a_off) != sage_get_number(vm,b_off); }
int sage_rt_lt(MipsVM* vm,  uint32_t a_off, uint32_t b_off)  { return sage_get_number(vm,a_off) <  sage_get_number(vm,b_off); }
int sage_rt_gt(MipsVM* vm,  uint32_t a_off, uint32_t b_off)  { return sage_get_number(vm,a_off) >  sage_get_number(vm,b_off); }
int sage_rt_lte(MipsVM* vm, uint32_t a_off, uint32_t b_off)  { return sage_get_number(vm,a_off) <= sage_get_number(vm,b_off); }
int sage_rt_gte(MipsVM* vm, uint32_t a_off, uint32_t b_off)  { return sage_get_number(vm,a_off) >= sage_get_number(vm,b_off); }

// Get boolean value from Sage value (for branch conditions)
int sage_rt_get_bool(MipsVM* vm, uint32_t offset) {
    int type = sage_get_type(vm, offset);
    if (type == SAGE_VAL_NUMBER) return sage_get_number(vm, offset) != 0.0;
    if (type == SAGE_VAL_NIL) return 0;
    return 1; // strings, etc. are truthy
}
