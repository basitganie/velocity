# Velocity

**Velocity** is a small, compiled programming language implemented in C.
It features a minimal syntax, a custom compiler pipeline (lexer → parser → codegen), and a lightweight standard library with direct system-level integrations.

This project is designed as both:

* a **learning-grade compiler implementation**, and
* a **practical experimental language** with real features (arrays, structs, pipelines, etc.)

---

## Overview

Velocity compiles `.vel` source files into executable binaries via a simple toolchain:

```
.vel source → Lexer → Parser → Codegen → Native binary
```

Core properties:

* Ahead-of-time compiled
* Statically typed (inferred from usage patterns)
* Minimal runtime
* Assembly-backed standard library

---

## Features

### Language

* Arithmetic & expressions
* Booleans and casting
* Strings
* Arrays & tuples
* Structs
* Optionals (`null`)
* Loops
* System arguments (`argv`)
* Functional-style utilities (`map`, `filter`)
* `sizeof` / type introspection

## keywords

| Keyword          | Meaning  | English Equivalent |
| ---------------- | -------- | ------------------ |
| `kar`            | function | `fn`               |
| `chu`            | return   | `return`           |
| `ath`            | declare  | `let`              |
| `mut`            | mutable  | `mut`              |
| `agar`           | if       | `if`               |
| `nate`           | else     | `else`             |
| `yeli`           | while    | `while`            |
| `bar` / `dohraw` | loop     | `for`              |
| `in` / `manz`    | in       | `in`               |
| `phutraw`        | break    | `break`            |
| `pakh`           | continue | `continue`         |
| `anaw`           | import   | `import`           |
| `bina`           | struct   | `structure`        |

## Types

| Type       | Keyword    | Description           |
| ---------- | ---------- | --------------------- |
| Integer    | `adad`     | 32-bit signed integer |
| Boolean    | `bool`     | `poz` / `apuz`        |
| Float (64) | `ashari`   | Double precision      |
| Float (32) | `ashari32` | Single precision      |
| String     | `lafz`     | UTF-8 string          |

## Booleans

| Literal | Meaning      |
| ------- | ------------ |
| `poz`   | true         |
| `apuz`  | false        |
| `null`  | null / empty |


### Compiler

* Handwritten lexer and parser
* Modular compilation pipeline
* Custom code generation

### Standard Library

Located in `stdlib/`, includes:

* I/O
* Math utilities
* Random
* Time
* OS/system bindings
* SDL bindings (native + `.vel` wrappers)

---

## Project Structure

```
velocity/
├── main.c          # Entry point (compiler driver)
├── lexer.c         # Tokenizer
├── parser.c        # AST builder
├── codegen.c       # Code generation
├── module.c        # Module handling
├── velocity.h      # Shared definitions
├── stdlib/         # Standard library (ASM + .vel)
├── examples/       # Example programs
├── Makefile        # Build system
└── velocity        # Compiled binary
```

---

## Build

```bash
make
```

This produces the `velocity` compiler binary.

---

## Usage

Compile a Velocity file:

```bash
./velocity file.vel -o out -v
```

## Example

```vel
anaw io;

kar main() -> adad {
    ath x = 10;
    ath y = 20;
    io.chhaap(x + y);
}
```

More examples:

* `examples/example1.vel`
* `examples/pipelines.vel`

---

## Language Notes

### Variables

Implicitly declared via assignment:

```vel
ath mut x = 5
```

### Control Flow

```vel
bar i manz 0..10 {
    io.chhaap(i)
}
```

### Optionals

```vel
ath x: adad? = null
```

### Structs

```vel
bina Point {
    x: ashari32;
    y: ashari32;
}
```

### Functional Utilities

```vel
arr.map(fn(x) { x * 2 })
```

---

## Standard Library Design

The standard library mixes:

* `.vel` high-level wrappers
* `.asm` low-level implementations

This keeps:

* performance tight
* abstractions clean

---

## Contributing

This project is intentionally simple and hackable.

Suggested areas:

* better error messages
* optimizer passes
* richer type system
* improved stdlib
* cross-platform support

---

## License

MIT License

Copyright © 2026 Basit Ahmad Ganie
