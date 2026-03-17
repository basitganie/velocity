# ══════════════════════════════════════════
#  Velocity Compiler - Kashmiri Edition v2
#  Makefile
# ══════════════════════════════════════════

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L
TARGET  = velocity
SRCS    = main.c lexer.c parser.c codegen.c module.c
OBJS    = $(SRCS:.c=.o)

STDLIB_DEST = /usr/local/lib/velocity

.PHONY: all clean install uninstall test help

# ── Default target ──────────────────────
all: $(TARGET)
	@echo ""
	@echo "  ✓ velocity compiler built!"
	@echo "  Run: ./velocity examples/example1.vel -o out && ./out"
	@echo ""

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c velocity.h
	$(CC) $(CFLAGS) -c $< -o $@

# ── Install ─────────────────────────────
install: all
	@echo "Installing compiler to /usr/local/bin ..."
	sudo cp $(TARGET) /usr/local/bin/velocity
	@echo "Installing standard library to $(STDLIB_DEST) ..."
	sudo mkdir -p $(STDLIB_DEST)
	sudo cp stdlib/*.vel $(STDLIB_DEST)/ 2>/dev/null || true
	sudo cp stdlib/*.asm $(STDLIB_DEST)/
	@echo "✓ Done!  You can now run 'velocity' from anywhere."
	@echo "  Standard library installed at: $(STDLIB_DEST)"

uninstall:
	sudo rm -f /usr/local/bin/velocity
	sudo rm -rf $(STDLIB_DEST)
	@echo "✓ Uninstalled."

# ── Tests ───────────────────────────────
test: all
	@echo "Running tests..."
	@echo ""

	@echo "[1] Arithmetic (expect exit 42)"
	@./$(TARGET) examples/example1.vel -o _t1
	@./_t1 ; echo "    exit=$$?"

	@echo "[2] Functions (expect exit 45)"
	@./$(TARGET) examples/example2.vel -o _t2
	@./_t2 ; echo "    exit=$$?"

	@echo "[3] Conditionals (expect exit 35)"
	@./$(TARGET) examples/example3.vel -o _t3
	@./_t3 ; echo "    exit=$$?"

	@echo "[4] Factorial (expect exit 120)"
	@./$(TARGET) examples/example4.vel -o _t4
	@./_t4 ; echo "    exit=$$?"

	@echo "[5] Fibonacci (expect exit 55)"
	@./$(TARGET) examples/example5.vel -o _t5
	@./_t5 ; echo "    exit=$$?"

	@echo "[6] Math import (expect exit 55)"
	@./$(TARGET) examples/example_math.vel -o _t6
	@./_t6 ; echo "    exit=$$?"

	@rm -f _t1 _t2 _t3 _t4 _t5 _t6
	@echo ""
	@echo "✓ All tests done."

# ── Clean ───────────────────────────────
# NOTE: only deletes compiler build artifacts - never touches stdlib/
clean:
	rm -f $(OBJS) $(TARGET) a.out _t*
	@# Remove only generated .asm/.o files (compiler output), not stdlib sources
	@find . -maxdepth 1 -name "*.asm" -delete
	@find . -maxdepth 1 -name "*.o"   -delete
	@echo "✓ Cleaned (stdlib preserved)."

# ── Help ────────────────────────────────
help:
	@echo "Velocity Compiler - Kashmiri Edition v2"
	@echo ""
	@echo "Targets:"
	@echo "  make            Build the compiler"
	@echo "  make install    Install to /usr/local/bin + stdlib"
	@echo "  make uninstall  Remove installed files"
	@echo "  make test       Compile and run all examples"
	@echo "  make clean      Remove build artefacts"
	@echo "  make help       Show this message"
	@echo ""
	@echo "Compile a .vel file:"
	@echo "  ./velocity program.vel            # output: a.out"
	@echo "  ./velocity program.vel -o prog    # output: prog"
	@echo "  ./velocity program.vel -v         # verbose (keeps .asm/.o)"
	@echo ""
	@echo "Use stdlib without installing:"
	@echo "  VELOCITY_STDLIB=./stdlib ./velocity program.vel"
