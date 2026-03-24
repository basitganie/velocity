; ============================================================
; Velocity Standard Library - System Module (system.asm)
; Linux syscalls + process globals
;
; Functions:
;   system__exit(code)        -> adad
;   system__argc()            -> adad
;   system__argv(i)           -> lafz? (null if out of range)
;   system__getenv(key)       -> lafz? (null if missing)
; ============================================================

extern _VL_argc
extern _VL_argv
extern _VL_envp

section .text

; ── system__exit ──────────────────────────────────────────
; rdi = exit code
global system__exit
system__exit:
    mov rax, 60
    syscall

; ── system__argc ──────────────────────────────────────────
global system__argc
system__argc:
    mov rax, [rel _VL_argc]
    ret

; ── system__argv ──────────────────────────────────────────
; rdi = index
; rax = argv[i] or 0
global system__argv
system__argv:
    mov rax, [rel _VL_argc]
    cmp rdi, rax
    jae .out_of_range
    mov rbx, [rel _VL_argv]
    mov rax, [rbx + rdi*8]
    ret
.out_of_range:
    xor rax, rax
    ret

; ── system__getenv ────────────────────────────────────────
; rdi = key (null-terminated)
; rax = value pointer or 0
global system__getenv
system__getenv:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    push r14

    mov r12, rdi          ; key
    mov rbx, [rel _VL_envp]

.env_loop:
    mov r13, [rbx]
    test r13, r13
    jz   .not_found

    mov rdi, r12          ; key ptr
    mov rsi, r13          ; env ptr
.compare:
    mov al, [rdi]
    mov dl, [rsi]
    cmp al, 0
    je  .key_end
    cmp al, dl
    jne .next_env
    inc rdi
    inc rsi
    jmp .compare
.key_end:
    cmp dl, '='
    jne .next_env
    lea rax, [rsi + 1]
    jmp .done

.next_env:
    add rbx, 8
    jmp .env_loop

.not_found:
    xor rax, rax

.done:
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
