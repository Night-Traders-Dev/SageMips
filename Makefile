# ============================================================================
# SageMips Makefile
# ============================================================================
# Dual-backend build: C (src/c/) and Sage (src/sage/)
# Optimizations: JIT=1 AOT=1 enable runtime optimizations
#
#   make                       Build C backend (hosted)
#   make JIT=1                 Build with JIT support
#   make AOT=1                 Build with AOT support
#   make JIT=1 AOT=1           Build with both JIT + AOT
#   make sage                  Build Sage backend
#   make bare                  Build C backend (freestanding)
#   make test                  Run tests
# ============================================================================

CC       ?= gcc
CFLAGS   ?= -std=c11 -Wall -Wextra -O2
LDFLAGS  ?=
SAGE     ?= /usr/local/bin/sage

C_SRC_DIR   = src/c
BUILD_DIR   = build
TARGET      = sagemips
TARGET_SAGE = sagemips_sage

C_SRCS = $(C_SRC_DIR)/mips_encode.c \
         $(C_SRC_DIR)/mips_vm.c \
         $(C_SRC_DIR)/mips_asm.c \
         $(C_SRC_DIR)/cli.c \
         $(C_SRC_DIR)/sage_rt.c \
         $(C_SRC_DIR)/mips_elf.c \
         $(C_SRC_DIR)/mips_dbg.c \
         $(C_SRC_DIR)/mips_heap.c

# JIT/AOT/ARC/ORC sources (conditionally compiled)
ifdef JIT
  C_SRCS += $(C_SRC_DIR)/mips_jit.c
  CFLAGS += -DSAGEMIPS_JIT
endif
ifdef AOT
  C_SRCS += $(C_SRC_DIR)/mips_aot.c
  CFLAGS += -DSAGEMIPS_AOT
endif
ifdef ARC
  C_SRCS += $(C_SRC_DIR)/mips_arc.c
  CFLAGS += -DSAGEMIPS_ARC
endif
ifdef ORC
  C_SRCS += $(C_SRC_DIR)/mips_orc.c
  CFLAGS += -DSAGEMIPS_ORC
endif

C_OBJS = $(patsubst $(C_SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))

SAGE_SRCS = src/sage/sagemips_main.sage
SAGE_PATH = src/sage

# ============================================================================
# C Backend Build
# ============================================================================

$(BUILD_DIR)/%.o: $(C_SRC_DIR)/%.c $(C_SRC_DIR)/sagemips.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(C_SRC_DIR) -c $< -o $@

$(TARGET): $(C_OBJS)
	$(CC) $(CFLAGS) $(C_OBJS) -o $@ $(LDFLAGS)
	@echo "Built: $(TARGET) (C backend)"

all: $(TARGET)

# Bare-metal build (C backend only, freestanding)
bare: CFLAGS += -ffreestanding -nostdlib -DSAGE_BARE_METAL -fno-stack-protector
bare: LDFLAGS += -nostdlib -static
bare: $(TARGET)
	@echo "Built: $(TARGET) (C bare-metal)"

# ============================================================================
# Sage Backend Build
# ============================================================================

sage:
	@echo "Building SageMips (Sage backend)..."
	@mkdir -p $(BUILD_DIR)
	SAGE_PATH=$(SAGE_PATH) $(SAGE) --compile $(SAGE_SRCS) -o $(TARGET_SAGE)
	@echo "Built: $(TARGET_SAGE) (Sage backend)"

sage-native:
	@echo "Building SageMips (Sage -> native MIPS)..."
	@mkdir -p $(BUILD_DIR)
	SAGE_PATH=$(SAGE_PATH) $(SAGE) --emit-asm $(SAGE_SRCS) --target mips -o $(BUILD_DIR)/sagemips_sage.asm
	@echo "MIPS assembly emitted to $(BUILD_DIR)/sagemips_sage.asm"

# ============================================================================
# Install
# ============================================================================

install: $(TARGET)
	@echo "Installing to /usr/local/bin..."
	@if [ -w /usr/local/bin ]; then \
		cp $(TARGET) /usr/local/bin/; \
		chmod 755 /usr/local/bin/$(TARGET); \
	else \
		sudo cp $(TARGET) /usr/local/bin/; \
		sudo chmod 755 /usr/local/bin/$(TARGET); \
	fi
	@echo "Installed: /usr/local/bin/$(TARGET)"

# ============================================================================
# Clean
# ============================================================================

clean:
	@rm -rf $(BUILD_DIR) $(TARGET) $(TARGET_SAGE)
	@echo "Cleaned"

# ============================================================================
# Build directory
# ============================================================================

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# ============================================================================
# Test
# ============================================================================

test: $(TARGET)
	@echo "=== SageMips Test Suite ==="
	@echo ""
	@echo "--- Test: Simple Arithmetic ---"
	@printf 'addiu $$v0, $$zero, 42\njr $$ra\n' > /tmp/test_arith.s
	@./$(TARGET) asm /tmp/test_arith.s /tmp/test_arith.mips
	@echo ""
	@echo "--- Test: Disassemble ---"
	@./$(TARGET) dis /tmp/test_arith.mips
	@rm -f /tmp/test_arith.s /tmp/test_arith.mips
	@echo ""
	@echo "=== Tests Complete ==="

# ============================================================================
# Help
# ============================================================================

help:
	@echo "SageMips Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make           Build SageMips (C backend, hosted)"
	@echo "  make sage      Build SageMips (Sage backend)"
	@echo "  make bare      Build C backend (freestanding/bare-metal)"
	@echo "  make install   Install to /usr/local/bin"
	@echo "  make clean     Remove build artifacts"
	@echo "  make test      Run quick tests"
	@echo "  make help      Show this help"

.PHONY: all bare sage sage-native install clean test help
