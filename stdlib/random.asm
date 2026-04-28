; ============================================================
; Velocity StdLib - Random Module (random.asm)
; Pure xorshift64* PRNG -- no syscalls needed, fully portable
; Functions: random__seed  random__randrange  random__random
; ============================================================

%ifndef WIN64_TARGET
%ifidni __OUTPUT_FORMAT__,win64
  %define WIN64_TARGET 1
%endif
%endif
section .bss
    rng_state resq 1

section .data
    rng_inv53 dq 0x3CA0000000000000    ; 1.0 / 2^53

section .text

; random__seed(seed: adad)
global random__seed
random__seed:
%ifdef WIN64_TARGET
    mov  rdi, rcx
%endif
    mov  [rel rng_state], rdi
    xor  rax, rax
    ret

; internal: advance PRNG state -> rax
rng_next:
    mov  rax, [rel rng_state]
    test rax, rax
    jne  .go
    rdtsc
    shl  rdx, 32
    or   rax, rdx
    test rax, rax
    jne  .seed_ok
    mov  rax, 88172645463393265
.seed_ok:
    mov  [rel rng_state], rax
.go:
    mov  rcx, rax
    shr  rax, 12
    xor  rcx, rax
    mov  rax, rcx
    shl  rcx, 25
    xor  rax, rcx
    mov  rcx, rax
    shr  rax, 27
    xor  rcx, rax
    mov  [rel rng_state], rcx
    mov  rax, rcx
    mov  r10, 2685821657736338717
    imul rax, r10
    ret

; random__randrange(max: adad) -> adad
global random__randrange
random__randrange:
%ifdef WIN64_TARGET
    mov  rdi, rcx
%endif
    push rbp
    mov  rbp, rsp
    push rbx
    mov  rbx, rdi
    cmp  rbx, 0
    jle  .zero
    call rng_next
    xor  rdx, rdx
    div  rbx
    mov  rax, rdx
    pop  rbx
    pop  rbp
    ret
.zero:
    xor  rax, rax
    pop  rbx
    pop  rbp
    ret

; random__random() -> ashari (f64 in xmm0)
global random__random
random__random:
    push rbp
    mov  rbp, rsp
    call rng_next
    shr  rax, 11
    cvtsi2sd xmm0, rax
    mulsd xmm0, [rel rng_inv53]
    pop  rbp
    ret
