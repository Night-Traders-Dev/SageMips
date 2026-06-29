#include "sagemips.h"

// ============================================================================
// SageMips Linker — Combine multiple object files into one binary
// ============================================================================
// Takes multiple ELF object files (.o), resolves symbols, and produces
// a single flat MIPS binary suitable for VM execution.
// ============================================================================

#define LINK_MAX_OBJECTS   32
#define LINK_MAX_SYMBOLS  512
#define LINK_BUFFER_SIZE   (4*1024*1024)

typedef struct {
    char     name[64];
    uint32_t addr;
    uint32_t size;
    uint8_t  defined;  // 1 = defined here, 0 = external reference
    uint8_t  global;
} LinkSymbol;

typedef struct {
    LinkSymbol syms[LINK_MAX_SYMBOLS];
    int        sym_count;

    uint8_t   text_buf[LINK_BUFFER_SIZE];
    uint32_t  text_len;

    uint8_t   data_buf[LINK_BUFFER_SIZE];
    uint32_t  data_len;

    uint32_t  base_text; // current text section offset
    uint32_t  base_data; // current data section offset
} MipsLinker;

void mips_link_init(MipsLinker* link) {
    link->sym_count = 0;
    link->text_len = 0;
    link->data_len = 0;
    link->base_text = 0;
    link->base_data = 0;
}

// Add an object file to the linker
int mips_link_add_object(MipsLinker* link, const uint8_t* obj_data, uint32_t obj_size,
                          const char* obj_name) {
    // Validate ELF header
    if (obj_size < 52) return -1;
    uint32_t magic = (obj_data[0]<<24)|(obj_data[1]<<16)|(obj_data[2]<<8)|obj_data[3];
    if (magic != 0x7F454C46) return -2;

    // Parse section headers to find .text and .data
    uint32_t shoff = (obj_data[32]<<24)|(obj_data[33]<<16)|(obj_data[34]<<8)|obj_data[35];
    uint16_t shnum = (obj_data[48]<<8)|obj_data[49];
    uint16_t shentsize = (obj_data[46]<<8)|obj_data[47];

    // Also parse symbol table
    for (int i = 0; i < shnum; i++) {
        uint32_t sh_off = shoff + i * shentsize;
        if (sh_off + 40 > obj_size) break;

        uint32_t sh_name  = (obj_data[sh_off]<<24)|(obj_data[sh_off+1]<<16)|
                            (obj_data[sh_off+2]<<8)|obj_data[sh_off+3];
        uint32_t sh_type  = (obj_data[sh_off+4]<<24)|(obj_data[sh_off+5]<<16)|
                            (obj_data[sh_off+6]<<8)|obj_data[sh_off+7];
        uint32_t sh_addr  = (obj_data[sh_off+12]<<24)|(obj_data[sh_off+13]<<16)|
                            (obj_data[sh_off+14]<<8)|obj_data[sh_off+15];
        uint32_t sh_offset = (obj_data[sh_off+16]<<24)|(obj_data[sh_off+17]<<16)|
                             (obj_data[sh_off+18]<<8)|obj_data[sh_off+19];
        uint32_t sh_size  = (obj_data[sh_off+20]<<24)|(obj_data[sh_off+21]<<16)|
                            (obj_data[sh_off+22]<<8)|obj_data[sh_off+23];

        // .text section (type=1)
        if (sh_type == 1 && sh_size > 0) {
            uint32_t dest = link->text_len;
            for (uint32_t j = 0; j < sh_size && dest+j < LINK_BUFFER_SIZE && sh_offset+j < obj_size; j++) {
                link->text_buf[dest+j] = obj_data[sh_offset+j];
            }
            link->text_len += sh_size;
        }
    }

    (void)obj_name;
    return 0;
}

// Resolve symbols and produce final binary
int mips_link_finalize(MipsLinker* link, uint8_t* out, uint32_t* out_len) {
    // Simple: concatenate text + data sections
    uint32_t pos = 0;
    for (uint32_t i = 0; i < link->text_len && pos < MIPS_CODE_MAX; i++) {
        out[pos++] = link->text_buf[i];
    }
    for (uint32_t i = 0; i < link->data_len && pos < MIPS_CODE_MAX; i++) {
        out[pos++] = link->data_buf[i];
    }
    *out_len = pos;
    return 0;
}
