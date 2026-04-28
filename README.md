# Velocity -- Kashmiri Edition v2

A compiled programming language with Kashmiri/Urdu keywords, targeting both **Linux ELF64** and **Windows PE64** natively.

```
.vel source -> Lexer -> Parser -> AST -> Codegen -> NASM -> ld/gcc -> binary
```

---

## What's New in v2

| Feature | Description |
|---|---|
| `uadad` | Unsigned 64-bit integer type |
| `uadad8` | Unsigned 8-bit integer (byte) |
| `&` `\|` `^` `~` `<<` `>>` | Bitwise operations |
| `&&` `\|\|` `!` | Logical AND / OR / NOT (`wa` / `ya` / `na` in Kashmiri) |
| `0xFF` `0b101` `0o77` | Hex, binary, octal integer literals |
| `+=` `-=` `*=` `/=` | Compound assignment |
| `koshish` / `ratt` | `try` / `catch` error handling |
| `panic("msg")` | Rust-style panic with message |
| `trayith expr` | Throw an error value |
| `arr.append(val)` | Dynamic array append (with correct `len()` tracking) |
| Struct array fields | `bina` fields can now hold arrays and nested structs |
| Chained field assign | `p.pos.x = 10.0` works correctly |
| `arr[i].field` | Field access on array elements works |
| `_` discard | `_ = expr;` evaluates and discards result |
| `else if` chains | `nate agar` chains work properly |
| Line/column errors | All errors show file, line, column, and source highlight |
| `--help` `--version` | Full CLI with flags |
| Windows PE64 | `--windows` flag generates Win64 ABI code |
| `--stdlib <path>` | Override stdlib search path |

---

## Keywords Reference

| Kashmiri | English | Meaning |
|---|---|---|
| `kar` | `fn` | Define a function |
| `chu` | `return` | Return from function |
| `ath` | `let` | Declare a variable |
| `mut` | `mut` | Make variable mutable |
| `agar` | `if` | Conditional |
| `nate` | `else` | Else branch |
| `yeli` | `while` | While loop |
| `bar` | `for` | For loop |
| `manz` / `in` | `in` | For-in iterator |
| `phutraw` | `break` | Break from loop |
| `pakh` | `continue` | Continue loop |
| `anaw` | `import` | Import a module |
| `bina` | `struct` | Define a struct |
| `koshish` | `try` | Try block |
| `ratt` | `catch` | Catch block |
| `panic` | `panic` | Fatal error with message |
| `trayith` | `throw` | Throw an error |
| `poz` | `true` | Boolean true |
| `apuz` | `false` | Boolean false |
| `null` | `null` | Null / empty |
| `wa` | `&&` | Logical AND |
| `ya` | `\|\|` | Logical OR |
| `na` | `!` | Logical NOT |

## Types Reference

| Type | Description |
|---|---|
| `adad` | Signed 64-bit integer |
| `uadad` | Unsigned 64-bit integer |
| `uadad8` | Unsigned 8-bit integer (byte, 0-255) |
| `ashari` | 64-bit float (f64) |
| `ashari32` | 32-bit float (f32) |
| `bool` | Boolean (`poz` / `apuz`) |
| `lafz` | UTF-8 string |
| `[type]` | Dynamic array |
| `[type; N]` | Fixed-size array of N elements |
| `(T1, T2)` | Tuple |
| `bina Name { ... }` | Struct |
| `type?` | Optional type |
| `khali` | Void return type |

---

## Build

### Linux (native)

```bash
make
```

Produces `./velocity`.

### Windows `.exe` from Linux (cross-compile)

```bash
sudo apt install mingw-w64
make cross-win
```

Produces `velocity.exe`. Copy it together with the `stdlib/` folder to Windows.

### Windows (native -- MSYS2/MinGW terminal)

```bash
gcc -O2 -std=c99 -o velocity.exe \
    main.c lexer.c parser.c codegen.c module.c
```

---

## Usage

```
velocity <source.vel> [options]

Options:
  -o, --output <file>    Output binary name (default: a.out / a.exe)
  -v, --verbose          Verbose: keep .asm/.o, print all commands
  -V, --version          Print version and exit
  -h, --help             Print help and exit
  --windows              Target Windows PE64 (MS x64 ABI)
  --linux                Target Linux ELF64 (System V AMD64)
  --stdlib <path>        Extra stdlib search path
  --emit-asm             Keep the generated .asm file
  --no-link              Stop after assembly (produce .o only)
```

### Examples

```bash
# Compile for Linux
./velocity hello.vel -o hello --stdlib ./stdlib
./hello

# Compile for Windows (on Linux)
./velocity hello.vel -o hello.exe --windows --stdlib ./stdlib
# Transfer hello.exe to Windows and run

# Verbose -- shows nasm + ld commands and keeps intermediate files
./velocity hello.vel -o hello --stdlib ./stdlib -v

# Override stdlib path via environment variable
VELOCITY_STDLIB=./stdlib ./velocity hello.vel -o hello
```

---

## Language Guide

### Variables

```vel
ath x = 42;              # immutable, type inferred (adad)
ath mut y: ashari = 3.14; # mutable float
ath name: lafz = "Velocity";
ath byte_val: uadad8 = 255;
```

### Functions

```vel
kar add(a: adad, b: adad) -> adad {
    chu a + b;
}

kar greet(name: lafz) -> khali {
    io.chhaap("Hello, %s!", name);
    io.chhaapLine();
}
```

### Control Flow

```vel
agar x > 0 {
    io.chhaap("positive");
} nate agar x == 0 {
    io.chhaap("zero");
} nate {
    io.chhaap("negative");
}
io.chhaapLine();
```

### Loops

```vel
# Range (exclusive)
bar i manz 0..10 {
    io.chhaap("%d", i);
}

# Range (inclusive)
bar i manz 1..=5 {
    io.chhaap("%d", i);
}

# Range with step
bar i manz 0..100 by 10 {
    io.chhaap("%d", i);
}

# While
yeli x > 0 {
    x -= 1;
}

# C-style for
bar (ath mut i = 0; i < 10; i += 1) {
    io.chhaap("%d", i);
}
```

### Arrays

```vel
# Fixed-size array -- stored inline on stack
ath mut fixed: [adad; 5] = [1, 2, 3, 4, 5];
fixed[2] = 99;
io.chhaap("len=%d", len(fixed));   # 5

# Dynamic array -- heap allocated, append-capable
ath mut dyn: [adad] = [];
dyn.append(10);
dyn.append(20);
dyn.append(30);
io.chhaap("len=%d val=%d", len(dyn), dyn[1]);   # len=3 val=20

# Iterate over array
bar item manz dyn {
    io.chhaap("%d", item);
    io.chhaapLine();
}
```

### Structs

```vel
bina Vec2 {
    x: ashari;
    y: ashari;
}

bina Particle {
    pos:  Vec2;          # nested struct -- works!
    vel:  Vec2;
    mass: ashari;
    tags: [lafz];        # array field -- works!
    id:   adad;
}

ath mut p = Particle {
    pos:  Vec2 { x: 1.0, y: 2.0 },
    vel:  Vec2 { x: 0.5, y: -0.3 },
    mass: 1.5,
    tags: [],
    id:   42
};

p.pos.x = 10.0;         # chained field assignment -- works!
p.tags.append("fast");
io.chhaap("id=%d x=%f", p.id, p.pos.x);
```

### Bitwise Operations

```vel
ath a = 0b1010;
ath b = 0xFF;
ath c = 0o17;

io.chhaap("%d", a & 0b1100);    # AND
io.chhaap("%d", a | b);         # OR
io.chhaap("%d", a ^ b);         # XOR
io.chhaap("%d", ~a);            # NOT
io.chhaap("%d", 1 << 4);        # SHL
io.chhaap("%d", 256 >> 3);      # SHR
```

### Error Handling

```vel
kar safe_divide(a: adad, b: adad) -> adad {
    agar b == 0 {
        panic("division by zero!");
    }
    chu a / b;
}

# try/catch
koshish {
    ath r = safe_divide(10, 0);
} ratt (err) {
    io.chhaap("caught: %s", err);
    io.chhaapLine();
}

# throw
trayith "something went wrong";
```

### Discard

```vel
_ = expensive_function();   # result discarded
```

---

## Modules (stdlib)

Import with `anaw`:

```vel
anaw io;
anaw lafz;
anaw math;
anaw random;
anaw time;
anaw system;
anaw os;
```

### io module

| Function | Description |
|---|---|
| `io.chhaap(fmt, ...)` | printf-style print (`%d` `%s` `%f` `%b` `%c` `%%`) |
| `io.chhaapLine()` | Print newline |
| `io.chhaapSpace()` | Print space |
| `io.stdin()` | Read line from stdin -> `lafz` |
| `io.input()` | Read integer from stdin (deprecated, use `stdin`) |

### lafz (string) module

| Function | Description |
|---|---|
| `lafz.len(s)` | String length |
| `lafz.eq(a, b)` | String equality |
| `lafz.concat(a, b)` | Concatenate strings |
| `lafz.slice(s, start, len)` | Substring |
| `lafz.from_adad(n)` | Integer to string |
| `lafz.from_ashari(f)` | Float to string |
| `lafz.to_adad(s)` | Parse integer |
| `lafz.to_ashari(s)` | Parse float |

### math module (via math.vel)

`sqrt`, `abs`, `floor`, `ceil`, `pow`, `sin`, `cos`, `tan`, `log`

### random module

`random.seed(n)`, `random.randrange(max)`, `random.random()` -> f64

### time module

`time.now()` (seconds), `time.now_ms()`, `time.now_ns()`, `time.sleep(ms)`, `time.ctime()`

### system module

`system.argc()`, `system.argv(i)`, `system.exit(code)`, `system.getenv(key)`

### os module

`os.getcwd()`, `os.chdir(path)`

---

## Standard Library Architecture

The stdlib `.asm` files are **dual-mode**: they assemble correctly for both Linux (`nasm -f elf64`) and Windows (`nasm -f win64`) using `%ifdef` blocks:

- **Linux**: raw Linux syscalls (`sys_write=1`, `sys_exit=60`, `clock_gettime=228` etc.)
- **Windows**: Win32 API calls (`WriteFile`, `ReadFile`, `GetStdHandle`, `ExitProcess`, `Sleep` etc.) with MS x64 ABI (args in `rcx`, `rdx`, `r8`, `r9`, shadow space)

The calling convention switch is automatic based on target:
- **Linux**: System V AMD64 ABI -- args in `rdi rsi rdx rcx r8 r9`
- **Windows**: Microsoft x64 ABI -- args in `rcx rdx r8 r9`, 32-byte shadow space

---

## Windows Setup Guide

### Prerequisites on Windows

1. **NASM** -- https://nasm.us/pub/nasm/releasebuilds/
   - Download the Windows installer
   - Add to PATH (e.g. `C:\Program Files\NASM`)

2. **GCC (MinGW-w64)** -- https://www.msys2.org/
   - Install MSYS2, then: `pacman -S mingw-w64-x86_64-gcc`
   - Add `C:\msys64\mingw64\bin` to PATH

3. **velocity.exe** + **stdlib/** folder -- copy from this project

### Verify setup

```cmd
nasm --version
gcc --version
velocity.exe --version
```

### Compile and run

```cmd
velocity.exe hello.vel -o hello.exe
hello.exe
```

### stdlib path

If velocity can't find the stdlib, use:

```cmd
velocity.exe hello.vel -o hello.exe --stdlib C:\path\to\stdlib
```

Or set the environment variable:

```cmd
set VELOCITY_STDLIB=C:\velocity\stdlib
velocity.exe hello.vel -o hello.exe
```

---

## Project Structure

```
velocity/
+-- velocity.h      Shared types, AST nodes, function declarations
+-- lexer.c         Tokenizer (handles all operators, literals, keywords)
+-- parser.c        AST builder (all language constructs)
+-- codegen.c       x86-64 NASM assembly emitter (Linux + Windows)
+-- module.c        Module/import resolution (cross-platform paths)
+-- main.c          Compiler driver + CLI argument parsing
+-- Makefile        Build system
+-- stdlib/
|   +-- io.asm      I/O (print/read) -- dual Win32/Linux
|   +-- lafz.asm    String utilities -- pure x86-64, no syscalls
|   +-- random.asm  PRNG (xorshift64*) -- pure x86-64, no syscalls
|   +-- system.asm  Process/argv/env -- dual Win32/Linux
|   +-- os.asm      Filesystem (cwd/chdir) -- dual Win32/Linux
|   +-- time.asm    Time/sleep -- dual Win32/Linux
|   +-- math.vel    Math functions (vel wrapper, calls libm)
|   +-- util.vel    Utility functions
+-- examples/
|   +-- hello.vel
|   +-- new_features.vel   All v2 features demo
|   +-- hashmap.vel
+-- tests/
    +-- hello.vel
    +-- arithmetic.vel
    +-- loops.vel
    +-- arrays.vel
    +-- structs.vel
    +-- bitwise.vel
```

---

## Author

Basit Ahmad Ganie  
basitahmed1412@gmail.com

MIT License -- Copyright (C) 2026
