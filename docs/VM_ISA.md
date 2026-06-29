# SageMips VM Reference

## MIPS32 Instruction Set (Implemented)

All instructions are 32-bit, big-endian encoded.

### R-Type (opcode = 0x00 SPECIAL)

| Instruction | Funct | Description | Operation |
|-------------|-------|-------------|-----------|
| ADD | 0x20 | Add (with overflow trap) | rd = rs + rt |
| ADDU | 0x21 | Add unsigned | rd = rs + rt |
| SUB | 0x22 | Subtract | rd = rs - rt |
| SUBU | 0x23 | Subtract unsigned | rd = rs - rt |
| AND | 0x24 | Bitwise AND | rd = rs & rt |
| OR | 0x25 | Bitwise OR | rd = rs \| rt |
| XOR | 0x26 | Bitwise XOR | rd = rs ^ rt |
| NOR | 0x27 | Bitwise NOR | rd = ~(rs \| rt) |
| SLT | 0x2A | Set less than | rd = (rs < rt) ? 1 : 0 |
| SLTU | 0x2B | Set less than unsigned | rd = (urs < urt) ? 1 : 0 |
| SLL | 0x00 | Shift left logical | rd = rt << shamt |
| SRL | 0x02 | Shift right logical | rd = rt >> shamt |
| SRA | 0x03 | Shift right arithmetic | rd = (int32)rt >> shamt |
| SLLV | 0x04 | Shift left logical variable | rd = rt << (rs & 0x1F) |
| SRLV | 0x06 | Shift right logical variable | rd = rt >> (rs & 0x1F) |
| SRAV | 0x07 | Shift right arithmetic variable | rd = (int32)rt >> (rs & 0x1F) |
| JR | 0x08 | Jump register | pc = rs |
| JALR | 0x09 | Jump and link register | rd = pc+8; pc = rs |
| MFHI | 0x10 | Move from HI | rd = HI |
| MTHI | 0x11 | Move to HI | HI = rs |
| MFLO | 0x12 | Move from LO | rd = LO |
| MTLO | 0x13 | Move to LO | LO = rs |
| MULT | 0x18 | Multiply | {HI,LO} = rs * rt |
| MULTU | 0x19 | Multiply unsigned | {HI,LO} = urs * urt |
| DIV | 0x1A | Divide | LO = rs/rt; HI = rs%rt |
| DIVU | 0x1B | Divide unsigned | LO = urs/urt; HI = urs%urt |
| SYSCALL | 0x0C | System call | See syscall table |
| BREAK | 0x0D | Breakpoint | Halts VM |
| MOVZ | 0x0A | Move if zero | if rt==0: rd = rs |
| MOVN | 0x0B | Move if not zero | if rt!=0: rd = rs |
| TGE | 0x30 | Trap if >= | trap if rs >= rt |
| TGEU | 0x31 | Trap if >= unsigned | trap if urs >= urt |
| TLT | 0x32 | Trap if < | trap if rs < rt |
| TLTU | 0x33 | Trap if < unsigned | trap if urs < urt |
| TEQ | 0x34 | Trap if equal | trap if rs == rt |
| TNE | 0x36 | Trap if not equal | trap if rs != rt |

### I-Type

| Instruction | Opcode | Description | Operation |
|-------------|--------|-------------|-----------|
| ADDI | 0x08 | Add immediate | rt = rs + imm |
| ADDIU | 0x09 | Add immediate unsigned | rt = rs + imm |
| SLTI | 0x0A | Set less than imm | rt = (rs < imm) ? 1 : 0 |
| SLTIU | 0x0B | Set less than imm unsigned | rt = (urs < uimm) ? 1 : 0 |
| ANDI | 0x0C | AND immediate | rt = rs & uimm |
| ORI | 0x0D | OR immediate | rt = rs \| uimm |
| XORI | 0x0E | XOR immediate | rt = rs ^ uimm |
| LUI | 0x0F | Load upper immediate | rt = imm << 16 |
| LW | 0x23 | Load word | rt = mem[rs+imm] |
| SW | 0x2B | Store word | mem[rs+imm] = rt |
| LB | 0x20 | Load byte (sign-extend) | rt = (int8)mem[rs+imm] |
| LBU | 0x24 | Load byte unsigned | rt = mem[rs+imm] |
| LH | 0x21 | Load halfword (sign-extend) | rt = (int16)mem[rs+imm] |
| LHU | 0x25 | Load halfword unsigned | rt = mem[rs+imm] |
| SB | 0x28 | Store byte | mem[rs+imm] = (uint8)rt |
| SH | 0x29 | Store halfword | mem[rs+imm] = (uint16)rt |
| BEQ | 0x04 | Branch if equal | if rs==rt: pc+=imm<<2 |
| BNE | 0x05 | Branch if not equal | if rs!=rt: pc+=imm<<2 |
| BLEZ | 0x06 | Branch if <= 0 | if rs<=0: pc+=imm<<2 |
| BGTZ | 0x07 | Branch if > 0 | if rs>0: pc+=imm<<2 |

### J-Type

| Instruction | Opcode | Description | Operation |
|-------------|--------|-------------|-----------|
| J | 0x02 | Jump | pc = (pc&0xF0000000)\|(target<<2) |
| JAL | 0x03 | Jump and link | $ra=pc+8; pc=(pc&0xF0000000)\|(target<<2) |

### REGIMM (opcode = 0x01)

| Instruction | RT | Description |
|-------------|------|-------------|
| BLTZ | 0x00 | Branch if rs < 0 |
| BGEZ | 0x01 | Branch if rs >= 0 |
| BLTZL | 0x02 | Branch if rs < 0 (likely) |
| BGEZL | 0x03 | Branch if rs >= 0 (likely) |
| BLTZAL | 0x10 | Branch if rs < 0 and link |
| BGEZAL | 0x11 | Branch if rs >= 0 and link |

### SPECIAL2 (opcode = 0x1C)

| Instruction | Funct | Description |
|-------------|-------|-------------|
| MUL | 0x02 | Multiply to register (rd = rs * rt) |

## Register Convention

| Register | Name | Convention |
|----------|------|------------|
| $0 | zero | Always zero |
| $1 | at | Assembler temporary |
| $2–$3 | v0–v1 | Return values / expression evaluation |
| $4–$7 | a0–a3 | Function arguments |
| $8–$15 | t0–t7 | Caller-saved temporaries |
| $16–$23 | s0–s7 | Callee-saved |
| $24–$25 | t8–t9 | Temporaries |
| $26–$27 | k0–k1 | Reserved for kernel |
| $28 | gp | Global pointer |
| $29 | sp | Stack pointer |
| $30 | fp | Frame pointer |
| $31 | ra | Return address |

## Syscall Convention

Syscall number in `$v0`. Arguments in `$a0`, `$a1`, `$a2`, `$a3`. Return value in `$v0`.

| Number | Name | Description |
|--------|------|-------------|
| 0 | exit | Exit program |
| 1 | print_int | Print integer in $a0 |
| 2 | print_str | Print null-terminated string at $a0 |
| 3 | read_int | Read integer → $v0 |
| 4 | read_str | Read string → buffer at $a0 |
| 5 | sbrk | Allocate $a0 bytes → $v0 |
| 6 | print_char | Print character in $a0 |
| 7 | read_char | Read character → $v0 |
| 8 | time | Get time → $v0 |
| 9 | halt | Stop VM |
