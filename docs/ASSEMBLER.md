# SageMips Assembler Reference

> Both the C backend (`src/c/mips_asm.c`) and Sage backend (`src/sage/mips_asm.sage`) implement compatible two-pass assemblers. This reference applies to both.

## Quick Start

```bash
# Assemble a file
sagemips asm hello.s          # Output: hello.mips
sagemips asm hello.s out.bin  # Output: out.bin

# Run assembled binary
sagemips run hello.mips
```

## Syntax

MIPS assembly uses the standard GNU assembler syntax:

```asm
# Comments start with #
    .text              # Code section directive
main:                  # Label definition
    addiu $v0, $zero, 42
    jr    $ra
```

### Tokens

- **Registers**: `$zero`, `$at`, `$v0`–`$v1`, `$a0`–`$a3`, `$t0`–`$t9`, `$s0`–`$s7`, `$k0`–`$k1`, `$gp`, `$sp`, `$fp`, `$ra`
- **Numeric registers**: `$0` through `$31`
- **Immediates**: Decimal (`42`), hex (`0xFF`), negative (`-5`)
- **Labels**: Alphanumeric identifiers followed by `:`

### Instruction Formats

#### Three-Register (R-type)
```asm
add  $rd, $rs, $rt
sub  $t0, $t1, $t2
and  $v0, $a0, $a1
```

#### Two-Register + Immediate
```asm
addiu $rt, $rs, imm
slti  $t0, $t1, 10
lui   $t0, 0x1000
```

#### Memory Access
```asm
lw    $rt, offset($base)
sw    $t0, 8($sp)
lb    $v0, 0($a0)
sb    $t1, 4($fp)
```

#### Branch
```asm
beq   $rs, $rt, label
bne   $t0, $t1, loop
bgtz  $s0, skip
```

#### Jump
```asm
j     label
jal   function
jr    $ra
jalr  $rs          # jalr $ra, $rs
jalr  $rd, $rs     # link to $rd
```

#### Shift
```asm
sll   $rd, $rt, shamt
srl   $t0, $t1, 4
sra   $v0, $a0, 2
sllv  $rd, $rt, $rs
```

#### Multiply/Divide
```asm
mult  $rs, $rt
div   $t0, $t1
mfhi  $rd
mflo  $v0
mthi  $rs
mtlo  $rs
```

## Pseudo-Instructions

| Pseudo | Expands To | Description |
|--------|-----------|-------------|
| `nop` | `sll $zero, $zero, 0` | No operation |
| `move $rd, $rs` | `addu $rd, $rs, $zero` | Register copy |
| `not $rd, $rs` | `nor $rd, $rs, $zero` | Bitwise NOT |
| `neg $rd, $rs` | `sub $rd, $zero, $rs` | Negate |
| `li $rd, imm` | `lui $rd, upper` + `ori $rd, $rd, lower` | Load immediate (32-bit) |
| `b label` | `beq $zero, $zero, label` | Unconditional branch |
| `la $rd, label` | `lui $rd, upper` + `ori $rd, $rd, lower` | Load address |

## Directives

| Directive | Description | Example |
|-----------|-------------|---------|
| `.text` | Code section | `.text` |
| `.data` | Data section | `.data` |
| `.word val` | 32-bit word | `.word 0xDEADBEEF` |
| `.byte val` | 8-bit byte | `.byte 0x41` |
| `.ascii "str"` | ASCII string | `.ascii "hello"` |
| `.asciiz "str"` | Null-terminated ASCII | `.asciiz "hello"` |
| `.space n` | Zero-fill n bytes | `.space 256` |
| `.align n` | Alignment (ignored) | `.align 4` |
| `.globl sym` | Export symbol | `.globl main` |
| `.global sym` | Export symbol | `.global _start` |

## Error Handling

The assembler is a two-pass design:

- **Pass 0**: Scans all lines, collects labels, validates syntax. Code size is tracked.
- **Pass 1**: Emits instructions, resolves forward and backward label references.

If assembly fails, `sagemips asm` returns exit code 1. Error messages are printed to stdout.

## Examples

### Hello World (print 42)
```asm
    .text
main:
    addiu $v0, $zero, 1      # syscall: print_int
    addiu $a0, $zero, 42     # argument: 42
    syscall
    addiu $v0, $zero, 9      # syscall: halt
    syscall
```

### Fibonacci
```asm
    .text
main:
    addiu $s0, $zero, 10     # n = 10
    addiu $s1, $zero, 0      # fib(0) = 0
    addiu $s2, $zero, 1      # fib(1) = 1
    addiu $s3, $zero, 0      # i = 0
loop:
    beq   $s3, $s0, done
    nop
    add   $s4, $s1, $s2
    move  $s1, $s2
    move  $s2, $s4
    addiu $s3, $s3, 1
    b     loop
    nop
done:
    addiu $v0, $zero, 9      # halt
    syscall
```
