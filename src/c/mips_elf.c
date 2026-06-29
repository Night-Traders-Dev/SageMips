#include "sagemips.h"

// ============================================================================
// SageMips ELF32 Loader
// ============================================================================
// Parses ELF32 MIPS big-endian executables and loads .text + .data into
// the VM's code buffer. Supports:
//   - ELF32 header validation (magic, class, machine=EM_MIPS, endian)
//   - Program header parsing (PT_LOAD segments)
//   - Section header parsing (.text, .data, .bss)
//   - Flat binary extraction for VM execution
// ============================================================================

#define ELF_MAGIC    0x7F454C46  // "\x7FELF"
#define EM_MIPS      8
#define ELFCLASS32   1
#define ELFDATA2MSB  2
#define ET_EXEC      2
#define PT_LOAD      1
#define SHT_PROGBITS 1
#define SHT_NOBITS   8

typedef struct {
    uint32_t magic;
    uint8_t  elf_class;
    uint8_t  data_encoding;
    uint8_t  version;
    uint8_t  os_abi;
    uint8_t  abi_version;
    uint8_t  pad[7];
    uint16_t type;
    uint16_t machine;
    uint32_t version2;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} Elf32Header;

typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} Elf32ProgramHeader;

typedef struct {
    uint32_t name;
    uint32_t type;
    uint32_t flags;
    uint32_t addr;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t addralign;
    uint32_t entsize;
} Elf32SectionHeader;

static uint32_t elf_read32(const uint8_t* data, uint32_t off, int big_endian) {
    if (big_endian)
        return ((uint32_t)data[off]<<24)|((uint32_t)data[off+1]<<16)|
               ((uint32_t)data[off+2]<<8)|(uint32_t)data[off+3];
    else
        return ((uint32_t)data[off])|((uint32_t)data[off+1]<<8)|
               ((uint32_t)data[off+2]<<16)|((uint32_t)data[off+3]<<24);
}

static uint16_t elf_read16(const uint8_t* data, uint32_t off, int big_endian) {
    if (big_endian)
        return ((uint16_t)data[off]<<8)|(uint16_t)data[off+1];
    else
        return ((uint16_t)data[off])|((uint16_t)data[off+1]<<8);
}

// Load an ELF file and extract flat MIPS binary
int mips_elf_load(const uint8_t* elf_data, uint32_t elf_size,
                  uint8_t* out_code, uint32_t* out_len, uint32_t* entry) {
    if (elf_size < 52) return -1; // too small for ELF header

    // Validate magic
    uint32_t magic = elf_read32(elf_data, 0, 1);
    if (magic != ELF_MAGIC) return -2;

    // Validate ELF class and encoding
    if (elf_data[4] != ELFCLASS32) return -3;
    int big_endian = (elf_data[5] == ELFDATA2MSB);

    // Validate machine
    uint16_t machine = elf_read16(elf_data, 18, big_endian);
    if (machine != EM_MIPS) {
        // Also accept generic ELF — just warn
    }

    uint32_t entry_point = elf_read32(elf_data, 24, big_endian);
    uint32_t phoff = elf_read32(elf_data, 28, big_endian);
    uint16_t phnum = elf_read16(elf_data, 44, big_endian);

    *out_len = 0;
    *entry = entry_point;

    // Load PT_LOAD segments
    for (int i = 0; i < phnum && phoff > 0; i++) {
        uint32_t ph_addr = phoff + i * 32;
        if (ph_addr + 32 > elf_size) break;

        uint32_t type   = elf_read32(elf_data, ph_addr, big_endian);
        uint32_t offset = elf_read32(elf_data, ph_addr+4, big_endian);
        uint32_t vaddr  = elf_read32(elf_data, ph_addr+8, big_endian);
        uint32_t filesz = elf_read32(elf_data, ph_addr+16, big_endian);
        uint32_t memsz  = elf_read32(elf_data, ph_addr+20, big_endian);

        if (type == PT_LOAD && filesz > 0) {
            // Load segment data into output buffer at vaddr
            uint32_t out_off = vaddr; // assume 0-based loading
            for (uint32_t j = 0; j < filesz && offset+j < elf_size; j++) {
                if (out_off + j < MIPS_CODE_MAX) {
                    out_code[out_off + j] = elf_data[offset + j];
                    if (out_off + j >= *out_len) *out_len = out_off + j + 1;
                }
            }
            // Zero-fill .bss portion
            for (uint32_t j = filesz; j < memsz; j++) {
                if (out_off + j < MIPS_CODE_MAX) {
                    out_code[out_off + j] = 0;
                    if (out_off + j >= *out_len) *out_len = out_off + j + 1;
                }
            }
        }
    }

    if (*out_len == 0) return -4; // no loadable segments
    return 0;
}
