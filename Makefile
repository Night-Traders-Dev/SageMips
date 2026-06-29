# ============================================================================
# SageMips Makefile
# ============================================================================
# Supports:
#   make              — Build SageMips (hosted)
#   make bare         — Build freestanding / bare-metal SageMips
#   make install      — Install to /usr/local/bin
#   make clean        — Remove build artifacts
#   make test         — Run tests
#   make help         — Show targets
# ============================================================================

CC       ?= gcc
CFLAGS   ?= -std=c11 -Wall -Wextra -O2
LDFLAGS  ?=

SRC_DIR   = src
BUILD_DIR = build
TARGET    = sagemips

SRCS = $(SRC_DIR)/mips_encode.c \
       $(SRC_DIR)/mips_vm.c \
       $(SRC_DIR)/mips_asm.c \
       $(SRC_DIR)/cli.c

OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Hosted build
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Built: $(TARGET)"

all: $(TARGET)

# Bare-metal build (freestanding, no libc)
bare: CFLAGS += -ffreestanding -nostdlib -DSAGE_BARE_METAL -fno-stack-protector
bare: LDFLAGS += -nostdlib -static
bare: $(TARGET)
	@echo "Built: $(TARGET) (bare-metal)"

# Install
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

# Clean
clean:
	@rm -rf $(BUILD_DIR) $(TARGET)
	@echo "Cleaned"

# Build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Test: assemble and run a simple test
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

# Help
help:
	@echo "SageMips Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make           Build SageMips (hosted)"
	@echo "  make bare      Build freestanding SageMips"
	@echo "  make install   Install to /usr/local/bin"
	@echo "  make clean     Remove build artifacts"
	@echo "  make test      Run quick tests"
	@echo "  make help      Show this help"

.PHONY: all bare install clean test help
