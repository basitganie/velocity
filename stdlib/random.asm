; ============================================================
; Velocity Standard Library - Random Module (random.asm)
; Simple xorshift64* PRNG
;
; Functions:
;   random__seed(seed)      -> adad
;   random__randrange(max)  -> adad
;   random__random()        -> ashari (f64 in xmm0)
; ============================================================

section .bss
    random_seed resq 1

section .data
    rand_inv_2p53 dq 0x3CA0000000000000     ; 1 / 2^53

section .text

; ── random__seed ───────────────────────────────────────────
; rdi = seed
global random__seed
random__seed:
    mov [rel random_seed], rdi
    xor rax, rax
    ret

; ── random__randrange ─────────────────────────────────────
; rdi = max (exclusive)
; rax = 0..max-1
global random__randrange
random__randrange:
    push rbp
    mov  rbp, rsp
    cmp  rdi, 0
    jle  .zero
    call random__next
    xor  rdx, rdx
    div  rdi
    mov  rax, rdx
    pop  rbp
    ret
.zero:
    xor  rax, rax
    pop  rbp
    ret

; ── random__random ────────────────────────────────────────
; returns f64 in xmm0, [0,1)
global random__random
random__random:
    push rbp
    mov  rbp, rsp
    call random__next
    shr  rax, 11
    cvtsi2sd xmm0, rax
    mulsd xmm0, [rel rand_inv_2p53]
    pop  rbp
    ret

; ── random_next (internal) ────────────────────────────────
; rax = next 64-bit random
random__next:
    mov  rax, [rel random_seed]
    test rax, rax
    jne  .have_seed
    rdtsc
    shl  rdx, 32
    or   rax, rdx
    test rax, rax
    jne  .seeded
    mov  rax, 88172645463393265
.seeded:
    mov  [rel random_seed], rax
.have_seed:
    mov  rcx, rax
    shr  rax, 12
    xor  rcx, rax
    mov  rax, rcx
    shl  rcx, 25
    xor  rax, rcx
    mov  rcx, rax
    shr  rax, 27
    xor  rcx, rax
    mov  [rel random_seed], rcx
    mov  rax, rcx
    mov  r10, 2685821657736338717
    imul rax, r10
    ret
