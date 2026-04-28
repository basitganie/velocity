    .arch armv8-a
    .text

// lafz__len(s: x0) -> x0: adad
// Returns byte length of null-terminated string.
    .global lafz__len
lafz__len:
    mov  x1, x0
1:  ldrb w2, [x1], #1
    cbnz w2, 1b
    sub  x0, x1, x0
    sub  x0, x0, #1
    ret

// lafz__eq(a: x0, b: x1) -> x0: bool (1=equal, 0=not)
    .global lafz__eq
lafz__eq:
    stp  x29, x30, [sp, #-16]!
    mov  x29, sp
1:  ldrb w2, [x0], #1
    ldrb w3, [x1], #1
    cmp  w2, w3
    bne  .Leq_ne
    cbnz w2, 1b
    mov  x0, #1
    b    .Leq_done
.Leq_ne:
    mov  x0, #0
.Leq_done:
    ldp  x29, x30, [sp], #16
    ret

// lafz__concat(a: x0, b: x1) -> x0: lafz
// Allocates new string, copies a then b into it.
    .global lafz__concat
lafz__concat:
    stp  x29, x30, [sp, #-48]!
    stp  x19, x20, [sp, #16]
    stp  x21, x22, [sp, #32]
    mov  x29, sp
    mov  x19, x0       // a
    mov  x20, x1       // b

    // len(a)
    mov  x0, x19
    bl   lafz__len
    mov  x21, x0       // len_a

    // len(b)
    mov  x0, x20
    bl   lafz__len
    mov  x22, x0       // len_b

    // allocate len_a + len_b + 1 bytes via mmap
    add  x1, x21, x22
    add  x1, x1, #1
    mov  x0, #0
    mov  x2, #3        // PROT_READ|PROT_WRITE
    mov  x3, #0x22     // MAP_PRIVATE|MAP_ANONYMOUS
    mov  x4, #-1
    mov  x5, #0
    mov  x8, #222      // __NR_mmap
    svc  #0
    mov  x9, x0        // dest ptr

    // copy a
    mov  x0, x9
    mov  x1, x19
    mov  x2, x21
    bl   .Lmemcpy

    // copy b after a
    add  x0, x9, x21
    mov  x1, x20
    mov  x2, x22
    bl   .Lmemcpy

    // null terminate
    add  x10, x9, x21
    add  x10, x10, x22
    mov  w11, #0
    strb w11, [x10]

    mov  x0, x9
    ldp  x21, x22, [sp, #32]
    ldp  x19, x20, [sp, #16]
    ldp  x29, x30, [sp], #48
    ret

// lafz__slice(s: x0, start: x1, end: x2) -> x0: lafz
// Returns new string containing s[start..end]
    .global lafz__slice
lafz__slice:
    stp  x29, x30, [sp, #-32]!
    stp  x19, x20, [sp, #16]
    mov  x29, sp
    mov  x19, x0       // s
    // len = end - start
    sub  x20, x2, x1   // len
    add  x9, x19, x1   // src = s + start

    // allocate
    add  x1, x20, #1
    mov  x0, #0
    mov  x2, #3
    mov  x3, #0x22
    mov  x4, #-1
    mov  x5, #0
    mov  x8, #222
    svc  #0
    mov  x10, x0

    // copy len bytes
    mov  x0, x10
    mov  x1, x9
    mov  x2, x20
    bl   .Lmemcpy

    // null terminate
    add  x11, x10, x20
    mov  w12, #0
    strb w12, [x11]

    mov  x0, x10
    ldp  x19, x20, [sp, #16]
    ldp  x29, x30, [sp], #32
    ret

// lafz__to_adad(s: x0) -> x0: adad
// Parses decimal integer string.
    .global lafz__to_adad
lafz__to_adad:
    mov  x9, x0
    mov  x0, #0
    mov  x10, #10
    mov  x11, #0      // negative flag
    ldrb w12, [x9]
    cmp  w12, #45     // '-'
    bne  .Lta_loop
    mov  x11, #1
    add  x9, x9, #1
.Lta_loop:
    ldrb w12, [x9], #1
    cbz  w12, .Lta_done
    sub  w12, w12, #48
    tst  w12, #0xfffffff0
    bne  .Lta_done
    mul  x0, x0, x10
    add  x0, x0, x12
    b    .Lta_loop
.Lta_done:
    cbz  x11, .Lta_ret
    neg  x0, x0
.Lta_ret:
    ret

// lafz__from_adad(n: x0) -> x0: lafz
// Converts integer to string.
    .global lafz__from_adad
lafz__from_adad:
    b    lafz__from_adad_v2
    .global lafz__from_adad_v2
lafz__from_adad_v2:
    stp  x29, x30, [sp, #-32]!
    stp  x19, x20, [sp, #16]
    mov  x29, sp
    mov  x19, x0       // save n

    mov  x0, #0
    mov  x1, #24
    mov  x2, #3
    mov  x3, #0x22
    mov  x4, #-1
    mov  x5, #0
    mov  x8, #222
    svc  #0
    mov  x9, x0

    // Convert x19 to string in buf x9
    add  x10, x9, #22
    mov  w11, #0
    strb w11, [x10]

    mov  x12, #0       // negative flag
    cmp  x19, #0
    bge  .Lfad_pos
    neg  x19, x19
    mov  x12, #1

.Lfad_pos:
    mov  x13, #10
    cbz  x19, .Lfad_zero
.Lfad_loop:
    cbz  x19, .Lfad_sign
    udiv x14, x19, x13
    msub x15, x14, x13, x19
    sub  x10, x10, #1
    add  w15, w15, #48
    strb w15, [x10]
    mov  x19, x14
    b    .Lfad_loop
.Lfad_zero:
    sub  x10, x10, #1
    mov  w14, #48
    strb w14, [x10]
.Lfad_sign:
    cbz  x12, .Lfad_done
    sub  x10, x10, #1
    mov  w14, #45
    strb w14, [x10]
.Lfad_done:
    mov  x0, x10
    ldp  x19, x20, [sp, #16]
    ldp  x29, x30, [sp], #32
    ret

// lafz__to_ashari(s: x0) -> d0: ashari (stub, returns 0.0)
    .global lafz__to_ashari
lafz__to_ashari:
    fmov d0, xzr
    ret

// lafz__from_ashari(f: d0) -> x0: lafz (stub, returns "0.0")
    .global lafz__from_ashari
lafz__from_ashari:
    adr  x0, .Lzero_str
    ret
    .align 2
.Lzero_str: .asciz "0.0"

// ------------------------------------------------------------
// .Lmemcpy(dst: x0, src: x1, len: x2) -- local helper, no bl out
    .align 2
.Lmemcpy:
    cbz  x2, .Lmc_done
1:  ldrb w3, [x1], #1
    strb w3, [x0], #1
    sub  x2, x2, #1
    cbnz x2, 1b
.Lmc_done:
    ret

// ------------------------------------------------------------
