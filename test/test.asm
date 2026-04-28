; Velocity Compiler v2 - Kashmiri Edition
; Target: Linux ELF64 (System V AMD64 ABI)

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

; ----- main -----
global main
main:
    push rbp
    mov  rbp, rsp
    sub  rsp, 8192
    mov rax, 1
    push rax
    lea rax, [rel _VL_str_0]
    push rax
    pop rdi
    pop rsi
    call io__open
    mov [rbp - 8], rax
    call io__Read
    mov [rbp - 16], rax
    mov rax, [rbp - 8]
    push rax
    pop rdi
    call io__close
    mov [rbp - 24], rax
    mov rax, [rbp - 24]
    push rax
    mov rax, [rbp - 8]
    push rax
    lea rax, [rel _VL_str_1]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call io__chhaap
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
    mov rsp, rbp
    pop rbp
    ret

; ----- entry point -----
global _start
_start:
    mov rbx, rsp
    mov rax, [rbx]
    mov [rel _VL_argc], rax
    lea rcx, [rbx + 8]
    mov [rel _VL_argv], rcx
    mov rdx, rax
    add rdx, 1
    shl rdx, 3
    add rcx, rdx
    mov [rel _VL_envp], rcx
    call main
    mov  rdi, rax
    mov  rax, 60
    syscall
global _VL_argc
global _VL_argv
global _VL_envp
section .bss
    _VL_argc resq 1
    _VL_argv resq 1
    _VL_envp resq 1

section .data
_VL_str_0:
    db 116,101,115,116,47,116,101,115,116,49,46,118,101,108,0
_VL_str_1:
    db 37,100,44,32,37,100,10,0
