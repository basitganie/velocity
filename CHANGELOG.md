# Velocity 0.3.0 Changelog

## New Language Features

### Pass-by-Reference (`&`)
Functions can now take reference parameters using `&`. The caller's variable
is mutated directly — no return value needed.

```vel
kar swap(&a: adad, &b: adad) -> khali {
    ath tmp = a;
    a = b;
    b = tmp;
}

kar increment(&n: adad) -> khali {
    n = n + 1;
}

ath mut x = 10;
ath mut y = 20;
swap(x, y);       # x=20, y=10
increment(x);     # x=21
```

### Generics (`<T>`)
Functions and structs can now be parameterized over types. Concrete
instantiations are monomorphized at compile time — no runtime overhead.

```vel
kar identity<T>(x: T) -> T { wapas x; }
kar add<T>(a: T, b: T) -> T { wapas a + b; }
kar gmax<T>(a: T, b: T) -> T {
    agar a > b { wapas a; }
    wapas b;
}

identity(42)      # → 42   (identity__adad emitted)
identity(3.14)    # → 3.14 (identity__ashari emitted)
add(1, 2)         # → 3
add(1.5, 2.5)     # → 4.0
gmax(7, 3)        # → 7
gmax(9.9, 3.1)    # → 9.9

bina Pair<T> {
    first:  T;
    second: T;
}
```

### `wapas` as return keyword
`wapas` (Kashmiri: "go back") is now accepted as a return keyword,
alongside the existing `chu`. Both work identically.

```vel
kar double(x: adad) -> adad { wapas x * 2; }  # same as: chu x * 2;
```

## Bug Fixes

### `io.chhaapLine(value)` for non-string types
`io.chhaapLine(integer)`, `io.chhaapLine(float)`, and `io.chhaapLine(bool)`
now correctly print the value followed by a newline. Previously they silently
dropped the value and printed only a blank line.

### Tuple return values now work end-to-end
`chu (a, b)` from a function returning `(T1, T2)` now correctly writes
through the sret pointer. Binding the result with `ath result = fn(...)` now
automatically detects the tuple type and copies all elements into local stack
slots, making `result.0` and `result.1` work correctly.

```vel
kar divmod(a: adad, b: adad) -> (adad, adad) {
    ath q = a / b;
    ath r = a - (q * b);
    wapas (q, r);
}

ath result = divmod(17, 5);
io.chhaap(result.0);    # 3
io.chhaap(result.1);    # 2
```

### `step` keyword collision with stdlib fixed
`step` was previously a reserved keyword, causing `stdlib/math.vel` to fail
compilation (line 539: `ath mut step = math__ulp(x)`). The range step
keyword is now `jadh` (Kashmiri: "pace/stride"); `step` is a free identifier
again. Both `jadh` and the original `by` keyword work in range expressions.
The legacy string `step` is also accepted contextually for backward
compatibility.

```vel
bar i manz 0..100 by 5   { ... }   # still works
bar i manz 0..100 jadh 5 { ... }   # new keyword
```

### `stdlib/math.vel` immutability fixes
Variables in math.vel that were declared `ath` but later reassigned
(e.g. inside `math__floor`, `math__ceil`, `math__log`, etc.) are now
correctly declared `ath mut`. All 4 previously failing math capability
tests now pass.

### `examples/hashmap.vel` — `io.stdin()` → `io.input()`
`get_key` was passing a `lafz` (string pointer) from `io.stdin()` to a
comparison expecting `adad`. Changed to `io.input()` which performs the
`atoi` conversion and returns an integer.

## New Standard Library Functions

### `io.read(fd, buf, len) -> adad`
Reads up to `len` bytes from file descriptor `fd` into `buf`.
Returns the number of bytes actually read, or -1 on error.
Works on both Linux (sys_read) and Windows (ReadFile).

```vel
ath fd = io.open("data.txt", 0);
ath mut buf = "                    ";  # 20-char buffer
ath n = io.read(fd, buf, 20);
io.close(fd);
```

### `io.write(fd, buf, len) -> adad`
Writes `len` bytes from `buf` to file descriptor `fd`.
Returns the number of bytes written, or -1 on error.
Works on both Linux (sys_write) and Windows (WriteFile).

## Regression Test Results

```
cap_01 through cap_26: 26/26 PASSED
test_ref (pass-by-reference):  PASSED
test_generics (6 cases):       PASSED
test_tuple (divmod):           PASSED
test_io_read:                  PASSED
test_wapas:                    PASSED
```

## AArch64-Linux-Android Target (`--target aarch64-linux-android`)

Velocity can now cross-compile to AArch64 (ARM 64-bit) for Linux/Android
using GNU as or the Android NDK toolchain.

### Usage

```bash
# With GNU cross toolchain (apt install gcc-aarch64-linux-gnu qemu-user):
export VELOCITY_NDK_AS=aarch64-linux-gnu-as
export VELOCITY_NDK_CLANG=aarch64-linux-gnu-gcc
velc hello.vel -o hello --stdlib stdlib --target aarch64-linux-android

# With Android NDK:
export VELOCITY_NDK_AS=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android-as
export VELOCITY_NDK_CLANG=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang
velc hello.vel -o hello --stdlib stdlib --target aarch64-linux-android
```

### What's included

- **GNU as `.s` output** — clean AArch64 assembly, no NASM
- **AAPCS64 calling convention** — `x0–x7` for integer args, `d0–d7` for floats
- **Raw Linux syscalls** — no libc dependency, fully static binaries
- **`stdlib/io_aarch64.s`** — complete io module port:
  - `io__chhaap` — format-string print (`%lld`, `%s`, `%f`) via raw write syscall
  - `io__chhaapLine` — newline print
  - `io__stdin` / `io__input` — line and integer input via `read` syscall
  - `io__open` / `io__close` — file open/close via `openat`/`close` syscalls
  - `io__read` / `io__write` — file read/write via `read`/`write` syscalls
  - `__vl_print_int` — integer-to-decimal conversion (no libc)
  - `__vl_print_float` — float-to-decimal conversion with 6dp (no libc)
- **Float arithmetic** — `fadd/fsub/fmul/fdiv/fcmp` via `d0`-`d1` registers
- **Generic monomorphization** — same `<T>` generics work on AArch64
- **Pass-by-reference** — `&` params use `sub x0, x29, #offset` / `ldr x0,[x0]`
- **`_start` entry** — `bl main` + `mov x8, #93 / svc #0` for clean exit

### Verified under QEMU

All tests pass under `qemu-aarch64`:
- Integer arithmetic: `5`, `6`, `42`, `3`, correct
- Generic `add<T>`: integers and floats both correct
- Generic `gmax<T>`: `7` (int), `9.900000` (float), correct
- Recursive `fib(10) = 55`, `10! = 3628800`
- Pass-by-reference `swap` and `increment`
- While loops, conditionals, negative numbers
