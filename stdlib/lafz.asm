; ============================================================
; Velocity StdLib - Lafz (String) Module (lafz.asm)
; Pure x86-64 -- no syscalls, fully portable Win64+Linux
;
; Uses a simple bump allocator (no free).
; Functions:
;   lafz__len      lafz__eq      lafz__concat  lafz__slice
;   lafz__from_adad  lafz__from_ashari
;   lafz__to_adad    lafz__to_ashari
; ============================================================

%ifndef WIN64_TARGET
%ifidni __OUTPUT_FORMAT__,win64
  %define WIN64_TARGET 1
%endif
%endif
%define LAFZ_HEAP_SIZE 131072

section .bss
    lafz_heap resb LAFZ_HEAP_SIZE
    lafz_hp   resq 1
    lf_num_buf resb 64

section .data
    lf_minus  db '-'
    lf_dot    db '.'
    lf_millf  dq 1000000.0
    lf_ten    dq 10.0

section .text

; -- heap init ----------------------------------------------------
lafz_init:
    mov  rax, [rel lafz_hp]
    test rax, rax
    jne  .ok
    lea  rax, [rel lafz_heap]
    mov  [rel lafz_hp], rax
.ok:
    ret

; lafz_alloc(n) -> ptr
lafz_alloc:
    ; rdi = bytes
    call lafz_init
    mov  rax, [rel lafz_hp]
    add  rdi, 1                  ; +1 for null terminator
    add  [rel lafz_hp], rdi
    ret

; -- lafz__len(s) -> adad ------------------------------------------
global lafz__len
lafz__len:
%ifdef WIN64_TARGET
    mov  rdi, rcx
%endif
    test rdi, rdi
    jz   .zero
    xor  rax, rax
.loop:
    cmp  byte [rdi + rax], 0
    je   .done
    inc  rax
    jmp  .loop
.zero:
    xor  rax, rax
.done:
    ret

; -- lafz__eq(a, b) -> adad ----------------------------------------
global lafz__eq
lafz__eq:
%ifdef WIN64_TARGET
    mov  rdi, rcx
    mov  rsi, rdx
%endif
    push rbx
.cmp:
    mov  al, [rdi]
    mov  bl, [rsi]
    cmp  al, bl
    jne  .neq
    test al, al
    jz   .eq
    inc  rdi
    inc  rsi
    jmp  .cmp
.eq:
    mov  rax, 1
    pop  rbx
    ret
.neq:
    xor  rax, rax
    pop  rbx
    ret

; -- lafz__concat(a, b) -> lafz ------------------------------------
global lafz__concat
lafz__concat:
%ifdef WIN64_TARGET
    mov  rdi, rcx
    mov  rsi, rdx
%endif
    push rbx
    push r12
    push r13
    mov  r12, rdi
    mov  r13, rsi

    ; len(a)
    mov  rdi, r12
    call lafz__len
    mov  rbx, rax

    ; len(b)
    mov  rdi, r13
    call lafz__len
    add  rbx, rax         ; total length

    ; alloc
    mov  rdi, rbx
    call lafz_alloc
    push rax              ; save dest ptr

    ; copy a
    mov  rdi, rax
    mov  rsi, r12
.cpa:
    mov  al, [rsi]
    test al, al
    jz   .cpa_done
    mov  [rdi], al
    inc  rdi
    inc  rsi
    jmp  .cpa
.cpa_done:
    ; copy b
    mov  rsi, r13
.cpb:
    mov  al, [rsi]
    mov  [rdi], al
    test al, al
    jz   .cpb_done
    inc  rdi
    inc  rsi
    jmp  .cpb
.cpb_done:
    pop  rax
    pop  r13
    pop  r12
    pop  rbx
    ret

; -- lafz__slice(s, start, len) -> lafz ----------------------------
global lafz__slice
lafz__slice:
%ifdef WIN64_TARGET
    mov  rdi, rcx
    mov  rsi, rdx
    mov  rdx, r8
%endif
    push rbx
    push r12
    push r13
    mov  r12, rdi           ; s
    mov  r13, rsi           ; start
    mov  rbx, rdx           ; len

    ; alloc
    mov  rdi, rbx
    call lafz_alloc
    push rax

    ; copy
    mov  rdi, rax
    mov  rsi, r12
    add  rsi, r13           ; s + start
    mov  rcx, rbx
.sl_loop:
    test rcx, rcx
    jz   .sl_done
    mov  al, [rsi]
    test al, al
    jz   .sl_done
    mov  [rdi], al
    inc  rdi
    inc  rsi
    dec  rcx
    jmp  .sl_loop
.sl_done:
    mov  byte [rdi], 0
    pop  rax
    pop  r13
    pop  r12
    pop  rbx
    ret

; -- lafz__from_adad(n: adad) -> lafz ------------------------------
global lafz__from_adad
lafz__from_adad:
%ifdef WIN64_TARGET
    mov  rdi, rcx
%endif
    push rbx
    push r12
    mov  r12, rdi           ; save n

    ; convert to string in lf_num_buf
    lea  rbx, [rel lf_num_buf]
    mov  byte [rbx+31], 0
    mov  rdi, 30
    mov  rsi, r12
    test rsi, rsi
    jns  .pos
    neg  rsi
.pos:
    test rsi, rsi
    jnz  .conv
    mov  byte [rbx+rdi], '0'
    dec  rdi
    jmp  .conv_done
.conv:
    xor  rdx, rdx
    mov  rax, rsi
    mov  rcx, 10
    div  rcx
    add  dl, '0'
    mov  [rbx + rdi], dl
    dec  rdi
    mov  rsi, rax
    test rsi, rsi
    jnz  .conv
.conv_done:
    test r12, r12
    jns  .no_minus
    mov  byte [rbx + rdi], '-'
    dec  rdi
.no_minus:
    inc  rdi
    ; rdi = start index
    lea  rsi, [rbx + rdi]
    ; length
    mov  rcx, 31
    sub  rcx, rdi
    ; alloc
    push rsi
    push rcx
    mov  rdi, rcx
    call lafz_alloc
    pop  rcx
    pop  rsi
    push rax
    ; copy
    mov  rdi, rax
    rep  movsb
    mov  byte [rdi], 0
    pop  rax
    pop  r12
    pop  rbx
    ret

; -- lafz__from_ashari(f: ashari) -> lafz --------------------------
global lafz__from_ashari
lafz__from_ashari:
    ; xmm0 = f64
    push rbp
    mov  rbp, rsp
    push rbx
    push r12

    ; integer part
    cvttsd2si r12, xmm0
    movq rbx, xmm0

    push r12
    mov  rdi, r12
    call lafz__from_adad
    pop  r12
    push rax           ; save int part string

    ; decimal: multiply frac by 1e6
    movq xmm0, rbx
    cvttsd2si rax, xmm0
    cvtsi2sd  xmm1, rax
    subsd     xmm0, xmm1     ; frac part
    movsd     xmm1, [rel lf_millf]
    mulsd     xmm0, xmm1
    cvttsd2si rax, xmm0
    test rax, rax
    jns  .fpos
    neg  rax
.fpos:
    mov  rdi, rax
    push rbx
    call lafz__from_adad
    pop  rbx
    ; rax = decimal string

    ; concat int_str + "." + dec_str
    ; first concat int + "."
    pop  r12           ; r12 = int_str
    push rax           ; save dec_str

    ; build "." string
    mov  rdi, 1
    call lafz_alloc
    mov  byte [rax],   '.'
    mov  byte [rax+1], 0
    push rax           ; dot_str

    mov  rdi, r12
%ifdef WIN64_TARGET
    mov  rcx, rdi
    mov  rdx, rax
%else
    mov  rsi, rax
%endif
    ; concat(int_str, dot_str)
    pop  rsi
    push rdi
%ifdef WIN64_TARGET
    mov  rcx, rdi
    mov  rdx, rsi
%else
    mov  rdi, [rsp]
    ; already set
%endif
    mov  rdi, r12
    call lafz__concat
    push rax           ; int_dot_str

    ; concat(int_dot_str, dec_str)
    pop  rdi
    pop  rsi           ; dec_str
    call lafz__concat

    pop  r12
    pop  rbx
    pop  rbp
    ret

; -- lafz__to_adad(s: lafz) -> adad --------------------------------
global lafz__to_adad
lafz__to_adad:
%ifdef WIN64_TARGET
    mov  rdi, rcx
%endif
    push rbx
    xor  rax, rax
    xor  rbx, rbx
    movzx rcx, byte [rdi]
    cmp  cl, '-'
    je   .neg
    jmp  .parse
.neg:
    mov  rbx, 1
    inc  rdi
.parse:
    movzx rcx, byte [rdi]
    test cl, cl
    jz   .done
    cmp  cl, '.'
    je   .done
    sub  cl, '0'
    imul rax, 10
    add  rax, rcx
    inc  rdi
    jmp  .parse
.done:
    test rbx, rbx
    jz   .ret
    neg  rax
.ret:
    pop  rbx
    ret

; -- lafz__to_ashari(s: lafz) -> ashari (xmm0) --------------------
global lafz__to_ashari
lafz__to_ashari:
%ifdef WIN64_TARGET
    mov  rdi, rcx
%endif
    push rbx
    push r12
    mov  r12, rdi

    ; integer part
    call lafz__to_adad
    cvtsi2sd xmm0, rax

    ; find '.'
    mov  rdi, r12
.find_dot:
    movzx rcx, byte [rdi]
    test cl, cl
    jz   .no_frac
    cmp  cl, '.'
    je   .has_frac
    inc  rdi
    jmp  .find_dot
.no_frac:
    pop  r12
    pop  rbx
    ret
.has_frac:
    inc  rdi            ; skip '.'
    ; parse fractional digits
    xorpd xmm1, xmm1
    movsd xmm2, [rel lf_ten]
    movsd xmm3, [rel lf_ten]     ; divisor
.frac_loop:
    movzx rcx, byte [rdi]
    test  cl, cl
    jz    .frac_done
    sub   cl, '0'
    cvtsi2sd xmm4, rcx
    divsd    xmm4, xmm3
    addsd    xmm1, xmm4
    mulsd    xmm3, xmm2
    inc  rdi
    jmp  .frac_loop
.frac_done:
    ; check sign of original
    movzx rcx, byte [r12]
    cmp  cl, '-'
    jne  .add_frac
    subsd xmm0, xmm1
    jmp  .done_frac
.add_frac:
    addsd xmm0, xmm1
.done_frac:
    pop  r12
    pop  rbx
    ret
