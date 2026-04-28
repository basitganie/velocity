; Velocity Compiler v2 - Kashmiri Edition
; Target: Windows PE64 (MS x64 ABI)

section .text

; externs for native module: io
extern io__chhaapLine
extern io__chhaapSpace
extern io__chhaap
extern io__stdin
extern io__input
extern io__open
extern io__close
extern io__fopen
extern io__fclose

; externs for native module: lafz
extern lafz__len
extern lafz__eq
extern lafz__concat
extern lafz__slice
extern lafz__from_adad
extern lafz__from_ashari
extern lafz__to_adad
extern lafz__to_ashari

; ----- demo_types -----
global demo_types
demo_types:
    push rbp
    mov  rbp, rsp
    sub  rsp, 8192
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    mov rax, 42
    mov [rbp - 8], rax
    mov rax, 87677
    mov [rbp - 16], rax
    mov rax, 200
    mov [rbp - 24], rax
    mov rax, [rbp - 8]
    push rax
    lea rax, [rel _VL_str_0]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, [rbp - 16]
    push rax
    lea rax, [rel _VL_str_1]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, [rbp - 16]
    push rax
    mov rax, 255
    mov rbx, rax
    pop rax
    add rax, rbx
    mov [rbp - 16], rax
    mov rax, [rbp - 16]
    push rax
    lea rax, [rel _VL_str_2]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    mov rax, [rbp - 24]
    push rax
    lea rax, [rel _VL_str_3]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ----- demo_literals -----
global demo_literals
demo_literals:
    push rbp
    mov  rbp, rsp
    sub  rsp, 8192
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    mov rax, 255
    mov [rbp - 8], rax
    mov rax, 202
    mov [rbp - 16], rax
    mov rax, 511
    mov [rbp - 24], rax
    mov rax, [rbp - 8]
    push rax
    lea rax, [rel _VL_str_4]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, [rbp - 16]
    push rax
    lea rax, [rel _VL_str_5]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, [rbp - 24]
    push rax
    lea rax, [rel _VL_str_6]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ----- demo_bitwise -----
global demo_bitwise
demo_bitwise:
    push rbp
    mov  rbp, rsp
    sub  rsp, 8192
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    mov rax, 10
    mov [rbp - 8], rax
    mov rax, 12
    mov [rbp - 16], rax
    mov rax, [rbp - 8]
    push rax
    mov rax, [rbp - 16]
    mov rbx, rax
    pop rax
    and rax, rbx
    mov [rbp - 24], rax
    mov rax, [rbp - 8]
    push rax
    mov rax, [rbp - 16]
    mov rbx, rax
    pop rax
    or  rax, rbx
    mov [rbp - 32], rax
    mov rax, [rbp - 8]
    push rax
    mov rax, [rbp - 16]
    mov rbx, rax
    pop rax
    xor rax, rbx
    mov [rbp - 40], rax
    mov rax, [rbp - 8]
    not rax
    mov [rbp - 48], rax
    mov rax, [rbp - 8]
    push rax
    mov rax, 2
    mov rbx, rax
    pop rax
    mov rcx, rbx
    shl rax, cl
    mov [rbp - 56], rax
    mov rax, [rbp - 8]
    push rax
    mov rax, 1
    mov rbx, rax
    pop rax
    mov rcx, rbx
    sar rax, cl
    mov [rbp - 64], rax
    mov rax, [rbp - 24]
    push rax
    lea rax, [rel _VL_str_7]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, [rbp - 32]
    push rax
    lea rax, [rel _VL_str_8]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, [rbp - 40]
    push rax
    lea rax, [rel _VL_str_9]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, [rbp - 56]
    push rax
    lea rax, [rel _VL_str_10]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, [rbp - 64]
    push rax
    lea rax, [rel _VL_str_11]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ----- demo_logical -----
global demo_logical
demo_logical:
    push rbp
    mov  rbp, rsp
    sub  rsp, 8192
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    mov rax, 1
    mov [rbp - 8], rax
    mov rax, 0
    mov [rbp - 16], rax
    mov rax, [rbp - 8]
    cmp rax, 0
    je  _VL0
    mov rax, [rbp - 16]
    cmp rax, 0
    je  _VL0
    mov rax, 1
    jmp _VL1
_VL0:
    xor rax, rax
_VL1:
    mov [rbp - 24], rax
    mov rax, [rbp - 8]
    cmp rax, 0
    jne _VL2
    mov rax, [rbp - 16]
    cmp rax, 0
    jne _VL2
    xor rax, rax
    jmp _VL3
_VL2:
    mov rax, 1
_VL3:
    mov [rbp - 32], rax
    mov rax, [rbp - 8]
    cmp rax, 0
    sete al
    movzx rax, al
    mov [rbp - 40], rax
    mov rax, [rbp - 24]
    push rax
    lea rax, [rel _VL_str_12]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, [rbp - 32]
    push rax
    lea rax, [rel _VL_str_13]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, [rbp - 40]
    push rax
    lea rax, [rel _VL_str_14]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ----- demo_compound -----
global demo_compound
demo_compound:
    push rbp
    mov  rbp, rsp
    sub  rsp, 8192
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    mov rax, 10
    mov [rbp - 8], rax
    mov rax, [rbp - 8]
    push rax
    mov rax, 5
    mov rbx, rax
    pop rax
    add rax, rbx
    mov [rbp - 8], rax
    mov rax, [rbp - 8]
    push rax
    lea rax, [rel _VL_str_15]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, [rbp - 8]
    push rax
    mov rax, 3
    mov rbx, rax
    pop rax
    sub rax, rbx
    mov [rbp - 8], rax
    mov rax, [rbp - 8]
    push rax
    lea rax, [rel _VL_str_16]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, [rbp - 8]
    push rax
    mov rax, 2
    mov rbx, rax
    pop rax
    imul rax, rbx
    mov [rbp - 8], rax
    mov rax, [rbp - 8]
    push rax
    lea rax, [rel _VL_str_17]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, [rbp - 8]
    push rax
    mov rax, 4
    mov rbx, rax
    pop rax
    cqo
    idiv rbx
    mov [rbp - 8], rax
    mov rax, [rbp - 8]
    push rax
    lea rax, [rel _VL_str_18]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ----- demo_dynamic_array -----
global demo_dynamic_array
demo_dynamic_array:
    push rbp
    mov  rbp, rsp
    sub  rsp, 8192
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    mov rcx, 1
    mov rdx, 8
    sub rsp, 32
    call __vl_arr_alloc
    add rsp, 32
    mov qword [rax - 8], 0
    mov [rbp - 8], rax
    mov rax, 10
    push rax
    lea rcx, [rbp - 8]
    pop rdx
    mov r8, 8
    sub rsp, 32
    call __vl_arr_append
    add rsp, 32
    mov rax, 20
    push rax
    lea rcx, [rbp - 8]
    pop rdx
    mov r8, 8
    sub rsp, 32
    call __vl_arr_append
    add rsp, 32
    mov rax, 30
    push rax
    lea rcx, [rbp - 8]
    pop rdx
    mov r8, 8
    sub rsp, 32
    call __vl_arr_append
    add rsp, 32
    mov rax, [rbp - 8]
    mov rbx, rax
    mov rax, [rbx - 8]
    push rax
    lea rax, [rel _VL_str_19]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, 0
    mov rcx, rax
    mov rbx, [rbp - 8]
    mov rax, [rbx + rcx*8]
    push rax
    lea rax, [rel _VL_str_20]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, 1
    mov rcx, rax
    mov rbx, [rbp - 8]
    mov rax, [rbx + rcx*8]
    push rax
    lea rax, [rel _VL_str_21]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, 2
    mov rcx, rax
    mov rbx, [rbp - 8]
    mov rax, [rbx + rcx*8]
    push rax
    lea rax, [rel _VL_str_22]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, 1
    mov [rbp - 40], rax
    mov rax, 2
    mov [rbp - 32], rax
    mov rax, 3
    mov [rbp - 24], rax
    mov rax, 4
    mov [rbp - 16], rax
    mov rax, 2
    mov rcx, rax
    lea rbx, [rbp - 40]
    push rcx
    push rbx
    mov rax, 99
    pop rbx
    pop rcx
    mov [rbx + rcx*8], rax
    mov rax, 2
    mov rcx, rax
    lea rbx, [rbp - 40]
    mov rax, [rbx + rcx*8]
    push rax
    lea rax, [rel _VL_str_23]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    lea rax, [rbp - 40]
    mov rax, 4
    push rax
    lea rax, [rel _VL_str_24]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ----- demo_struct_with_arrays -----
global demo_struct_with_arrays
demo_struct_with_arrays:
    push rbp
    mov  rbp, rsp
    sub  rsp, 8192
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    lea rbx, [rbp - 40]
    lea rdx, [rbx + 0]
    push rbx
    mov rbx, rdx
    push rbx
    mov rax, 100
    pop rbx
    cvtsi2sd xmm0, rax
    movsd [rbx + 0], xmm0
    push rbx
    mov rax, 200
    pop rbx
    cvtsi2sd xmm0, rax
    movsd [rbx + 8], xmm0
    pop rbx
    lea rdx, [rbx + 16]
    push rbx
    mov rbx, rdx
    push rbx
    mov rax, 255
    pop rbx
    mov [rbx + 0], rax
    push rbx
    mov rax, 128
    pop rbx
    mov [rbx + 8], rax
    push rbx
    mov rax, 0
    pop rbx
    mov [rbx + 16], rax
    pop rbx
    lea rax, [rbp - 40]
    add rax, 0
    mov rbx, rax
    movsd xmm0, [rbx + 8]
    movq rax, xmm0
    push rax
    lea rax, [rbp - 40]
    add rax, 0
    mov rbx, rax
    movsd xmm0, [rbx + 0]
    movq rax, xmm0
    push rax
    lea rax, [rel _VL_str_25]
    push rax
    pop rcx
    pop rdx
    pop r8
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    lea rax, [rbp - 40]
    add rax, 16
    mov rbx, rax
    mov rax, [rbx + 16]
    push rax
    lea rax, [rbp - 40]
    add rax, 16
    mov rbx, rax
    mov rax, [rbx + 8]
    push rax
    lea rax, [rbp - 40]
    add rax, 16
    mov rbx, rax
    mov rax, [rbx + 0]
    push rax
    lea rax, [rel _VL_str_26]
    push rax
    pop rcx
    pop rdx
    pop r8
    pop r9
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ----- safe_divide -----
global safe_divide
safe_divide:
    push rbp
    mov  rbp, rsp
    sub  rsp, 8192
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    mov [rbp - 8], rcx
    mov [rbp - 16], rdx
    mov rax, [rbp - 16]
    push rax
    mov rax, 0
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    sete  al
    cmp rax, 0
    je  _VL4
    lea rax, [rel _VL_str_27]
    mov rcx, rax
    sub rsp, 32
    call __vl_panic
    add rsp, 32
    xor rax, rax
    mov rsp, rbp
    pop rbp
    ret
    jmp _VL5
_VL4:
_VL5:
    mov rax, [rbp - 8]
    push rax
    mov rax, [rbp - 16]
    mov rbx, rax
    pop rax
    cqo
    idiv rbx
    mov rsp, rbp
    pop rbp
    ret
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ----- demo_error_handling -----
global demo_error_handling
demo_error_handling:
    push rbp
    mov  rbp, rsp
    sub  rsp, 8192
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    mov rax, 2
    push rax
    mov rax, 10
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call safe_divide
    add rsp, 32
    mov [rbp - 8], rax
    mov rax, [rbp - 8]
    push rax
    lea rax, [rel _VL_str_28]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov qword [rbp - 16], 0
    add qword [rel _VL_try_depth_rt], 1
    mov rax, 0
    push rax
    mov rax, 5
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call safe_divide
    add rsp, 32
    cmp qword [rel _VL_err_flag], 0
    je  _VL8
    mov rax, [rel _VL_err_msg]
    mov [rbp - 16], rax
    mov qword [rel _VL_err_flag], 0
    mov qword [rel _VL_err_msg], 0
    sub qword [rel _VL_try_depth_rt], 1
    jmp _VL6
_VL8:
    mov [rbp - 24], rax
    mov rax, [rbp - 24]
    push rax
    lea rax, [rel _VL_str_29]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    cmp qword [rel _VL_err_flag], 0
    je  _VL9
    mov rax, [rel _VL_err_msg]
    mov [rbp - 16], rax
    mov qword [rel _VL_err_flag], 0
    mov qword [rel _VL_err_msg], 0
    sub qword [rel _VL_try_depth_rt], 1
    jmp _VL6
_VL9:
    sub qword [rel _VL_try_depth_rt], 1
    jmp _VL7
_VL6:
    mov qword [rel _VL_err_flag], 0
    mov qword [rel _VL_err_msg], 0
    mov rax, [rbp - 16]
    push rax
    lea rax, [rel _VL_str_30]
    push rax
    pop rcx
    pop rdx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
_VL7:
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ----- demo_discard -----
global demo_discard
demo_discard:
    push rbp
    mov  rbp, rsp
    sub  rsp, 8192
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    lea rax, [rel _VL_str_31]
    lea rax, [rel _VL_str_32]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ----- classify -----
global classify
classify:
    push rbp
    mov  rbp, rsp
    sub  rsp, 8192
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    mov [rbp - 8], rcx
    mov rax, [rbp - 8]
    push rax
    mov rax, 0
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setl  al
    cmp rax, 0
    je  _VL10
    lea rax, [rel _VL_str_33]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    jmp _VL11
_VL10:
    mov rax, [rbp - 8]
    push rax
    mov rax, 0
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    sete  al
    cmp rax, 0
    je  _VL12
    lea rax, [rel _VL_str_34]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    jmp _VL13
_VL12:
    mov rax, [rbp - 8]
    push rax
    mov rax, 10
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setl  al
    cmp rax, 0
    je  _VL14
    lea rax, [rel _VL_str_35]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    jmp _VL15
_VL14:
    mov rax, [rbp - 8]
    push rax
    mov rax, 100
    mov rbx, rax
    pop rax
    cmp rax, rbx
    mov rax, 0
    setl  al
    cmp rax, 0
    je  _VL16
    lea rax, [rel _VL_str_36]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    jmp _VL17
_VL16:
    lea rax, [rel _VL_str_37]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
_VL17:
_VL15:
_VL13:
_VL11:
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ----- main_vel -----
global main_vel
main_vel:
    push rbp
    mov  rbp, rsp
    sub  rsp, 8192
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    lea rax, [rel _VL_str_38]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    lea rax, [rel _VL_str_39]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    sub rsp, 32
    call demo_types
    add rsp, 32
    lea rax, [rel _VL_str_40]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    sub rsp, 32
    call demo_literals
    add rsp, 32
    lea rax, [rel _VL_str_41]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    sub rsp, 32
    call demo_bitwise
    add rsp, 32
    lea rax, [rel _VL_str_42]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    sub rsp, 32
    call demo_logical
    add rsp, 32
    lea rax, [rel _VL_str_43]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    sub rsp, 32
    call demo_compound
    add rsp, 32
    lea rax, [rel _VL_str_44]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    sub rsp, 32
    call demo_dynamic_array
    add rsp, 32
    lea rax, [rel _VL_str_45]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    sub rsp, 32
    call demo_struct_with_arrays
    add rsp, 32
    lea rax, [rel _VL_str_46]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    sub rsp, 32
    call demo_error_handling
    add rsp, 32
    lea rax, [rel _VL_str_47]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    sub rsp, 32
    call demo_discard
    add rsp, 32
    lea rax, [rel _VL_str_48]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, 0
    push rax
    mov rax, 5
    mov rbx, rax
    pop rax
    sub rax, rbx
    push rax
    pop rcx
    sub rsp, 32
    call classify
    add rsp, 32
    mov rax, 0
    push rax
    pop rcx
    sub rsp, 32
    call classify
    add rsp, 32
    mov rax, 7
    push rax
    pop rcx
    sub rsp, 32
    call classify
    add rsp, 32
    mov rax, 42
    push rax
    pop rcx
    sub rsp, 32
    call classify
    add rsp, 32
    mov rax, 999
    push rax
    pop rcx
    sub rsp, 32
    call classify
    add rsp, 32
    lea rax, [rel _VL_str_49]
    push rax
    pop rcx
    sub rsp, 32
    call io__chhaap
    add rsp, 32
    sub rsp, 32
    call io__chhaapLine
    add rsp, 32
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    mov rsp, rbp
    pop rbp
    ret

; ----- entry point -----
global main
extern GetCommandLineA
extern ExitProcess
main:
    push rbp
    mov  rbp, rsp
    sub  rsp, 64
    mov [rel _VL_argc], rcx
    mov [rel _VL_argv], rdx
    xor rax, rax
    mov [rel _VL_envp], rax
    sub rsp, 32
    call main_vel
    add rsp, 32
    mov rcx, rax
    sub rsp, 32
    call ExitProcess
    add rsp, 32
    pop rbp
    ret

global _VL_argc
global _VL_argv
global _VL_envp
section .bss
    _VL_argc resq 1
    _VL_argv resq 1
    _VL_envp resq 1

section .data
_VL_str_0:
    db 97,100,97,100,58,32,37,100,0
_VL_str_1:
    db 117,97,100,97,100,58,32,37,100,0
_VL_str_2:
    db 117,97,100,97,100,58,32,37,100,10,0
_VL_str_3:
    db 117,97,100,97,100,56,58,32,37,100,0
_VL_str_4:
    db 104,101,120,32,48,120,70,70,32,61,32,37,100,0
_VL_str_5:
    db 98,105,110,32,48,98,49,49,48,48,49,48,49,48,32,61,32,37,100,0
_VL_str_6:
    db 111,99,116,32,48,111,55,55,55,32,61,32,37,100,0
_VL_str_7:
    db 49,48,32,38,32,49,50,32,61,32,37,100,0
_VL_str_8:
    db 49,48,32,124,32,49,50,32,61,32,37,100,0
_VL_str_9:
    db 49,48,32,94,32,49,50,32,61,32,37,100,0
_VL_str_10:
    db 49,48,32,60,60,32,50,32,61,32,37,100,0
_VL_str_11:
    db 49,48,32,62,62,32,49,32,61,32,37,100,0
_VL_str_12:
    db 112,111,122,32,38,38,32,97,112,117,122,32,61,32,37,98,0
_VL_str_13:
    db 112,111,122,32,124,124,32,97,112,117,122,32,61,32,37,98,0
_VL_str_14:
    db 33,112,111,122,32,61,32,37,98,0
_VL_str_15:
    db 49,48,32,43,61,32,53,32,45,62,32,37,100,0
_VL_str_16:
    db 45,61,32,51,32,45,62,32,37,100,0
_VL_str_17:
    db 42,61,32,50,32,45,62,32,37,100,0
_VL_str_18:
    db 47,61,32,52,32,45,62,32,37,100,0
_VL_str_19:
    db 97,114,114,32,108,101,110,103,116,104,32,97,102,116,101,114,32,51,32,97,112,112,101,110,100,115,58,32,37,100,0
_VL_str_20:
    db 97,114,114,91,48,93,32,61,32,37,100,0
_VL_str_21:
    db 97,114,114,91,49,93,32,61,32,37,100,0
_VL_str_22:
    db 97,114,114,91,50,93,32,61,32,37,100,0
_VL_str_23:
    db 102,105,120,101,100,91,50,93,32,97,102,116,101,114,32,97,115,115,105,103,110,58,32,37,100,0
_VL_str_24:
    db 108,101,110,40,102,105,120,101,100,41,32,61,32,37,100,0
_VL_str_25:
    db 80,105,120,101,108,32,112,111,115,58,32,40,37,102,44,32,37,102,41,0
_VL_str_26:
    db 67,111,108,111,114,32,82,71,66,58,32,40,37,100,44,32,37,100,44,32,37,100,41,0
_VL_str_27:
    db 68,105,118,105,115,105,111,110,32,98,121,32,122,101,114,111,33,10,0
_VL_str_28:
    db 49,48,32,47,32,50,32,61,32,37,100,0
_VL_str_29:
    db 83,104,111,117,108,100,32,110,111,116,32,114,101,97,99,104,32,104,101,114,101,58,32,37,100,0
_VL_str_30:
    db 67,97,117,103,104,116,32,101,114,114,111,114,58,32,37,115,0
_VL_str_31:
    db 104,101,108,112,0
_VL_str_32:
    db 95,32,100,105,115,99,97,114,100,32,119,111,114,107,115,10,0
_VL_str_33:
    db 110,101,103,97,116,105,118,101,0
_VL_str_34:
    db 122,101,114,111,0
_VL_str_35:
    db 115,109,97,108,108,0
_VL_str_36:
    db 109,101,100,105,117,109,0
_VL_str_37:
    db 108,97,114,103,101,0
_VL_str_38:
    db 61,61,61,32,86,101,108,111,99,105,116,121,32,118,50,32,70,101,97,116,117,114,101,32,68,101,109,111,32,61,61,61,0
_VL_str_39:
    db 45,45,45,32,84,121,112,101,115,32,45,45,45,0
_VL_str_40:
    db 45,45,45,32,76,105,116,101,114,97,108,115,32,45,45,45,0
_VL_str_41:
    db 45,45,45,32,66,105,116,119,105,115,101,32,45,45,45,0
_VL_str_42:
    db 45,45,45,32,76,111,103,105,99,97,108,32,45,45,45,0
_VL_str_43:
    db 45,45,45,32,67,111,109,112,111,117,110,100,32,65,115,115,105,103,110,109,101,110,116,32,45,45,45,0
_VL_str_44:
    db 45,45,45,32,68,121,110,97,109,105,99,32,65,114,114,97,121,32,43,32,97,112,112,101,110,100,32,45,45,45,0
_VL_str_45:
    db 45,45,45,32,83,116,114,117,99,116,32,119,105,116,104,32,65,114,114,97,121,115,32,45,45,45,0
_VL_str_46:
    db 45,45,45,32,69,114,114,111,114,32,72,97,110,100,108,105,110,103,32,45,45,45,0
_VL_str_47:
    db 45,45,45,32,68,105,115,99,97,114,100,32,45,45,45,0
_VL_str_48:
    db 45,45,45,32,67,108,97,115,115,105,102,121,32,45,45,45,0
_VL_str_49:
    db 61,61,61,32,68,111,110,101,32,61,61,61,0
section .bss
    _VL_arr_heap resb 131072
    _VL_arr_hp   resq 1

section .text
__vl_arr_alloc:
    push rbp
    mov  rbp, rsp
    push rbx
    mov  rax, [rel _VL_arr_hp]
    test rax, rax
    jne  .have_w
    lea  rax, [rel _VL_arr_heap]
    mov  [rel _VL_arr_hp], rax
.have_w:
    mov  rbx, rax
    mov  [rbx], rcx
    lea  rax, [rbx + 8]
    mov  r10, rcx
    imul r10, rdx
    add  r10, 8
    add  rbx, r10
    mov  [rel _VL_arr_hp], rbx
    pop  rbx
    pop  rbp
    ret

__vl_arr_append:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    mov  r12, rcx
    mov  r13, rdx
    mov  r14, r8
    mov  rbx, [r12]
    test rbx, rbx
    jz   .append_new
    mov  rax, [rbx - 8]
    mov  rcx, rax
    add  rax, 1
    mov  [rbx - 8], rax
    mov  r10, rcx
    imul r10, r14
    lea  r11, [rbx + r10]
    cmp  r14, 8
    jg   .copy_existing
    cmp  r14, 1
    je   .store_u8_existing
    mov  [r11], r13
    jmp  .append_done
.store_u8_existing:
    mov  [r11], r13b
    jmp  .append_done
.copy_existing:
    mov  rdi, r11
    mov  rsi, r13
    mov  rcx, r14
    rep movsb
    jmp  .append_done
.append_new:
    mov  rcx, 1
    mov  rdx, r14
    call __vl_arr_alloc
    mov  [r12], rax
    cmp  r14, 8
    jg   .copy_new
    cmp  r14, 1
    je   .store_u8_new
    mov  [rax], r13
    jmp  .append_done
.store_u8_new:
    mov  [rax], r13b
    jmp  .append_done
.copy_new:
    mov  rdi, rax
    mov  rsi, r13
    mov  rcx, r14
    rep movsb
.append_done:
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp
    ret

section .bss
    _VL_try_depth_rt resq 1
    _VL_err_flag     resq 1
    _VL_err_msg      resq 1
section .text
__vl_panic:
    push rbp
    mov  rbp, rsp
    mov  rax, [rel _VL_try_depth_rt]
    test rax, rax
    jz   .panic_fatal
    mov  [rel _VL_err_msg], rcx
    mov  qword [rel _VL_err_flag], 1
    pop  rbp
    ret
.panic_fatal:
    push rbx
    mov  rbx, rcx
    mov  r10, rbx
    xor  r11, r11
.panic_len:
    cmp  byte [r10 + r11], 0
    je   .panic_write
    inc  r11
    jmp  .panic_len
.panic_write:
    sub  rsp, 40
    mov  rcx, -12
    extern GetStdHandle
    call GetStdHandle
    add  rsp, 40
    mov  r12, rax
    sub  rsp, 40
    mov  rcx, r12
    mov  rdx, rbx
    mov  r8,  r11
    xor  r9,  r9
    push 0
    extern WriteFile
    call WriteFile
    add  rsp, 48
    pop  rbx
    sub  rsp, 40
    mov  rcx, 1
    extern ExitProcess
    call ExitProcess
    add  rsp, 40
    pop  rbp
    ret

