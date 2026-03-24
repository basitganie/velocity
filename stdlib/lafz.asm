; ============================================================
; Velocity Standard Library - Lafz (String) Module
; String utilities with a simple bump allocator.
;
; Functions:
;   lafz__len(s)                -> length
;   lafz__eq(a, b)              -> 1 if equal else 0
;   lafz__concat(a, b)          -> new string
;   lafz__slice(s, start, len)  -> new string
;   lafz__from_adad(n)          -> new string
;   lafz__from_ashari(f64)      -> new string
; ============================================================

%define LAFZ_HEAP_SIZE 65536

section .bss
    lafz_heap resb LAFZ_HEAP_SIZE
    lafz_hp   resq 1
    tmp_buf   resb 128
    num_buf   resb 32

section .data
    lafz_empty  db 0
    minus_char  db '-'
    dot_char    db '.'
    one_million dq 1000000.0
    ten_f64     dq 10.0
    one_f64     dq 1.0
    neg_one_f64 dq -1.0

section .text

; ── lafz__len ──────────────────────────────────────────────
global lafz__len
lafz__len:
    push rbp
    mov  rbp, rsp
    xor  rax, rax
    test rdi, rdi
    jz   .done
.loop:
    cmp  byte [rdi + rax], 0
    je   .done
    inc  rax
    jmp  .loop
.done:
    pop  rbp
    ret

; ── lafz__eq ───────────────────────────────────────────────
global lafz__eq
lafz__eq:
    push rbp
    mov  rbp, rsp

    cmp  rdi, 0
    jne  .check_b
    cmp  rsi, 0
    jne  .false
    mov  rax, 1
    jmp  .done
.check_b:
    cmp  rsi, 0
    je   .false
.cmp_loop:
    mov  al, [rdi]
    mov  dl, [rsi]
    cmp  al, dl
    jne  .false
    test al, al
    je   .true
    inc  rdi
    inc  rsi
    jmp  .cmp_loop
.true:
    mov  rax, 1
    jmp  .done
.false:
    xor  rax, rax
.done:
    pop  rbp
    ret

; ── lafz__concat ───────────────────────────────────────────
global lafz__concat
lafz__concat:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov  r12, rdi
    mov  r13, rsi

    mov  rdi, r12
    call lafz_strlen
    mov  r14, rax

    mov  rdi, r13
    call lafz_strlen
    mov  r15, rax

    mov  rdi, r14
    add  rdi, r15
    inc  rdi
    call lafz_alloc
    test rax, rax
    jz   .oom
    mov  rbx, rax

    mov  rcx, r14
    mov  rsi, r12
    mov  rdi, rbx
    rep  movsb

    mov  rcx, r15
    mov  rsi, r13
    rep  movsb
    mov  byte [rdi], 0

    mov  rax, rbx
    jmp  .done
.oom:
    lea  rax, [rel lafz_empty]
.done:
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    ret

; ── lafz__slice ────────────────────────────────────────────
global lafz__slice
lafz__slice:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov  r12, rdi  ; s
    mov  r13, rsi  ; start
    mov  r14, rdx  ; len

    test r12, r12
    jz   .empty

    test r13, r13
    jns  .start_ok
    xor  r13, r13
.start_ok:
    test r14, r14
    jg   .len_ok
    jmp  .empty
.len_ok:
    mov  rdi, r12
    call lafz_strlen
    mov  r15, rax  ; total len
    cmp  r13, r15
    jae  .empty

    mov  rax, r15
    sub  rax, r13  ; remaining
    cmp  r14, rax
    jbe  .use_len
    mov  r14, rax
.use_len:
    mov  rdi, r14
    inc  rdi
    call lafz_alloc
    test rax, rax
    jz   .empty
    mov  rbx, rax

    lea  rsi, [r12 + r13]
    mov  rdi, rbx
    mov  rcx, r14
    rep  movsb
    mov  byte [rdi], 0

    mov  rax, rbx
    jmp  .done
.empty:
    lea  rax, [rel lafz_empty]
.done:
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    ret

; ── lafz__from_adad ────────────────────────────────────────
global lafz__from_adad
lafz__from_adad:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13

    mov  r12, rdi
    lea  rbx, [rel num_buf]
    lea  r13, [rbx + 31]
    mov  byte [r13], 0

    cmp  r12, 0
    jne  .nz
    dec  r13
    mov  byte [r13], '0'
    mov  rbx, r13
    jmp  .digits_done
.nz:
    mov  rax, r12
    cmp  rax, 0
    jge  .pos
    neg  rax
.pos:
.digit_loop:
    xor  rdx, rdx
    mov  r8, 10
    div  r8
    add  dl, '0'
    dec  r13
    mov  [r13], dl
    test rax, rax
    jnz  .digit_loop
    mov  rbx, r13

    cmp  r12, 0
    jge  .digits_done
    dec  rbx
    mov  byte [rbx], '-'

.digits_done:
    mov  rdi, rbx
    call lafz_strlen
    mov  r12, rax

    mov  rdi, r12
    inc  rdi
    call lafz_alloc
    test rax, rax
    jz   .oom
    mov  rdx, rax

    mov  rcx, r12
    mov  rsi, rbx
    mov  rdi, rdx
    rep  movsb
    mov  byte [rdi], 0

    mov  rax, rdx
    jmp  .done
.oom:
    lea  rax, [rel lafz_empty]
.done:
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    ret

; ── lafz__from_ashari ──────────────────────────────────────
global lafz__from_ashari
lafz__from_ashari:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov  rax, rdi
    lea  r12, [rel tmp_buf]
    mov  r13, r12           ; write ptr

    test rax, rax
    jns  .fa_pos
    mov  byte [r13], '-'
    inc  r13
    mov  rdx, 0x7fffffffffffffff
    and  rax, rdx
.fa_pos:
    movq xmm0, rax

    ; integer part -> num_buf
    cvttsd2si rbx, xmm0
    lea  r14, [rel num_buf]
    lea  r15, [r14 + 31]
    mov  byte [r15], 0

    cmp  rbx, 0
    jne  .fa_int_nz
    dec  r15
    mov  byte [r15], '0'
    mov  r14, r15
    jmp  .fa_int_done
.fa_int_nz:
    mov  rax, rbx
.fa_int_loop:
    xor  rdx, rdx
    mov  r8, 10
    div  r8
    add  dl, '0'
    dec  r15
    mov  [r15], dl
    test rax, rax
    jnz  .fa_int_loop
    mov  r14, r15
.fa_int_done:
    mov  rdi, r14
    call lafz_strlen
    mov  rcx, rax
    mov  rsi, r14
    mov  rdi, r13
    rep  movsb
    mov  r13, rdi

    ; fractional part
    cvtsi2sd xmm1, rbx
    subsd xmm0, xmm1
    mulsd xmm0, [rel one_million]
    cvttsd2si rdx, xmm0      ; frac int

    mov  byte [r13], '.'
    inc  r13

    mov  rax, rdx
    mov  rcx, 6
    lea  rsi, [r13 + 5]
.fa_frac_loop:
    xor  rdx, rdx
    mov  r9, 10
    div  r9
    add  dl, '0'
    mov  [rsi], dl
    dec  rsi
    dec  rcx
    jnz  .fa_frac_loop
    add  r13, 6
    mov  byte [r13], 0

    mov  rdi, r12
    call lafz_strlen
    mov  rbx, rax

    mov  rdi, rbx
    inc  rdi
    call lafz_alloc
    test rax, rax
    jz   .fa_oom
    mov  rdx, rax

    mov  rcx, rbx
    mov  rsi, r12
    mov  rdi, rdx
    rep  movsb
    mov  byte [rdi], 0

    mov  rax, rdx
    jmp  .fa_done
.fa_oom:
    lea  rax, [rel lafz_empty]
.fa_done:
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    ret

; ── lafz__to_adad ─────────────────────────────────────────
; parse decimal integer from string
global lafz__to_adad
lafz__to_adad:
    push rbp
    mov  rbp, rsp
    push rbx

    mov  rsi, rdi
    xor  rax, rax
    test rsi, rsi
    jz   .ta_done

.ta_skip:
    mov  bl, [rsi]
    cmp  bl, ' '
    je   .ta_ws
    cmp  bl, 9
    jne  .ta_sign
.ta_ws:
    inc  rsi
    jmp  .ta_skip

.ta_sign:
    mov  rcx, 1
    cmp  bl, '-'
    jne  .ta_plus
    mov  rcx, -1
    inc  rsi
    jmp  .ta_digits
.ta_plus:
    cmp  bl, '+'
    jne  .ta_digits
    inc  rsi

.ta_digits:
    xor  rax, rax
.ta_loop:
    mov  bl, [rsi]
    cmp  bl, '0'
    jb   .ta_end
    cmp  bl, '9'
    ja   .ta_end
    imul rax, rax, 10
    movzx rdx, bl
    sub  rdx, '0'
    add  rax, rdx
    inc  rsi
    jmp  .ta_loop

.ta_end:
    cmp  rcx, 1
    je   .ta_done
    neg  rax
.ta_done:
    pop  rbx
    pop  rbp
    ret

; ── lafz__to_ashari ───────────────────────────────────────
; parse decimal float (supports optional exponent)
global lafz__to_ashari
lafz__to_ashari:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    push r14

    mov  rsi, rdi
    pxor xmm0, xmm0
    test rsi, rsi
    jz   .tf_done

.tf_skip:
    mov  bl, [rsi]
    cmp  bl, ' '
    je   .tf_ws
    cmp  bl, 9
    jne  .tf_sign
.tf_ws:
    inc  rsi
    jmp  .tf_skip

.tf_sign:
    xor  r12, r12
    cmp  bl, '-'
    jne  .tf_plus
    mov  r12, 1
    inc  rsi
    jmp  .tf_int
.tf_plus:
    cmp  bl, '+'
    jne  .tf_int
    inc  rsi

.tf_int:
    xor  rax, rax
.tf_int_loop:
    mov  bl, [rsi]
    cmp  bl, '0'
    jb   .tf_int_done
    cmp  bl, '9'
    ja   .tf_int_done
    imul rax, rax, 10
    movzx rdx, bl
    sub  rdx, '0'
    add  rax, rdx
    inc  rsi
    jmp  .tf_int_loop
.tf_int_done:
    cvtsi2sd xmm0, rax

    cmp  byte [rsi], '.'
    jne  .tf_frac_done
    inc  rsi
    xor  r13, r13          ; frac
    mov  r14, 1            ; scale
    xor  rcx, rcx
.tf_frac_loop:
    mov  bl, [rsi]
    cmp  bl, '0'
    jb   .tf_frac_done
    cmp  bl, '9'
    ja   .tf_frac_done
    cmp  rcx, 18
    jae  .tf_frac_skip
    imul r13, r13, 10
    movzx rdx, bl
    sub  rdx, '0'
    add  r13, rdx
    imul r14, r14, 10
    inc  rcx
.tf_frac_skip:
    inc  rsi
    jmp  .tf_frac_loop
.tf_frac_done:
    test r14, r14
    jz   .tf_exp
    cvtsi2sd xmm1, r13
    cvtsi2sd xmm2, r14
    divsd xmm1, xmm2
    addsd xmm0, xmm1

.tf_exp:
    mov  bl, [rsi]
    cmp  bl, 'e'
    je   .tf_exp_parse
    cmp  bl, 'E'
    jne  .tf_apply_sign
.tf_exp_parse:
    inc  rsi
    mov  r13, 1
    mov  bl, [rsi]
    cmp  bl, '-'
    jne  .tf_exp_plus
    mov  r13, -1
    inc  rsi
    jmp  .tf_exp_num
.tf_exp_plus:
    cmp  bl, '+'
    jne  .tf_exp_num
    inc  rsi
.tf_exp_num:
    xor  r14, r14
.tf_exp_loop:
    mov  bl, [rsi]
    cmp  bl, '0'
    jb   .tf_exp_done
    cmp  bl, '9'
    ja   .tf_exp_done
    imul r14, r14, 10
    movzx rdx, bl
    sub  rdx, '0'
    add  r14, rdx
    inc  rsi
    jmp  .tf_exp_loop
.tf_exp_done:
    movsd xmm1, [rel one_f64]
    cmp  r14, 0
    je   .tf_apply_exp
.tf_exp_calc:
    cmp  r13, 0
    jg   .tf_exp_mul
    divsd xmm1, [rel ten_f64]
    dec  r14
    jnz  .tf_exp_calc
    jmp  .tf_apply_exp
.tf_exp_mul:
    mulsd xmm1, [rel ten_f64]
    dec  r14
    jnz  .tf_exp_calc
.tf_apply_exp:
    mulsd xmm0, xmm1

.tf_apply_sign:
    test r12, r12
    jz   .tf_done
    mulsd xmm0, [rel neg_one_f64]

.tf_done:
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    ret

; ── internal: strlen ───────────────────────────────────────
lafz_strlen:
    xor  rax, rax
    test rdi, rdi
    jz   .ls_done
.ls_loop:
    cmp  byte [rdi + rax], 0
    je   .ls_done
    inc  rax
    jmp  .ls_loop
.ls_done:
    ret

; ── internal: bump allocator ───────────────────────────────
; rdi = size, returns rax = ptr or 0
lafz_alloc:
    push rbx
    mov  rax, [rel lafz_hp]
    test rax, rax
    jnz  .ha_have
    lea  rax, [rel lafz_heap]
    mov  [rel lafz_hp], rax
.ha_have:
    mov  rbx, rax
    add  rax, rdi
    lea  rcx, [rel lafz_heap]
    add  rcx, LAFZ_HEAP_SIZE
    cmp  rax, rcx
    ja   .ha_oom
    mov  [rel lafz_hp], rax
    mov  rax, rbx
    pop  rbx
    ret
.ha_oom:
    xor  rax, rax
    pop  rbx
    ret
