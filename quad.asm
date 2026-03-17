; Velocity Compiler - Kashmiri Edition v2
; x86-64 Linux (NASM syntax)

section .text

; externs for native module: io
extern io__chhaapLine
extern io__chhaapSpace
extern io__chhaap
extern io__input
extern io__stdin

; ===== module: math =====
; --- math__mul_chohrus ---
global math__mul_chohrus
math__mul_chohrus:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1032
    mov [rbp - 8], rdi
    movsd xmm0, [rbp - 8]
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 2
    cvtsi2sd xmm0, rax
    movsd xmm1, [rsp]
    add rsp, 8
    divsd xmm1, xmm0
    movapd xmm0, xmm1
    movsd [rbp - 16], xmm0
    mov rax, [rbp - 24]
    mov [rbp - 24], rax
    mov rax, 20
    mov [rbp - 32], rax
    mov rax, 1
    mov [rbp - 40], rax
    mov rax, [rbp - 40]
    cmp rax, 0
    jle _VL4
_VL2:
    mov rax, [rbp - 24]
    mov rbx, [rbp - 32]
    cmp rax, rbx
    jg  _VL4
    movsd xmm0, [rbp - 16]
    sub rsp, 8
    movsd [rsp], xmm0
    movsd xmm0, [rbp - 16]
    movsd xmm1, [rsp]
    add rsp, 8
    mulsd xmm1, xmm0
    movapd xmm0, xmm1
    sub rsp, 8
    movsd [rsp], xmm0
    movsd xmm0, [rbp - 8]
    movsd xmm1, [rsp]
    add rsp, 8
    addsd xmm1, xmm0
    movapd xmm0, xmm1
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 2
    push rax
    movsd xmm0, [rbp - 16]
    pop rbx
    cvtsi2sd xmm1, rbx
    mulsd xmm1, xmm0
    movapd xmm0, xmm1
    movsd xmm1, [rsp]
    add rsp, 8
    divsd xmm1, xmm0
    movapd xmm0, xmm1
    movsd [rbp - 16], xmm0
_VL3:
    mov rax, [rbp - 24]
    mov rbx, [rbp - 40]
    add rax, rbx
    mov [rbp - 24], rax
    jmp _VL2
_VL4:
    movsd xmm0, [rbp - 16]
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; --- math__mutlaq ---
global math__mutlaq
math__mutlaq:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1032
    mov [rbp - 8], rdi
    mov rax, [rbp - 8]
    push rax
    mov rax, 0
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setl  al
    cmp rax, 0
    je  _VL5
    mov rax, 0
    push rax
    mov rax, [rbp - 8]
    mov rbx, rax
    pop rax
    sub rax, rbx
    mov rsp, rbp
    pop rbp
    ret
    jmp _VL6
_VL5:
_VL6:
    mov rax, [rbp - 8]
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; --- math__azeem ---
global math__azeem
math__azeem:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1040
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov rax, [rbp - 8]
    push rax
    mov rax, [rbp - 16]
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setg  al
    cmp rax, 0
    je  _VL7
    mov rax, [rbp - 8]
    mov rsp, rbp
    pop rbp
    ret
    jmp _VL8
_VL7:
_VL8:
    mov rax, [rbp - 16]
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; --- math__asgar ---
global math__asgar
math__asgar:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1040
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov rax, [rbp - 8]
    push rax
    mov rax, [rbp - 16]
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setl  al
    cmp rax, 0
    je  _VL9
    mov rax, [rbp - 8]
    mov rsp, rbp
    pop rbp
    ret
    jmp _VL10
_VL9:
_VL10:
    mov rax, [rbp - 16]
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; --- math__shakti ---
global math__shakti
math__shakti:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1040
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov rax, [rbp - 16]
    push rax
    mov rax, 0
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    sete  al
    cmp rax, 0
    je  _VL11
    mov rax, 1
    mov rsp, rbp
    pop rbp
    ret
    jmp _VL12
_VL11:
_VL12:
    mov rax, 1
    mov [rbp - 24], rax
    mov rax, 0
    mov [rbp - 32], rax
_VL13:
    mov rax, [rbp - 32]
    push rax
    mov rax, [rbp - 16]
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setl  al
    cmp rax, 0
    je  _VL14
    mov rax, [rbp - 24]
    push rax
    mov rax, [rbp - 8]
    mov rbx, rax
    pop rax
    imul rax, rbx
    mov [rbp - 24], rax
    mov rax, [rbp - 32]
    push rax
    mov rax, 1
    mov rbx, rax
    pop rax
    add rax, rbx
    mov [rbp - 32], rax
    jmp _VL13
_VL14:
    mov rax, [rbp - 24]
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; --- math__jar ---
global math__jar
math__jar:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1032
    mov [rbp - 8], rdi
    mov rax, [rbp - 8]
    push rax
    mov rax, 2
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setl  al
    cmp rax, 0
    je  _VL15
    mov rax, [rbp - 8]
    mov rsp, rbp
    pop rbp
    ret
    jmp _VL16
_VL15:
_VL16:
    mov rax, [rbp - 8]
    mov [rbp - 16], rax
    mov rax, 1
    mov [rbp - 24], rax
_VL17:
    mov rax, [rbp - 16]
    push rax
    mov rax, [rbp - 24]
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setg  al
    cmp rax, 0
    je  _VL18
    mov rax, [rbp - 16]
    push rax
    mov rax, [rbp - 24]
    mov rbx, rax
    pop rax
    add rax, rbx
    push rax
    mov rax, 2
    mov rbx, rax
    pop rax
    cqo
    idiv rbx
    mov [rbp - 16], rax
    mov rax, [rbp - 8]
    push rax
    mov rax, [rbp - 16]
    mov rbx, rax
    pop rax
    cqo
    idiv rbx
    mov [rbp - 24], rax
    jmp _VL17
_VL18:
    mov rax, [rbp - 16]
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; --- math__zarb_tartib ---
global math__zarb_tartib
math__zarb_tartib:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1032
    mov [rbp - 8], rdi
    mov rax, [rbp - 8]
    push rax
    mov rax, 1
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setle al
    cmp rax, 0
    je  _VL19
    mov rax, 1
    mov rsp, rbp
    pop rbp
    ret
    jmp _VL20
_VL19:
_VL20:
    mov rax, [rbp - 8]
    push rax
    mov rax, [rbp - 8]
    push rax
    mov rax, 1
    mov rbx, rax
    pop rax
    sub rax, rbx
    push rax
    pop rdi
    call math__zarb_tartib
    mov rbx, rax
    pop rax
    imul rax, rbx
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; --- math__mushtarak_qasim ---
global math__mushtarak_qasim
math__mushtarak_qasim:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1040
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
_VL21:
    mov rax, [rbp - 16]
    push rax
    mov rax, 0
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setne al
    cmp rax, 0
    je  _VL22
    mov rax, [rbp - 16]
    mov [rbp - 24], rax
    mov rax, [rbp - 8]
    push rax
    mov rax, [rbp - 16]
    mov rbx, rax
    pop rax
    cqo
    idiv rbx
    mov rax, rdx
    mov [rbp - 16], rax
    mov rax, [rbp - 24]
    mov [rbp - 8], rax
    jmp _VL21
_VL22:
    mov rax, [rbp - 8]
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; --- math__mushtarak_mutaaddid ---
global math__mushtarak_mutaaddid
math__mushtarak_mutaaddid:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1040
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov rax, [rbp - 8]
    push rax
    mov rax, [rbp - 16]
    mov rbx, rax
    pop rax
    imul rax, rbx
    push rax
    mov rax, [rbp - 16]
    push rax
    mov rax, [rbp - 8]
    push rax
    pop rdi
    pop rsi
    call math__mushtarak_qasim
    mov rbx, rax
    pop rax
    cqo
    idiv rbx
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; --- math__awwal_chu ---
global math__awwal_chu
math__awwal_chu:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1032
    mov [rbp - 8], rdi
    mov rax, [rbp - 8]
    push rax
    mov rax, 1
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setle al
    cmp rax, 0
    je  _VL23
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    jmp _VL24
_VL23:
_VL24:
    mov rax, [rbp - 8]
    push rax
    mov rax, 3
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setle al
    cmp rax, 0
    je  _VL25
    mov rax, 1
    mov rsp, rbp
    pop rbp
    ret
    jmp _VL26
_VL25:
_VL26:
    mov rax, [rbp - 8]
    push rax
    mov rax, 2
    mov rbx, rax
    pop rax
    cqo
    idiv rbx
    mov rax, rdx
    push rax
    mov rax, 0
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    sete  al
    cmp rax, 0
    je  _VL27
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    jmp _VL28
_VL27:
_VL28:
    mov rax, 3
    mov [rbp - 16], rax
_VL29:
    mov rax, [rbp - 16]
    push rax
    mov rax, [rbp - 16]
    mov rbx, rax
    pop rax
    imul rax, rbx
    push rax
    mov rax, [rbp - 8]
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setle al
    cmp rax, 0
    je  _VL30
    mov rax, [rbp - 8]
    push rax
    mov rax, [rbp - 16]
    mov rbx, rax
    pop rax
    cqo
    idiv rbx
    mov rax, rdx
    push rax
    mov rax, 0
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    sete  al
    cmp rax, 0
    je  _VL31
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    jmp _VL32
_VL31:
_VL32:
    mov rax, [rbp - 16]
    push rax
    mov rax, 2
    mov rbx, rax
    pop rax
    add rax, rbx
    mov [rbp - 16], rax
    jmp _VL29
_VL30:
    mov rax, 1
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; --- math__fibonacci ---
global math__fibonacci
math__fibonacci:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1032
    mov [rbp - 8], rdi
    mov rax, [rbp - 8]
    push rax
    mov rax, 1
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setle al
    cmp rax, 0
    je  _VL33
    mov rax, [rbp - 8]
    mov rsp, rbp
    pop rbp
    ret
    jmp _VL34
_VL33:
_VL34:
    mov rax, 0
    mov [rbp - 16], rax
    mov rax, 1
    mov [rbp - 24], rax
    mov rax, 2
    mov [rbp - 32], rax
_VL35:
    mov rax, [rbp - 32]
    push rax
    mov rax, [rbp - 8]
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setle al
    cmp rax, 0
    je  _VL36
    mov rax, [rbp - 16]
    push rax
    mov rax, [rbp - 24]
    mov rbx, rax
    pop rax
    add rax, rbx
    mov [rbp - 40], rax
    mov rax, [rbp - 24]
    mov [rbp - 16], rax
    mov rax, [rbp - 40]
    mov [rbp - 24], rax
    mov rax, [rbp - 32]
    push rax
    mov rax, 1
    mov rbx, rax
    pop rax
    add rax, rbx
    mov [rbp - 32], rax
    jmp _VL35
_VL36:
    mov rax, [rbp - 24]
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; --- math__jama_tak ---
global math__jama_tak
math__jama_tak:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1032
    mov [rbp - 8], rdi
    mov rax, [rbp - 8]
    push rax
    mov rax, [rbp - 8]
    push rax
    mov rax, 1
    mov rbx, rax
    pop rax
    add rax, rbx
    mov rbx, rax
    pop rax
    imul rax, rbx
    push rax
    mov rax, 2
    mov rbx, rax
    pop rax
    cqo
    idiv rbx
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; --- math__joft_chu ---
global math__joft_chu
math__joft_chu:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1032
    mov [rbp - 8], rdi
    mov rax, [rbp - 8]
    push rax
    mov rax, 2
    mov rbx, rax
    pop rax
    cqo
    idiv rbx
    mov rax, rdx
    push rax
    mov rax, 0
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    sete  al
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; --- math__taaq_chu ---
global math__taaq_chu
math__taaq_chu:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1032
    mov [rbp - 8], rdi
    mov rax, [rbp - 8]
    push rax
    mov rax, 2
    mov rbx, rax
    pop rax
    cqo
    idiv rbx
    mov rax, rdx
    push rax
    mov rax, 0
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setne al
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; ----- solve_quad -----
global solve_quad
solve_quad:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1048
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    movsd xmm0, [rbp - 16]
    sub rsp, 8
    movsd [rsp], xmm0
    movsd xmm0, [rbp - 16]
    movsd xmm1, [rsp]
    add rsp, 8
    mulsd xmm1, xmm0
    movapd xmm0, xmm1
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 4
    push rax
    movsd xmm0, [rbp - 8]
    pop rbx
    cvtsi2sd xmm1, rbx
    mulsd xmm1, xmm0
    movapd xmm0, xmm1
    sub rsp, 8
    movsd [rsp], xmm0
    movsd xmm0, [rbp - 24]
    movsd xmm1, [rsp]
    add rsp, 8
    mulsd xmm1, xmm0
    movapd xmm0, xmm1
    movsd xmm1, [rsp]
    add rsp, 8
    subsd xmm1, xmm0
    movapd xmm0, xmm1
    movsd [rbp - 32], xmm0
    mov rdi, 2
    call __vl_arr_alloc
    mov [rbp - 40], rax
    mov rax, 0
    cvtsi2sd xmm0, rax
    mov rbx, [rbp - 40]
    movsd [rbx + 0], xmm0
    mov rax, 0
    cvtsi2sd xmm0, rax
    mov rbx, [rbp - 40]
    movsd [rbx + 8], xmm0
    movsd xmm0, [rbp - 32]
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 0
    cvtsi2sd xmm0, rax
    movsd xmm1, [rsp]
    add rsp, 8
    ucomisd xmm1, xmm0
    mov rax, 0
    sete  al
    cmp rax, 0
    je  _VL37
    mov rax, 0
    mov rcx, rax
    mov rax, 0
    push rax
    movsd xmm0, [rbp - 16]
    pop rbx
    cvtsi2sd xmm1, rbx
    subsd xmm1, xmm0
    movapd xmm0, xmm1
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 2
    push rax
    movsd xmm0, [rbp - 8]
    pop rbx
    cvtsi2sd xmm1, rbx
    mulsd xmm1, xmm0
    movapd xmm0, xmm1
    movsd xmm1, [rsp]
    add rsp, 8
    divsd xmm1, xmm0
    movapd xmm0, xmm1
    mov rbx, [rbp - 40]
    movsd [rbx + rcx*8], xmm0
    mov rax, [rbp - 40]
    mov rsp, rbp
    pop rbp
    ret
    jmp _VL38
_VL37:
_VL38:
    movsd xmm0, [rbp - 32]
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 0
    cvtsi2sd xmm0, rax
    movsd xmm1, [rsp]
    add rsp, 8
    ucomisd xmm1, xmm0
    mov rax, 0
    setae al
    cmp rax, 0
    je  _VL39
    mov rax, 0
    mov rcx, rax
    mov rax, 0
    push rax
    movsd xmm0, [rbp - 16]
    pop rbx
    cvtsi2sd xmm1, rbx
    subsd xmm1, xmm0
    movapd xmm0, xmm1
    sub rsp, 8
    movsd [rsp], xmm0
    movsd xmm0, [rbp - 32]
    movq rax, xmm0
    push rax
    pop rdi
    call math__mul_chohrus
    movsd xmm1, [rsp]
    add rsp, 8
    addsd xmm1, xmm0
    movapd xmm0, xmm1
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 2
    push rax
    movsd xmm0, [rbp - 8]
    pop rbx
    cvtsi2sd xmm1, rbx
    mulsd xmm1, xmm0
    movapd xmm0, xmm1
    movsd xmm1, [rsp]
    add rsp, 8
    divsd xmm1, xmm0
    movapd xmm0, xmm1
    mov rbx, [rbp - 40]
    movsd [rbx + rcx*8], xmm0
    mov rax, 1
    mov rcx, rax
    mov rax, 0
    push rax
    movsd xmm0, [rbp - 16]
    pop rbx
    cvtsi2sd xmm1, rbx
    subsd xmm1, xmm0
    movapd xmm0, xmm1
    sub rsp, 8
    movsd [rsp], xmm0
    movsd xmm0, [rbp - 32]
    movq rax, xmm0
    push rax
    pop rdi
    call math__mul_chohrus
    movsd xmm1, [rsp]
    add rsp, 8
    subsd xmm1, xmm0
    movapd xmm0, xmm1
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 2
    push rax
    movsd xmm0, [rbp - 8]
    pop rbx
    cvtsi2sd xmm1, rbx
    mulsd xmm1, xmm0
    movapd xmm0, xmm1
    movsd xmm1, [rsp]
    add rsp, 8
    divsd xmm1, xmm0
    movapd xmm0, xmm1
    mov rbx, [rbp - 40]
    movsd [rbx + rcx*8], xmm0
    mov rax, [rbp - 40]
    mov rsp, rbp
    pop rbp
    ret
    jmp _VL40
_VL39:
_VL40:
    lea rax, [rel _VL_str_0]
    push rax
    pop rdi
    call io__chhaap
    mov rax, [rbp - 40]
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; ----- main -----
global main
main:
    push rbp
    mov  rbp, rsp
    sub  rsp, 1024
    call io__input
    mov [rbp - 8], rax
    call io__input
    mov [rbp - 16], rax
    call io__input
    mov [rbp - 24], rax
    mov rdi, 0
    call __vl_arr_alloc
    mov [rbp - 32], rax
    mov rax, [rbp - 24]
    cvtsi2sd xmm0, rax
    movq rax, xmm0
    push rax
    mov rax, [rbp - 16]
    cvtsi2sd xmm0, rax
    movq rax, xmm0
    push rax
    mov rax, [rbp - 8]
    cvtsi2sd xmm0, rax
    movq rax, xmm0
    push rax
    pop rdi
    pop rsi
    pop rdx
    call solve_quad
