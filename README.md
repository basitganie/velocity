# Velocity — Minimal Programming Language

A compiled programming language with Koshur keywords.
Compiles directly to x86-64 Linux binaries via NASM.

## Kashmiri Keywords

| Keyword | Meaning     | English     |
|---------|-------------|-------------|
| kar     | کار (work)  | function    |
| chu     | چھ (is)    | return      |
| ath     | اَتھ (this) | let         |
| agar    | اگر         | if          |
| nate    | نَتہ (else) | else        |
| yeli    | یلہ (when)  | while       |
| bar/dohraw | (repeat) | for         |
| in/manz | (in)        | in          |
| break / phutraw   | (stop)      | break       |
| continue / pakh   | (skip)      | continue    |
| anaw    | اَناو (bring)| import     |
| adad    | عدد (number)| int / i32   |
| bool    | (poz(true)/apuz(false)| boolean    |
| ashari  | (float)      | float / f64 |
| ashari32| (float32)    | float / f32 |
| lafz    | (word)       | string      |

### Literals
`poz` (true), `apuz` (false), and `null`.

## Quick Start

### Requirements
```bash
sudo apt install gcc nasm make     # Ubuntu/Debian
sudo dnf install gcc nasm make     # Fedora
sudo pacman -S gcc nasm make       # Arch
```

### Build
```bash
cd velocity
make
```

### Hello World
```bash
cat > hello.vel << 'END'
anaw io;

kar main() -> adad {
    io.chhaap("Hello, World!\n");
    chu 0;
}
END

./velocity hello.vel -o hello
./hello
```

### Run Examples
```bash
./velocity examples/example1.vel -o out && ./out && echo "exit: $?"
./velocity examples/example4.vel -o out && ./out && echo "exit: $?"  # factorial
./velocity examples/example5.vel -o out && ./out && echo "exit: $?"  # fibonacci
```

## Language Guide

### Functions
```kashmiri
kar jama(a: adad, b: adad) -> adad {
    chu a + b;
}
```

### Variables
```kashmiri
ath x = 10;
ath y = 20;
```

With mutability:
```kashmiri
ath mut x = 10;
x = x + 1;
```
By default, variables are immutable. Use `mut` to allow reassignment.

### Booleans
```kashmiri
ath ok: bool = poz;
ath nope: bool = apuz;
io.chhaap("%b\n", ok);
```

### Null and Optional
Use `null` for absence. Optional types are written with `?` (currently only `lafz?` is supported).
```kashmiri
ath maybe: lafz? = null;

agar maybe != null {
    io.chhaap("%s\n", maybe);
}
```

### Floats
```kashmiri
ath x: ashari = 3.14;
ath y: ashari32 = 2.5;
io.chhaap("%f\n", x + y);
```

### Arrays (fixed-size)
```kashmiri
ath nums: [adad; 3] = [10, 20, 30];
io.chhaap("%d\n", nums[1]);
nums[1] = 99;
```

### Arrays (dynamic-length)
```kashmiri
ath nums: [adad] = [1, 2, 3, 4];
io.chhaap("%d\n", nums[3]);
```

### Tuples
```kashmiri
ath pair: (adad, lafz) = (7, "saat");
io.chhaap("%d %s\n", pair.0, pair.1);
```

Notes:
Arrays/tuples require explicit type annotations.
Dynamic arrays use `[T]` syntax; fixed arrays use `[T; N]`.
Tuples are supported in function parameters and return types.
Fixed-size arrays are not supported in function parameters or return types yet.

### If / Else
```kashmiri
agar x > 10 {
    chu x;
} nate {
    chu 0;
}
```

### While Loop
```kashmiri
ath i = 0;
yeli i < 10 {
    i = i + 1;
}
```

### For Loops
```kashmiri
# C-style
bar (ath mut i: adad = 0; i < 5; i = i + 1) {
    io.chhaap("%d\n", i);
}

# For-in range
bar i in 0..5 {
    io.chhaap("%d\n", i);
}

# Inclusive range
bar i in 0..=5 {
    io.chhaap("%d\n", i);
}

# Step ranges
bar i in 0..10 by 2 {
    io.chhaap("%d\n", i);
}

bar i in 0..=10 step 3 {
    io.chhaap("%d\n", i);
}

# For-in array
ath nums: [adad; 3] = [10, 20, 30];
bar n in nums {
    io.chhaap("%d\n", n);
}

# For-in array literal (type inferred)
bar v in [1, 2, 3] {
    io.chhaap("%d\n", v);
}

# For-in array-returning call
bar w in make_nums() {
    io.chhaap("%d\n", w);
}

# break / continue
bar (ath mut i: adad = 0; i < 10; i = i + 1) {
    agar i == 3 { continue; }
    agar i == 7 { break; }
    io.chhaap("%d\n", i);
}
```

### Imports
```kashmiri
anaw math;
anaw io;

kar main() -> adad {
    ath result = math.shakti(2, 10);   # 2^10 = 1024
    io.chhaap("%d\n", result);
    chu 0;
}
```

### Strings (lafz)
```kashmiri
anaw lafz;
anaw io;

kar main() -> adad {
    ath a: lafz = "Salaam";
    ath b: lafz = "Dost";
    ath mut c: lafz = lafz.concat(a, " ");
    c = lafz.concat(c, b);
    io.chhaap("%s\n", c);
    io.chhaap("len=%d\n", lafz.len(c));
    io.chhaap("eq=%d\n", lafz.eq(a, b));
    io.chhaap("%s\n", lafz.slice(c, 0, 5));
    io.chhaap("%s\n", lafz.from_adad(123));
    io.chhaap("%s\n", lafz.from_ashari(3.14));
    chu 0;
}
```

## Standard Library

### io (native — stdlib/io.asm)
| Function              | Description                  |
|-----------------------|------------------------------|
| io.chhaap(fmt, ...)   | Print formatted output (%d %s %c %f %b %% , up to 5 args) |
| io.chhaapLine()       | Print newline                |
| io.chhaapSpace()      | Print a space                |
| io.input()            | Read integer from stdin (deprecated) |
| io.stdin()            | Read line as lafz            |

**Example - Print multiple numbers:**
```kashmiri
anaw io;

kar main() -> adad {
    io.chhaap("%d %d %d\n", 10, 20, 30);  # Output: 10 20 30
    chu 0;
}
```

**Example - Read input:**
```kashmiri
anaw io;

kar main() -> adad {
    ath name: lafz = io.stdin();  # Read line as string
    io.chhaap("Name: %s\n", name);
    chu 0;
}
```

### math (stdlib/math.vel)
| Function                       | Description              |
|--------------------------------|--------------------------|
| math.mutlaq(x)                 | Absolute value           |
| math.azeem(a, b)               | Maximum of two           |
| math.asgar(a, b)               | Minimum of two           |
| math.shakti(base, exp)         | Power (base^exp)         |
| math.jar(n)                    | Integer square root      |
| math.zarb_tartib(n)            | Factorial                |
| math.fibonacci(n)              | Nth Fibonacci number     |
| math.mushtarak_qasim(a, b)     | GCD                      |
| math.mushtarak_mutaaddid(a, b) | LCM                      |
| math.awwal_chu(n)              | Is prime? (1=yes, 0=no)  |
| math.jama_tak(n)               | Sum 1..n                 |
| math.joft_chu(n)               | Is even? (1=yes, 0=no)   |
| math.taaq_chu(n)               | Is odd? (1=yes, 0=no)    |

### lafz (stdlib/lafz.asm)
| Function                 | Description                 |
|--------------------------|-----------------------------|
| lafz.len(s)              | String length               |
| lafz.eq(a, b)            | String equality (1/0)       |
| lafz.concat(a, b)        | Concatenate strings         |
| lafz.slice(s, start, len)| Substring                   |
| lafz.from_adad(n)        | Int to string               |
| lafz.from_ashari(f)      | Float to string (6 decimals)|

## Writing Your Own Libraries

### Pure Velocity library (mylib.vel)
```kashmiri
kar double(x: adad) -> adad {
    chu x * 2;
}

kar square(x: adad) -> adad {
    chu x * x;
}
```

### Use it
```kashmiri
anaw mylib;

kar main() -> adad {
    io.chhaap("%d\n", mylib.square(7));   # prints 49
    chu 0;
}
```

Place `mylib.vel` in the same directory as your source file.

## Module Search Order

1. Same directory as source file
2. Current working directory
3. `./stdlib/` subdirectory
4. Directory where `velocity` binary lives
5. `<binary dir>/stdlib/`
6. `$VELOCITY_STDLIB` env var (if set)
7. `/usr/local/lib/velocity` (system install)

## Install System-Wide
```bash
sudo make install
# compiler -> /usr/local/bin/velocity
# stdlib   -> /usr/local/lib/velocity/
```

## Project Structure
```
velocity/
├── velocity.h          # all types and declarations
├── main.c              # compiler driver
├── lexer.c             # tokenizer
├── parser.c            # recursive descent parser
├── codegen.c           # x86-64 assembly generator
├── module.c            # module/import system
├── Makefile
├── stdlib/
│   ├── io.asm          # native I/O (Linux syscalls)
│   └── math.vel        # math functions
└── examples/
    ├── example1.vel    # arithmetic
    ├── example2.vel    # functions
    ├── example3.vel    # conditionals
    ├── example4.vel    # factorial (recursion)
    ├── example5.vel    # fibonacci (loop)
    └── example_math.vel # using math library
```
