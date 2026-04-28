

    .arch armv8-a
    .text

__vl_write_str:
    stp  x29, x30, [sp, #-16]!
    mov  x29, sp
    mov  x2, x1          // // len 
    mov  x1, x0          // // buf 
    mov  x0, #1          // // fd=stdout 
    mov  x8, #64         // // __NR_write 
    svc  #0
    ldp  x29, x30, [sp], #16
    ret

// __vl_printf(fmt:x0, a1:x1, ..., a7:x7)
// Frame: 128 bytes. Arg slots at [sp+48]..[sp+96].
    .global __vl_printf
    .global io__printf
__vl_printf:
io__printf:
    stp  x29, x30, [sp, #-128]!
    stp  x19, x20, [sp, #16]
    stp  x21, x22, [sp, #32]
    mov  x29, sp
    mov  x19, x0
    str  x1,  [sp, #48]
    str  x2,  [sp, #56]
    str  x3,  [sp, #64]
    str  x4,  [sp, #72]
    str  x5,  [sp, #80]
    str  x6,  [sp, #88]
    str  x7,  [sp, #96]
    mov  x20, #0
.Lpf_loop:
    ldrb w21, [x19], #1
    cbz  w21, .Lpf_done
    cmp  w21, #37; bne .Lpf_literal
    ldrb w21, [x19], #1
    cmp  w21, #37;  beq .Lpf_literal
    cmp  w21, #100; beq .Lpf_int
    cmp  w21, #115; beq .Lpf_str
    cmp  w21, #102; beq .Lpf_flt
    cmp  w21, #98;  beq .Lpf_bool
    cmp  w21, #108; beq .Lpf_long
    b    .Lpf_literal
.Lpf_long:
    ldrb w21, [x19], #1
    cmp  w21, #108; beq .Lpf_long2
    b    .Lpf_int
.Lpf_long2:
    ldrb w21, [x19], #1
    b    .Lpf_int
.Lpf_int:
    lsl  x9, x20, #3
    add  x9, x9, #48
    ldr  x0, [sp, x9]
    add  x20, x20, #1
    bl   __vl_print_int
    b    .Lpf_loop
.Lpf_str:
    lsl  x9, x20, #3
    add  x9, x9, #48
    ldr  x0, [sp, x9]
    add  x20, x20, #1
    str  x0, [sp, #104]    // save ptr on stack (scratch slot)
    bl   __vl_strlen
    mov  x2, x0
    ldr  x1, [sp, #104]    // restore ptr from stack
    mov  x0, #1; mov x8, #64; svc #0
    b    .Lpf_loop
.Lpf_flt:
    lsl  x9, x20, #3
    add  x9, x9, #48
    ldr  x0, [sp, x9]
    add  x20, x20, #1
    fmov d0, x0
    bl   __vl_print_float
    b    .Lpf_loop
.Lpf_bool:
    lsl  x9, x20, #3
    add  x9, x9, #48
    ldr  x0, [sp, x9]
    add  x20, x20, #1
    cbnz x0, .Lpf_bool_t
    adr  x1, .Lpf_naa; mov x2, #3; b .Lpf_bw
.Lpf_bool_t:
    adr  x1, .Lpf_poz; mov x2, #3
.Lpf_bw:
    mov  x0, #1; mov x8, #64; svc #0
    b    .Lpf_loop
.Lpf_literal:
    strb w21, [sp, #112]
    add  x1, sp, #112
    mov  x0, #1; mov x2, #1; mov x8, #64; svc #0
    b    .Lpf_loop
.Lpf_done:
    ldp  x21, x22, [sp, #32]
    ldp  x19, x20, [sp, #16]
    ldp  x29, x30, [sp], #128
    ret
    .align 2
.Lpf_poz: .ascii "poz"
.Lpf_naa: .ascii "naa"

    .text
    .align 2
.global __vl_strlen
__vl_strlen:
    mov  x1, x0
1:  ldrb w2, [x1], #1
    cbnz w2, 1b
    sub  x0, x1, x0
    sub  x0, x0, #1
    ret

__vl_print_int:
    stp  x29, x30, [sp, #-64]!
    mov  x29, sp
    // // buf[21] at [sp+16..36] 
    mov  x9, x0          // // value 
    add  x10, sp, #36    // // end of buffer 
    mov  x11, #0
    strb w11, [x10]      // // null terminator 

    // // Handle negative 
    mov  x12, #0         // // negative flag 
    cmp  x9, #0
    bge  .Lpos
    neg  x9, x9
    mov  x12, #1

.Lpos:
    // // Handle zero 
    cbnz x9, .Ldigits
    sub  x10, x10, #1
    mov  w11, #48        // // '0' 
    strb w11, [x10]
    b    .Lprint

.Ldigits:
    mov  x13, #10
.Lloop:
    cbz  x9, .Ldone
    udiv x14, x9, x13
    msub x15, x14, x13, x9   // // remainder 
    sub  x10, x10, #1
    add  w15, w15, #48
    strb w15, [x10]
    mov  x9, x14
    b    .Lloop

.Ldone:
    cbz  x12, .Lprint
    sub  x10, x10, #1
    mov  w11, #45         // // '-' 
    strb w11, [x10]

.Lprint:
    // // x10 = start, compute len 
    add  x1, sp, #37     // // one past end (after null) 
    sub  x1, x1, x10
    sub  x1, x1, #1      // // exclude null 
    mov  x0, x10
    bl   __vl_write_str

    ldp  x29, x30, [sp], #64
    ret

__vl_print_float:
    stp  x29, x30, [sp, #-64]!
    stp  x19, x20, [sp, #16]
    stp  x21, x22, [sp, #32]
    mov  x29, sp

    // // Check sign 
    fmov x19, d0
    asr  x20, x19, #63             // // x20 = sign (-1 or 0) 
    cbz  x20, .Lflt_pos
    // // negative: negate and print '-' 
    fneg d0, d0
    adr  x0, __vl_minus
    mov  x1, #1
    bl   __vl_write_str

.Lflt_pos:
    // // Integer part 
    fcvtzs x21, d0                 // // x21 = trunc(d0) as integer 
    scvtf  d1, x21                 // // d1 = (double)x21 
    fsub   d2, d0, d1              // // d2 = fractional part 

    mov  x0, x21
    bl   __vl_print_int

    // // Print decimal point 
    adr  x0, __vl_dot
    mov  x1, #1
    bl   __vl_write_str

    // // Fractional part: multiply by 1000000 then print as integer with leading zeros 
    movz x22, #0x4240, lsl #0
    movk x22, #0xF, lsl #16
    scvtf d3, x22
    fmul  d2, d2, d3
    fcvtzs x21, d2                 
    // // abs 
    cmp  x21, #0
    bge  .Lflt_frac_ok
    neg  x21, x21
.Lflt_frac_ok:
    // // Print 6 digits with leading zeros 
    mov  x22, #6                   // // digit count 
    add  x10, sp, #56             // // end of 7-byte buf 
    mov  w11, #0
    strb w11, [x10]               // // null terminator 
    mov  x13, #10
.Lflt_frac_loop:
    sub  x10, x10, #1
    udiv x14, x21, x13
    msub x15, x14, x13, x21      // // digit 
    add  w15, w15, #48
    strb w15, [x10]
    mov  x21, x14
    sub  x22, x22, #1
    cbnz x22, .Lflt_frac_loop

    mov  x0, x10
    mov  x1, #6
    bl   __vl_write_str

    ldp  x21, x22, [sp, #32]
    ldp  x19, x20, [sp, #16]
    ldp  x29, x30, [sp], #64
    ret

    .align 2
__vl_minus: .byte 45, 0     // // '-' 
__vl_dot:   .byte 46, 0     // // '.' 

    .global io__chhaap
io__chhaap:
    stp  x29, x30, [sp, #-48]!
    stp  x19, x20, [sp, #16]      // // save callee-saved regs 
    mov  x29, sp
    mov  x19, x0                   // // x19 = fmt ptr (survives bl) 
    mov  x20, x1                   // // x20 = value   (survives bl) 

.Lfmt_loop:
    ldrb w10, [x19], #1            // // fetch next fmt char 
    cbz  w10, .Lfmt_done
    cmp  w10, #37                  // // '%' ? 
    bne  .Lfmt_literal

    // // got '%' — check specifier 
    ldrb w10, [x19], #1
    cmp  w10, #115                 // // 's' 
    beq  .Lfmt_str
    cmp  w10, #100                 // // 'd' 
    beq  .Lfmt_int
    cmp  w10, #98                  // // 'b' (bool) 
    beq  .Lfmt_int
    cmp  w10, #102                 // // 'f' 
    beq  .Lfmt_float
    cmp  w10, #108                 // // 'l' → 'ld' or 'lld' 
    beq  .Lfmt_long
    // // unknown specifier — skip 
    b    .Lfmt_loop

.Lfmt_long:
    ldrb w10, [x19], #1           // // consume 'l' or 'd' 
    cmp  w10, #108                 // // another 'l'? 
    beq  .Lfmt_long2
    // // was 'ld' — print int 
    b    .Lfmt_int
.Lfmt_long2:
    ldrb w10, [x19], #1           // consume 'd'
    b    .Lfmt_int                 // print as integer (was falling into .Lfmt_float)
.Lfmt_float:
    // // x20 holds raw bits of the double (via fmov x1,d0 at call site) 
    fmov d0, x20
    bl   __vl_print_float          // // x19, x20 preserved (callee-saved) 
    b    .Lfmt_loop

.Lfmt_int:
    mov  x0, x20
    bl   __vl_print_int            // // x19, x20 preserved (callee-saved) 
    b    .Lfmt_loop

.Lfmt_str:
    mov  x0, x20
    bl   __vl_strlen               // // x19, x20 preserved 
    mov  x1, x0
    mov  x0, x20
    bl   __vl_write_str            // // x19, x20 preserved 
    b    .Lfmt_loop

.Lfmt_literal:
    // // x10 = the character, x19 already past it — write 1 byte 
    // // store char on stack and write from there 
    strb w10, [sp, #40]
    add  x0, sp, #40
    mov  x1, #1
    bl   __vl_write_str            // // x19, x20 preserved 
    b    .Lfmt_loop

.Lfmt_done:
    ldp  x19, x20, [sp, #16]      // // restore callee-saved 
    ldp  x29, x30, [sp], #48
    ret

    .global io__chhaapLine
io__chhaapLine:
    stp  x29, x30, [sp, #-16]!
    mov  x29, sp
    adr  x0, __vl_nl
    mov  x1, #1
    bl   __vl_write_str
    ldp  x29, x30, [sp], #16
    ret

    .align 2
__vl_nl: .byte 10, 0

    .global io__chhaapSpace
io__chhaapSpace:
    stp  x29, x30, [sp, #-16]!
    mov  x29, sp
    adr  x0, __vl_sp
    mov  x1, #1
    bl   __vl_write_str
    ldp  x29, x30, [sp], #16
    ret

    .align 2
__vl_sp: .byte 32, 0

    .global io__stdin
io__stdin:
    stp  x29, x30, [sp, #-16]!
    mov  x29, sp
    adr  x0, __vl_input_buf
    mov  x1, #255
    mov  x2, x1
    mov  x1, x0
    mov  x0, #0            // // fd=stdin 
    mov  x8, #63           // // __NR_read 
    svc  #0
    // // strip trailing newline 
    adr  x9, __vl_input_buf
    add  x10, x9, x0
    sub  x10, x10, #1
    ldrb w11, [x10]
    cmp  w11, #10
    bne  .Lstdin_done
    mov  w11, #0
    strb w11, [x10]
.Lstdin_done:
    adr  x0, __vl_input_buf
    ldp  x29, x30, [sp], #16
    ret

    .global io__input
io__input:
    stp  x29, x30, [sp, #-16]!
    mov  x29, sp
    bl   io__stdin
    // // parse integer from x0 (string ptr) 
    mov  x9, x0
    mov  x0, #0            // // result 
    mov  x10, #10          // // base 
    mov  x11, #0           // // negative flag 

    ldrb w12, [x9]
    cmp  w12, #45          // // '-' 
    bne  .Lparse_loop
    mov  x11, #1
    add  x9, x9, #1

.Lparse_loop:
    ldrb w12, [x9], #1
    cbz  w12, .Lparse_done
    sub  w12, w12, #48
    tst  w12, #0xfffffff0  // // out of 0-9? 
    bne  .Lparse_done
    mul  x0, x0, x10
    add  x0, x0, x12
    b    .Lparse_loop

.Lparse_done:
    cbz  x11, .Linput_ret
    neg  x0, x0
.Linput_ret:
    ldp  x29, x30, [sp], #16
    ret

    .global io__open
io__open:
    stp  x29, x30, [sp, #-16]!
    mov  x29, sp
    // // x0=path, x1=mode 
    mov  x2, x1
    // // Convert mode: 0→O_RDONLY(0), 1→O_WRONLY|O_CREAT|O_TRUNC(577), 2→O_WRONLY|O_CREAT|O_APPEND(1089) 
    cmp  x2, #1
    beq  .Lopen_write
    cmp  x2, #2
    beq  .Lopen_append
    mov  x2, #0           // // O_RDONLY 
    b    .Lopen_go
.Lopen_write:
    mov  x2, #577         // // O_WRONLY|O_CREAT|O_TRUNC 
    b    .Lopen_go
.Lopen_append:
    mov  x2, #1089        // // O_WRONLY|O_CREAT|O_APPEND 
.Lopen_go:
    mov  x1, x0           // // path 
    mov  x0, #-100        // // AT_FDCWD 
    mov  x3, #420       // // mode bits 
    mov  x8, #56          // // __NR_openat 
    svc  #0
    ldp  x29, x30, [sp], #16
    ret

    .global io__fopen
io__fopen:
    b    io__open

    .global io__close
io__close:
    stp  x29, x30, [sp, #-16]!
    mov  x29, sp
    mov  x8, #57          // // __NR_close 
    svc  #0
    ldp  x29, x30, [sp], #16
    ret

    .global io__fclose
io__fclose:
    b    io__close

    .global io__read
io__read:
    stp  x29, x30, [sp, #-16]!
    mov  x29, sp
    // // x0=fd, x1=buf, x2=len 
    mov  x8, #63          // // __NR_read 
    svc  #0
    ldp  x29, x30, [sp], #16
    ret

    .global io__write
io__write:
    stp  x29, x30, [sp, #-16]!
    mov  x29, sp
    // // x0=fd, x1=buf, x2=len 
    mov  x8, #64          // // __NR_write 
    svc  #0
    ldp  x29, x30, [sp], #16
    ret

// __vl_arr_append(arr_ptr_addr: x0, elem: x1)
// *x0 is the current data pointer (or 0 if empty).
// We store [len][cap][data...] before the returned pointer.
// Layout: [base] = len(8) cap(8) data(len*8)...
// User ptr points to data start = base+16.
// On first append: mmap initial block.
// On subsequent: check cap, mremap if needed.
    .global __vl_arr_append
__vl_arr_append:
    stp  x29, x30, [sp, #-32]!
    stp  x19, x20, [sp, #16]
    mov  x29, sp
    mov  x19, x0   // ptr to the variable holding the data ptr
    mov  x20, x1   // element to append

    ldr  x9, [x19] // current data ptr (points past header)
    cbz  x9, .Larrapp_init

    // data ptr exists - get len/cap from header
    sub  x10, x9, #16   // base = data_ptr - 16
    ldr  x11, [x10]     // len
    ldr  x12, [x10, #8] // cap
    cmp  x11, x12
    blt  .Larrapp_store  // still has room

    // double capacity via mremap
    lsl  x13, x12, #1   // new_cap = cap * 2
    add  x14, x13, #2   // +2 for len/cap slots
    lsl  x14, x14, #3   // * 8 bytes
    add  x15, x12, #2
    lsl  x15, x15, #3   // old_size in bytes
    mov  x0, x10        // old_ptr
    mov  x1, x15        // old_size
    mov  x2, x14        // new_size
    mov  x3, #1         // MREMAP_MAYMOVE
    mov  x4, #0
    mov  x8, #216       // __NR_mremap
    svc  #0
    mov  x10, x0        // new base
    str  x13, [x10, #8] // update cap
    add  x9, x10, #16   // update data ptr
    str  x9, [x19]      // write back
    ldr  x11, [x10]     // reload len

.Larrapp_store:
    // store elem at data[len]
    lsl  x13, x11, #3
    str  x20, [x9, x13]
    add  x11, x11, #1
    str  x11, [x10]     // update len
    b    .Larrapp_done

.Larrapp_init:
    // first append: mmap 16+8*4 = 48 bytes (cap=4)
    mov  x0, #0
    mov  x1, #48
    mov  x2, #3
    mov  x3, #0x22
    mov  x4, #-1
    mov  x5, #0
    mov  x8, #222
    svc  #0
    mov  x10, x0
    mov  x11, #0
    str  x11, [x10]     // len = 0
    mov  x12, #4
    str  x12, [x10, #8] // cap = 4
    add  x9, x10, #16   // data ptr
    str  x9, [x19]      // store data ptr in variable
    // now store the element
    str  x20, [x9]
    mov  x11, #1
    str  x11, [x10]     // len = 1

.Larrapp_done:
    ldp  x19, x20, [sp, #16]
    ldp  x29, x30, [sp], #32
    ret

    .section .bss
    .align 3
__vl_input_buf: .space 256
