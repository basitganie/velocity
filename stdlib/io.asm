; ============================================================
; Velocity Standard Library - I/O Module (io.asm)
; Complete I/O via Linux syscalls
;
; Functions:
;   io__chhaap(fmt, ...)       - printf-like (supports %d %s %c %f %b %%)
;   io__chhaapLine()           - print newline
;   io__chhaapSpace()          - print space
;   io__chhaapText(str, len)   - print string (future)
;   io__input()                - read integer from stdin (deprecated)
;   io__stdin()                - read line as string (lafz)
; ============================================================

section .bss
    num_buf resb 32         ; buffer for int->string conversion
    input_buf resb 256      ; buffer for reading input
    char_buf resb 1         ; single-char buffer

section .data
    newline_char  db 10
    space_char    db 32
    dot_char      db '.'
    minus_char    db '-'
    zero_char     db '0', 10
    poz_str       db 'poz', 0
    apuz_str      db 'apuz', 0
    one_million   dq 1000000.0

section .text

; ── io__chhaapLine ─────────────────────────────────────────
; Print a newline character
global io__chhaapLine
io__chhaapLine:
    push rbp
    mov  rbp, rsp
    lea  rsi, [rel newline_char]
    mov  rax, 1
    mov  rdi, 1
    mov  rdx, 1
    syscall
    mov  rax, 0
    pop  rbp
    ret

; ── io__chhaapSpace ────────────────────────────────────────
; Print a space character
global io__chhaapSpace
io__chhaapSpace:
    push rbp
    mov  rbp, rsp
    lea  rsi, [rel space_char]
    mov  rax, 1
    mov  rdi, 1
    mov  rdx, 1
    syscall
    mov  rax, 0
    pop  rbp
    ret

; ── io__chhaap ─────────────────────────────────────────────
; printf-like formatter.
; Args:
;   rdi = fmt (null-terminated)
;   rsi, rdx, rcx, r8, r9 = up to 5 values
; Supports: %d %s %c %%
global io__chhaap
io__chhaap:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; spill args into a small array on stack (rsi..r9)
    sub  rsp, 48
    mov  [rsp + 0],  rsi
    mov  [rsp + 8],  rdx
    mov  [rsp + 16], rcx
    mov  [rsp + 24], r8
    mov  [rsp + 32], r9
    mov  r12, rsp          ; r12 = args base
    xor  r13, r13          ; r13 = arg index

    mov  rbx, rdi          ; rbx = fmt pointer

.loop:
    mov  al, [rbx]
    test al, al
    je   .done
    cmp  al, '%'
    jne  .emit_literal

    inc  rbx
    mov  al, [rbx]
    test al, al
    je   .done
    cmp  al, '%'
    je   .emit_percent
    cmp  al, 'd'
    je   .emit_int
    cmp  al, 's'
    je   .emit_str
    cmp  al, 'c'
    je   .emit_char_arg
    cmp  al, 'f'
    je   .emit_float
    cmp  al, 'b'
    je   .emit_bool

    ; unknown specifier: print '%' then the char
    mov  al, '%'
    call .write_char
    mov  al, [rbx]
    call .write_char
    jmp  .next

.emit_literal:
    call .write_char
    jmp  .next

.emit_percent:
    mov  al, '%'
    call .write_char
    jmp  .next

.emit_char_arg:
    cmp  r13, 5
    jae  .next
    mov  rdi, [r12 + r13*8]
    inc  r13
    mov  al, dil
    call .write_char
    jmp  .next

.emit_int:
    cmp  r13, 5
    jae  .next
    mov  rdi, [r12 + r13*8]
    inc  r13
    call .print_int
    jmp  .next

.emit_str:
    cmp  r13, 5
    jae  .next
    mov  rdi, [r12 + r13*8]
    inc  r13
    call .print_cstr
    jmp  .next

.emit_float:
    cmp  r13, 5
    jae  .next
    mov  rdi, [r12 + r13*8]
    inc  r13
    call .print_float
    jmp  .next

.emit_bool:
    cmp  r13, 5
    jae  .next
    mov  rdi, [r12 + r13*8]
    inc  r13
    call .print_bool
    jmp  .next

.next:
    inc  rbx
    jmp  .loop

.done:
    add  rsp, 48
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    mov  rax, 0
    pop  rbp
    ret

.write_char:
    mov  [char_buf], al
    mov  rax, 1
    mov  rdi, 1
    lea  rsi, [rel char_buf]
    mov  rdx, 1
    syscall
    ret

.print_cstr:
    test rdi, rdi
    jz   .print_cstr_done
    push rbx
    mov  rbx, rdi
    xor  rcx, rcx
.len_loop:
    mov  al, [rbx + rcx]
    test al, al
    je   .len_done
    inc  rcx
    jmp  .len_loop
.len_done:
    mov  rax, 1
    mov  rdi, 1
    mov  rsi, rbx
    mov  rdx, rcx
    syscall
    pop  rbx
.print_cstr_done:
    ret

.print_bool:
    cmp  rdi, 0
    jne  .pb_true
    lea  rdi, [rel apuz_str]
    jmp  .pb_emit
.pb_true:
    lea  rdi, [rel poz_str]
.pb_emit:
    call .print_cstr
    ret

.print_int:
    push rbx
    push r12
    push r13

    mov  r12, rdi

    ; handle zero
    test r12, r12
    jnz  .pi_not_zero
    lea  rsi, [rel zero_char]
    mov  rax, 1
    mov  rdi, 1
    mov  rdx, 1
    syscall
    jmp  .pi_done

.pi_not_zero:
    ; handle negative
    test r12, r12
    jge  .pi_positive
    lea  rsi, [rel minus_char]
    mov  rax, 1
    mov  rdi, 1
    mov  rdx, 1
    syscall
    neg  r12

.pi_positive:
    ; convert to decimal string (right-to-left)
    lea  rbx, [rel num_buf]
    mov  rcx, 31            ; start at end of buffer
    mov  rax, r12

.pi_digit_loop:
    xor  rdx, rdx
    mov  r8,  10
    div  r8
    add  rdx, '0'
    mov  [rbx + rcx], dl
    dec  rcx
    test rax, rax
    jnz  .pi_digit_loop

    ; write the string
    inc  rcx                ; point to first digit
    lea  rsi, [rbx + rcx]
    mov  rdx, 32
    sub  rdx, rcx
    mov  rax, 1
    mov  rdi, 1
    syscall

.pi_done:
    pop  r13
    pop  r12
    pop  rbx
    ret

.print_float:
    ; rdi holds raw bits of f64
    push rbx

    mov  rax, rdi
    test rax, rax
    jns  .pf_positive
    mov  al, '-'
    call .write_char
    mov  rdx, 0x7fffffffffffffff
    and  rax, rdx

.pf_positive:
    movq xmm0, rax

    ; integer part (rbx) and fractional part (r14)
    cvttsd2si rbx, xmm0
    cvtsi2sd xmm1, rbx
    subsd xmm0, xmm1
    mulsd xmm0, [rel one_million]
    cvttsd2si r14, xmm0

    mov  rdi, rbx
    call .print_int

    ; write '.'
    mov  al, '.'
    call .write_char

    ; print 6 digits with leading zeros
    mov  rax, r14
    lea  rbx, [rel num_buf]
    add  rbx, 6
    mov  r10, 6
.pf_loop:
    dec  rbx
    xor  rdx, rdx
    mov  r9, 10
    div  r9
    add  dl, '0'
    mov  [rbx], dl
    dec  r10
    jnz  .pf_loop

    mov  rax, 1
    mov  rdi, 1
    mov  rsi, rbx
    mov  rdx, 6
    syscall

    pop  rbx
    ret

; ── io__input ──────────────────────────────────────────────
; Read an integer from stdin
; Returns: integer in rax
global io__input
io__input:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12

    call io__stdin
    test rax, rax
    jle  .error             ; if read failed or 0 bytes
    mov  rbx, rax           ; rbx = input pointer

    ; parse the string
    xor  r12, r12           ; r12 = result accumulator
    xor  r8, r8             ; r8 = index
    xor  r9, r9             ; r9 = sign (0=positive, 1=negative)

    ; check for minus sign
    cmp  byte [rbx], '-'
    jne  .parse_digits
    mov  r9, 1
    inc  r8                 ; skip the '-'

.parse_digits:
    movzx rcx, byte [rbx + r8]
    
    ; check for newline or null
    cmp  rcx, 10
    je   .finish
    cmp  rcx, 0
    je   .finish
    
    ; check if digit
    cmp  rcx, '0'
    jl   .finish
    cmp  rcx, '9'
    jg   .finish
    
    ; convert ASCII to digit and accumulate
    sub  rcx, '0'
    imul r12, 10
    add  r12, rcx
    inc  r8
    jmp  .parse_digits

.finish:
    ; apply sign
    test r9, r9
    jz   .return
    neg  r12

.return:
    mov  rax, r12
    pop  r12
    pop  rbx
    pop  rbp
    ret

.error:
    xor  rax, rax           ; return 0 on error
    pop  r12
    pop  rbx
    pop  rbp
    ret

; ── io__stdin ──────────────────────────────────────────────
; Read a line from stdin into input_buf
; Returns: pointer to null-terminated string in rax (0 on EOF/error)
global io__stdin
io__stdin:
    push rbp
    mov  rbp, rsp
    push rbx

    lea  rsi, [rel input_buf]
    mov  rax, 0             ; sys_read
    mov  rdi, 0             ; stdin
    mov  rdx, 255
    syscall

    test rax, rax
    jle  .stdin_empty

    lea  rbx, [rel input_buf]
    mov  rcx, rax
    dec  rcx
    cmp  byte [rbx + rcx], 10
    jne  .stdin_no_nl
    mov  byte [rbx + rcx], 0
    jmp  .stdin_done
.stdin_no_nl:
    mov  byte [rbx + rax], 0
.stdin_done:
    mov  rax, rbx
    pop  rbx
    pop  rbp
    ret

.stdin_empty:
    xor  rax, rax
    pop  rbx
    pop  rbp
    ret
