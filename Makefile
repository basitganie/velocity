# ------------------------------------------------------------
#  Velocity Compiler - Kashmiri Edition v2
#  Makefile  (Linux host -> produces velocity binary)
#
#  To cross-compile velocity.exe for Windows using MinGW-w64:
#    make CC=x86_64-w64-mingw32-gcc TARGET_EXE=velocity.exe
# ------------------------------------------------------------

CC       ?= gcc
CFLAGS   = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L
TARGET   = velocity
SRCS     = main.c lexer.c parser.c codegen.c module.c
OBJS     = $(SRCS:.c=.o)

STDLIB_DEST ?= /usr/local/lib/velocity

.PHONY: all clean install uninstall test capability-test help cross cross-win

# -- Default target ----------------------------------------------
all: $(TARGET)
	@echo ""
	@echo "  [OK] velocity compiler built!"
	@echo "  Run: ./velocity examples/hello.vel -o hello && ./hello"
	@echo ""

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c velocity.h
	$(CC) $(CFLAGS) -c $< -o $@

# -- Cross-compile velocity.exe for Windows ----------------------
cross: cross-win

cross-win:
	@echo "Building velocity.exe for Windows..."
	x86_64-w64-mingw32-gcc $(CFLAGS) -o velocity.exe \
		main.c lexer.c parser.c codegen.c module.c
	@echo "[OK] velocity.exe ready"
	@echo "  Copy to Windows along with stdlib/ folder."
	@echo "  On Windows, ensure nasm and gcc (MinGW) are in PATH."

# -- Install -----------------------------------------------------
install: all
	@echo "Installing velocity to /usr/local/bin ..."
	sudo cp $(TARGET) /usr/local/bin/velocity
	@echo "Installing stdlib to $(STDLIB_DEST) ..."
	sudo mkdir -p $(STDLIB_DEST)
	sudo cp stdlib/*.asm $(STDLIB_DEST)/
	-sudo cp stdlib/*.vel $(STDLIB_DEST)/ 2>/dev/null
	@echo "[OK] Done.  Run 'velocity --help' anywhere."

uninstall:
	sudo rm -f /usr/local/bin/velocity
	sudo rm -rf $(STDLIB_DEST)
	@echo "[OK] Uninstalled."

# -- Tests -------------------------------------------------------
test: all
	@echo "Running tests..."
	@echo ""

	@echo "[1] Hello World"
	@./$(TARGET) tests/hello.vel -o _t_hello
	@./_t_hello
	@echo ""

	@echo "[2] Arithmetic"
	@./$(TARGET) tests/arithmetic.vel -o _t_arith
	@./_t_arith
	@echo ""

	@echo "[3] Loops"
	@./$(TARGET) tests/loops.vel -o _t_loops
	@./_t_loops
	@echo ""

	@echo "[4] Arrays (fixed + dynamic)"
	@./$(TARGET) tests/arrays.vel -o _t_arr
	@./_t_arr
	@echo ""

	@echo "[5] Structs"
	@./$(TARGET) tests/structs.vel -o _t_struct
	@./_t_struct
	@echo ""

	@echo "[6] Bitwise ops"
	@./$(TARGET) tests/bitwise.vel -o _t_bit
	@./_t_bit
	@echo ""

	@rm -f _t_hello _t_arith _t_loops _t_arr _t_struct _t_bit
	@echo ""
	@echo "[OK] All tests done."

capability-test: all
	@echo "Running capability suite..."
	@./tests/run_capability_suite.sh

# -- Clean --------------------------------------------------------
clean:
	rm -f $(OBJS) $(TARGET) velocity.exe a.out _t_*
	@find . -maxdepth 1 -name "*.asm" ! -path "./stdlib/*" -delete
	@find . -maxdepth 1 -name "*.o"   ! -path "./stdlib/*" -delete
	@echo "[OK] Cleaned."

# -- Help ---------------------------------------------------------
help:
	@echo "Velocity Compiler - Kashmiri Edition v2"
	@echo ""
	@echo "Make targets:"
	@echo "  make              Build the compiler (Linux)"
	@echo "  make cross        Alias of cross-win"
	@echo "  make cross-win    Cross-compile velocity.exe for Windows"
	@echo "  make install      Install to /usr/local/bin + stdlib"
	@echo "  make uninstall    Remove installed files"
	@echo "  make test         Run test suite"
	@echo "  make capability-test Run capability suite (pass + xfail tracking)"
	@echo "  make clean        Remove build artefacts"
	@echo ""
	@echo "Compile a .vel file (Linux):"
	@echo "  ./velocity program.vel             # -> a.out"
	@echo "  ./velocity program.vel -o prog     # -> prog"
	@echo "  ./velocity program.vel -v          # verbose"
	@echo ""
	@echo "Compile for Windows (from Linux):"
	@echo "  make cross-win"
	@echo "  cp velocity.exe stdlib/ /mnt/windows-share/"
	@echo "  # On Windows: velocity.exe program.vel -o program.exe"
	@echo ""
	@echo "stdlib search path without install:"
	@echo "  VELOCITY_STDLIB=./stdlib ./velocity program.vel"
	@echo "  ./velocity program.vel --stdlib ./stdlib"
