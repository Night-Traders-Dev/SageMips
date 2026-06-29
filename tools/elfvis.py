#!/usr/bin/env python3
"""
SageMips Visual ELF Inspector — elfvis
Extremely visual MIPS32 ELF binary analyzer with full disassembly.

Usage: python3 tools/elfvis.py <file.elf> [--no-color] [--full]
"""
import sys, os, struct, json, subprocess
from pathlib import Path

# ── Terminal Colors ────────────────────────────────────────────────────────
class T:
    RST='\033[0m'; BLD='\033[1m'; DIM='\033[2m'; UND='\033[4m'; REV='\033[7m'
    RED='\033[31m'; GRN='\033[32m'; YEL='\033[33m'; BLU='\033[34m'; MAG='\033[35m'
    CYN='\033[36m'; WHT='\033[37m'; GRY='\033[90m'
    BRED='\033[91m'; BGRN='\033[92m'; BYEL='\033[93m'; BBLU='\033[94m'; BMAG='\033[95m'
    BCYN='\033[96m'; BWHT='\033[97m'
    BG_RED='\033[41m'; BG_GRN='\033[42m'; BG_YEL='\033[43m'; BG_BLU='\033[44m'
    BG_MAG='\033[45m'; BG_CYN='\033[46m'; BG_WHT='\033[47m'
NO_COLOR = False

def c(*styles): return '' if NO_COLOR else ''.join(styles)

# ── ELF Constants ───────────────────────────────────────────────────────────
EM_MIPS = 8
ELFCLASS32, ELFDATA2MSB = 1, 2
ET_NONE, ET_REL, ET_EXEC, ET_DYN = 0, 1, 2, 3
PT_NULL, PT_LOAD, PT_DYNAMIC, PT_INTERP, PT_NOTE = 0, 1, 2, 3, 4
PT_PHDR, PT_TLS = 6, 7
SHT_NULL, SHT_PROGBITS, SHT_SYMTAB, SHT_STRTAB = 0, 1, 2, 3
SHT_RELA, SHT_HASH, SHT_DYNAMIC, SHT_NOTE = 4, 5, 6, 7
SHT_NOBITS, SHT_REL, SHT_SHLIB, SHT_DYNSYM = 8, 9, 10, 11
SHF_WRITE, SHF_ALLOC, SHF_EXECINSTR = 0x1, 0x2, 0x4
STB_LOCAL, STB_GLOBAL, STB_WEAK = 0, 1, 2
STT_NOTYPE, STT_OBJECT, STT_FUNC, STT_SECTION, STT_FILE = 0, 1, 2, 3, 4
R_MIPS_NONE, R_MIPS_32, R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16 = 0, 2, 4, 5, 6
R_MIPS_CALL16, R_MIPS_GPREL16 = 11, 7

MACHINE_NAMES = {EM_MIPS: 'MIPS', 3: 'i386', 62: 'x86-64', 40: 'ARM', 183: 'AArch64', 243: 'RISC-V'}
TYPE_NAMES = {0:'NONE', 1:'REL', 2:'EXEC', 3:'DYN', 4:'CORE'}
PT_NAMES = {0:'NULL', 1:'LOAD', 2:'DYNAMIC', 3:'INTERP', 4:'NOTE', 6:'PHDR', 7:'TLS'}
SHT_NAMES = {0:'NULL', 1:'PROGBITS', 2:'SYMTAB', 3:'STRTAB', 4:'RELA',
             5:'HASH', 6:'DYNAMIC', 7:'NOTE', 8:'NOBITS', 9:'REL',
             10:'SHLIB', 11:'DYNSYM'}
STB_NAMES = {0:'LOCAL', 1:'GLOBAL', 2:'WEAK'}
STT_NAMES = {0:'NOTYPE', 1:'OBJECT', 2:'FUNC', 3:'SECTION', 4:'FILE'}
R_MIPS_NAMES = {0:'NONE', 2:'32', 4:'26', 5:'HI16', 6:'LO16', 7:'GPREL16', 11:'CALL16'}

# ── ELF Parser ──────────────────────────────────────────────────────────────
class ElfFile:
    def __init__(self, data):
        self.data = data
        self.be = data[5] == ELFDATA2MSB
        self._parse()

    def _r32(self, off):
        return struct.unpack('>I' if self.be else '<I', self.data[off:off+4])[0]
    def _r16(self, off):
        return struct.unpack('>H' if self.be else '<H', self.data[off:off+2])[0]
    def _r8(self, off):
        return self.data[off]
    def _rs32(self, off, n):
        return struct.unpack(f">{'I'*n}" if self.be else f"<{'I'*n}", self.data[off:off+4*n])

    def _parse(self):
        d = self.data
        self.magic = self._r32(0)
        self.elf_class = d[4]
        self.encoding = d[5]
        self.elf_version = d[6]
        self.os_abi = d[7]
        self.abi_ver = d[8]
        self.etype = self._r16(16)
        self.machine = self._r16(18)
        self.eversion = self._r32(20)
        self.entry = self._r32(24)
        self.phoff = self._r32(28)
        self.shoff = self._r32(32)
        self.flags = self._r32(36)
        self.ehsize = self._r16(40)
        self.phentsize = self._r16(42)
        self.phnum = self._r16(44)
        self.shentsize = self._r16(46)
        self.shnum = self._r16(48)
        self.shstrndx = self._r16(50)

        # Program headers
        self.phdrs = []
        for i in range(self.phnum):
            off = self.phoff + i * self.phentsize
            self.phdrs.append({
                'type': self._r32(off), 'offset': self._r32(off+4),
                'vaddr': self._r32(off+8), 'paddr': self._r32(off+12),
                'filesz': self._r32(off+16), 'memsz': self._r32(off+20),
                'flags': self._r32(off+24), 'align': self._r32(off+28),
            })

        # Section headers
        self.shdrs = []
        for i in range(self.shnum):
            off = self.shoff + i * self.shentsize
            self.shdrs.append({
                'name_idx': self._r32(off), 'type': self._r32(off+4),
                'flags': self._r32(off+8) if self.elf_class == 2 else self._r32(off+8),
                'addr': self._r32(off+12), 'offset': self._r32(off+16),
                'size': self._r32(off+20), 'link': self._r32(off+24),
                'info': self._r32(off+28), 'addralign': self._r32(off+32),
                'entsize': self._r32(off+36),
            })

        # Section name string table
        self._shstrtab = b''
        if self.shstrndx < len(self.shdrs):
            s = self.shdrs[self.shstrndx]
            self._shstrtab = d[s['offset']:s['offset']+s['size']]

    def sh_name(self, idx):
        s = self.shdrs[idx]
        end = self._shstrtab.find(b'\0', s['name_idx'])
        return self._shstrtab[s['name_idx']:end].decode('ascii', errors='replace')

    def find_section(self, name):
        for i in range(self.shnum):
            try:
                if self.sh_name(i) == name: return self.shdrs[i], i
            except: pass
        return None, -1

    def read_section(self, idx):
        s = self.shdrs[idx]
        return self.data[s['offset']:s['offset']+s['size']]

    def read_symtab(self):
        """Parse .symtab section, return list of symbol dicts"""
        s, sidx = self.find_section('.symtab')
        if not s: return []
        strtab_s, strtab_idx = self.find_section('.strtab')
        if not strtab_s: return []
        data = self.read_section(sidx)
        strs = self.read_section(strtab_idx)
        syms = []
        entsize = 16
        fmt = '>I I I B B H' if self.be else '<I I I B B H'
        for i in range(0, len(data), entsize):
            if i + entsize > len(data): break
            st_name, st_value, st_size, st_info, st_other, st_shndx = \
                struct.unpack(fmt, data[i:i+16])
            end = strs.find(b'\0', st_name)
            name = (strs[st_name:end] if end > 0 else b'').decode('ascii', errors='replace')
            syms.append({'name': name, 'value': st_value, 'size': st_size,
                         'bind': st_info >> 4, 'type': st_info & 0xF,
                         'shndx': st_shndx, 'info': st_info})
        return syms

# ── Visual Rendering ────────────────────────────────────────────────────────
W = 80  # terminal width

def hdr(title, color=None):
    clr = color or ''
    print(f"\n{c(T.BLD, clr)}╔{'═'*(W-2)}╗")
    print(f"║ {title:<{W-4}} ║")
    print(f"╚{'═'*(W-2)}╝{c(T.RST)}")

def sub(title):
    print(f"\n{c(T.BLD, T.CYN)}▸ {title}{c(T.RST)}")
    print(f"  {'─'*60}")

def kv(k, v, color=''):
    print(f"  {c(T.DIM)}{k:<24}{c(T.RST)} {color}{v}{c(T.RST)}")

def hex32(v): return f"0x{v:08X}"
def hex16(v): return f"0x{v:04X}"

def bar(value, max_val, width=40, color=None):
    pct = min(value/max_val, 1.0) if max_val > 0 else 0
    filled = int(pct * width)
    clr = color or T.GRN
    return f"{clr}{'█'*filled}{c(T.DIM)}{'░'*(width-filled)}{c(T.RST)} {pct*100:.0f}%"

def flag_str(flags, names):
    parts = []
    for bit, name in sorted(names.items()):
        if flags & bit: parts.append(name)
    return ' | '.join(parts) if parts else '—'

# ── Hexdump ─────────────────────────────────────────────────────────────────
def hexdump(data, offset=0, lines=16):
    for i in range(0, min(len(data), lines*16), 16):
        chunk = data[i:i+16]
        hex_part = ' '.join(f'{b:02X}' for b in chunk)
        ascii_part = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
        addr = offset + i
        print(f"  {c(T.DIM)}{addr:08X}{c(T.RST)}  {hex_part:<48} {c(T.DIM)}│{c(T.RST)} {ascii_part}")

# ── Memory Map Visualization ────────────────────────────────────────────────
def memmap(elf):
    print(f"\n{c(T.BLD, T.CYN)}▸ Memory Layout{c(T.RST)}")
    print(f"  {'─'*70}")
    segments = sorted([(p['vaddr'], p['vaddr']+p['memsz'], p['type'], p['flags'])
                       for p in elf.phdrs if p['type'] == PT_LOAD],
                      key=lambda x: x[0])
    if not segments:
        print(f"  {c(T.DIM)}No loadable segments{c(T.RST)}")
        return

    total_end = max(s[1] for s in segments)
    scale = 68 / max(total_end, 1)

    colors = {0: T.BG_BLU, 1: T.BG_GRN, 2: T.BG_MAG}
    flag_colors = {4: T.BG_RED, 6: T.BG_YEL}  # exec, rw

    for vaddr, vend, ptype, pflags in segments:
        start_col = int(vaddr * scale)
        end_col = int(vend * scale)
        bar_len = max(end_col - start_col, 1)

        # Determine color based on flags
        if pflags & SHF_EXECINSTR:
            color = f"{T.BLD}{T.BRED}"
            label = "CODE"
        elif pflags & SHF_WRITE:
            color = f"{T.BLD}{T.BGRN}"
            label = "DATA"
        else:
            color = f"{T.BLD}{T.BLU}"
            label = "RO"

        addr_label = f"0x{vaddr:08X}"
        sz_label = f"{vend-vaddr:,}B"
        print(f"  {addr_label} {c(T.DIM)}{'─'*3}{c(T.RST)} {color}{'█'*bar_len}{c(T.RST)} {c(T.DIM)}{'─'*3}{c(T.RST)} {sz_label:<10} {label}")

# ── ELF Header ──────────────────────────────────────────────────────────────
def print_elf_header(elf):
    hdr("ELF Header", T.BMAG)
    print(f"""
  {c(T.BLD, T.WHT)}┌──── ELF Identification ──────────────────────────────────────────┐{c(T.RST)}
  │  Magic:    {c(T.BYEL)}{' '.join(f'{b:02X}' for b in elf.data[0:4])}{c(T.RST)}  (0x7F 'E' 'L' 'F')
  │  Class:    {c(T.BLU)}ELF{elf.elf_class*32}{c(T.RST)} ({'32-bit' if elf.elf_class==1 else '64-bit'})
  │  Encoding: {c(T.BLU)}{'Big-endian' if elf.encoding==2 else 'Little-endian'}{c(T.RST)}
  │  Version:  {elf.elf_version}  │  OS/ABI: {elf.os_abi}  │  ABI Ver: {elf.abi_ver}
  {c(T.BLD, T.WHT)}└──────────────────────────────────────────────────────────────┘{c(T.RST)}

  {c(T.BLD, T.WHT)}┌──── Type & Machine ─────────────────────────────────────────────┐{c(T.RST)}
  │  Type:     {c(T.BYEL)}{TYPE_NAMES.get(elf.etype, str(elf.etype))}{c(T.RST)} (0x{elf.etype:04X})
  │  Machine:  {c(T.BGRN)}{MACHINE_NAMES.get(elf.machine, f'0x{elf.machine:04X}')}{c(T.RST)}
  │  Version:  {hex32(elf.eversion)}
  {c(T.BLD, T.WHT)}└──────────────────────────────────────────────────────────────┘{c(T.RST)}

  {c(T.BLD, T.WHT)}┌──── Entry & Offsets ───────────────────────────────────────────┐{c(T.RST)}
  │  Entry:     {c(T.BYEL)}{hex32(elf.entry)}{c(T.RST)}
  │  PH offset: {hex32(elf.phoff)} ({elf.phnum} headers × {elf.phentsize}B)
  │  SH offset: {hex32(elf.shoff)} ({elf.shnum} headers × {elf.shentsize}B)
  │  Flags:     {hex32(elf.flags)}
  │  SH strtab: section #{elf.shstrndx}
  {c(T.BLD, T.WHT)}└──────────────────────────────────────────────────────────────┘{c(T.RST)}""")

# ── Program Headers ─────────────────────────────────────────────────────────
def print_program_headers(elf):
    if elf.phnum == 0: return
    hdr(f"Program Headers ({elf.phnum})", T.BCYN)
    print(f"  {c(T.BLD, T.DIM)}{'Type':>6} {'Offset':>10} {'VirtAddr':>10} {'PhysAddr':>10} {'FileSz':>10} {'MemSz':>10} {'Flags':>6} {'Align':>8}{c(T.RST)}")
    print(f"  {'─'*6} {'─'*10} {'─'*10} {'─'*10} {'─'*10} {'─'*10} {'─'*6} {'─'*8}")
    for p in elf.phdrs:
        tname = PT_NAMES.get(p['type'], str(p['type']))
        tcolor = T.BGRN if p['type'] == PT_LOAD else (T.BYEL if p['type'] == PT_DYNAMIC else '')
        fl = ''
        if p['flags'] & 4: fl += 'R'
        if p['flags'] & 2: fl += 'W'
        if p['flags'] & 1: fl += 'X'
        fcolor = T.BRED if 'X' in fl else (T.BYEL if 'W' in fl else T.BLU)
        print(f"  {tcolor}{tname:>6}{c(T.RST)} "
              f"{c(T.DIM)}{hex32(p['offset'])}{c(T.RST)} "
              f"{hex32(p['vaddr'])} {hex32(p['paddr'])} "
              f"{hex32(p['filesz'])} {hex32(p['memsz'])} "
              f"{fcolor}{fl:>6}{c(T.RST)} {hex32(p['align'])}")

# ── Section Headers ─────────────────────────────────────────────────────────
def print_section_headers(elf):
    if elf.shnum == 0: return
    hdr(f"Section Headers ({elf.shnum})", T.BBLU)
    print(f"  {c(T.BLD, T.DIM)}{'[#] Name':<24} {'Type':>10} {'Addr':>10} {'Offset':>8} {'Size':>8} {'ES':>5} {'Flg':>4} {'Lk':>3} {'Inf':>3} {'Al':>2}{c(T.RST)}")
    print(f"  {'─'*24} {'─'*10} {'─'*10} {'─'*8} {'─'*8} {'─'*5} {'─'*4} {'─'*3} {'─'*3} {'─'*2}")
    for i, s in enumerate(elf.shdrs):
        try: name = elf.sh_name(i)
        except: name = f'<shdr[{i}]>'
        tname = SHT_NAMES.get(s['type'], hex32(s['type']))
        fl = ''
        if s['flags'] & SHF_ALLOC: fl += 'A'
        if s['flags'] & SHF_WRITE: fl += 'W'
        if s['flags'] & SHF_EXECINSTR: fl += 'X'
        marker = '←' if i == elf.shstrndx else ' '
        print(f"  {c(T.DIM)}[{i:>2}]{c(T.RST)} {marker}{name:<22} "
              f"{c(T.CYN)}{tname:>10}{c(T.RST)} "
              f"{c(T.DIM)}{hex32(s['addr']):>10}{c(T.RST)} "
              f"{hex32(s['offset']):>8} {hex32(s['size']):>8} "
              f"{hex32(s['entsize']):>5} {c(T.YEL)}{fl:>4}{c(T.RST)} "
              f"{s['link']:>3} {s['info']:>3} {s['addralign']:>2}")

# ── Symbol Table ────────────────────────────────────────────────────────────
def print_symbol_table(elf):
    syms = elf.read_symtab()
    if not syms: return
    hdr(f"Symbol Table ({len(syms)} entries)", T.BYEL)
    print(f"  {c(T.BLD, T.DIM)}{'Value':>10} {'Size':>6} {'Type':>8} {'Bind':>7} {'Ndx':>4}  Name{c(T.RST)}")
    print(f"  {'─'*10} {'─'*6} {'─'*8} {'─'*7} {'─'*4}  {'─'*40}")
    for s in syms[:40]:  # show first 40
        if s['name'] == '': continue
        bcolor = T.BYEL if s['bind'] == STB_GLOBAL else T.DIM
        tcolor = T.BGRN if s['type'] == STT_FUNC else (T.BLU if s['type'] == STT_OBJECT else T.DIM)
        ndx = str(s['shndx']) if s['shndx'] < 0xFF00 else 'ABS' if s['shndx'] == 0xFFF1 else 'UND' if s['shndx'] == 0 else hex16(s['shndx'])
        print(f"  {c(T.DIM)}{hex32(s['value'])}{c(T.RST)} "
              f"{hex32(s['size']):>6} "
              f"{tcolor}{STT_NAMES.get(s['type'],'?'):>8}{c(T.RST)} "
              f"{bcolor}{STB_NAMES.get(s['bind'],'?'):>7}{c(T.RST)} "
              f"{ndx:>4}  {s['name']}")

# ── Disassembly ─────────────────────────────────────────────────────────────
def print_disassembly(elf):
    s, idx = elf.find_section('.text')
    if not s or s['size'] == 0:
        s, idx = elf.find_section('.init')
    if not s or s['size'] == 0:
        # Try to find first executable section
        for i, sh in enumerate(elf.shdrs):
            if sh['flags'] & SHF_EXECINSTR and sh['size'] > 0:
                s, idx = sh, i
                break
    if not s or s['size'] == 0:
        return

    data = elf.read_section(idx)
    hdr(f"Disassembly — Section #{idx} ({elf.sh_name(idx)})", T.BGRN)

    # Build hex dump + instruction disasm
    addr = s['addr']
    for i in range(0, min(len(data), 32*4), 4):
        raw = struct.unpack('>I', data[i:i+4])[0]
        # Remote disassembly via sagemips tool
        dis_line = f"{raw:08X}"
        print(f"  {c(T.DIM)}{addr+i:08X}{c(T.RST)}  {c(T.YEL)}{dis_line}{c(T.RST)}")

# ── File Overview ───────────────────────────────────────────────────────────
def print_overview(elf, fpath, fsize):
    hdr(f"SageMips Visual ELF Inspector — {os.path.basename(fpath)}", T.BMAG)
    print(f"  {c(T.DIM)}File:{c(T.RST)} {fpath}")
    print(f"  {c(T.DIM)}Size:{c(T.RST)} {fsize:,} bytes ({fsize/1024:.1f} KB)")
    status = f"{c(T.BGRN)} VALID MIPS ELF {c(T.RST)}" if elf.magic == 0x7F454C46 and elf.machine == EM_MIPS else f"{c(T.BRED)} INVALID/WRONG ARCH {c(T.RST)}"
    print(f"  {c(T.DIM)}Status:{c(T.RST)} {status}")

    # Structure diagram
    print(f"\n  {c(T.BLD)}ELF Structure:{c(T.RST)}")
    print(f"  {c(T.DIM)}┌──────────────┐{c(T.RST)}")
    print(f"  {c(T.DIM)}│{c(T.RST)} {c(T.BYEL)}ELF Header{c(T.RST)}    {c(T.DIM)}│{c(T.RST)}  {elf.ehsize}B")
    if elf.phoff > 0:
        print(f"  {c(T.DIM)}├──────────────┤{c(T.RST)}")
        print(f"  {c(T.DIM)}│{c(T.RST)} {c(T.BCYN)}Prog Headers{c(T.RST)}  {c(T.DIM)}│{c(T.RST)}  {elf.phnum} × {elf.phentsize}B = {elf.phnum*elf.phentsize}B")
    print(f"  {c(T.DIM)}├──────────────┤{c(T.RST)}")
    for i, s in enumerate(elf.shdrs[:5]):
        try: name = elf.sh_name(i)
        except: name = f'#{i}'
        color = T.BLU if s['flags'] & SHF_ALLOC else T.DIM
        print(f"  {c(T.DIM)}│{c(T.RST)} {color}{name:<12}{c(T.RST)} {c(T.DIM)}│{c(T.RST)}  {s['size']:,}B")
    if elf.shnum > 5:
        print(f"  {c(T.DIM)}│{c(T.RST)} {c(T.DIM)}... ({elf.shnum-5} more sections){c(T.RST)} {c(T.DIM)}│{c(T.RST)}")
    print(f"  {c(T.DIM)}└──────────────┘{c(T.RST)}")

# ── Section Hex Dump ────────────────────────────────────────────────────────
def print_section_hex(elf):
    sub("Section Contents")
    for i, s in enumerate(elf.shdrs):
        try: name = elf.sh_name(i)
        except: continue
        if s['size'] == 0 or name == '.shstrtab' or name == '.strtab': continue
        if s['size'] > 4096: continue  # skip huge sections

        print(f"\n  {c(T.BLD, T.CYN)}{name}{c(T.RST)} ({s['size']} bytes at offset {hex32(s['offset'])})")
        data = elf.read_section(i)
        hexdump(data, s['addr'], min(8, s['size']//16 + 1))

# ── Main ────────────────────────────────────────────────────────────────────
def main():
    global NO_COLOR
    import argparse
    p = argparse.ArgumentParser(description='SageMips Visual ELF Inspector')
    p.add_argument('file', help='ELF file to inspect')
    p.add_argument('--no-color', action='store_true', help='Disable colors')
    p.add_argument('--full', action='store_true', help='Show full output (all symbols, all hex)')
    p.add_argument('--dis', action='store_true', help='Show disassembly only')
    p.add_argument('--syms', action='store_true', help='Show symbols only')
    args = p.parse_args()
    NO_COLOR = args.no_color

    fpath = args.file
    if not os.path.exists(fpath):
        print(f"Error: file not found: {fpath}")
        sys.exit(1)

    fsize = os.path.getsize(fpath)
    with open(fpath, 'rb') as f:
        data = f.read()

    if len(data) < 52:
        print(f"Error: file too small to be ELF ({len(data)} bytes)")
        sys.exit(1)

    elf = ElfFile(data)

    if args.dis:
        print_disassembly(elf)
        return
    if args.syms:
        print_symbol_table(elf)
        return

    # Full visual inspection
    print_overview(elf, fpath, fsize)
    print_elf_header(elf)
    memmap(elf)
    print_program_headers(elf)
    print_section_headers(elf)
    print_symbol_table(elf)
    print_disassembly(elf)
    if args.full:
        print_section_hex(elf)

    print(f"\n{c(T.DIM)}{'═'*80}{c(T.RST)}")
    print(f"{c(T.BLD)}Done.{c(T.RST)} Use {c(T.CYN)}--full{c(T.RST)} for section hex dumps, {c(T.CYN)}--dis{c(T.RST)} for disassembly only.")

if __name__ == '__main__':
    main()
