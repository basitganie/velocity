; ============================================================
; Velocity Standard Library - OS Module (os.asm)
; Linux syscalls only
;
; Functions:
;   os__getcwd()       -> lafz? (null on failure)
;   os__chdir(path)    -> adad  (0 on success, -errno on failure)
; ============================================================

section .bss
    os_cwd_buf resb 4096

section .text

; ── os__getcwd ────────────────────────────────────────────
global os__getcwd
os__getcwd:
    mov rax, 79             ; getcwd
    lea rdi, [rel os_cwd_buf]
    mov rsi, 4096
    syscall
    cmp rax, 0
    jl  .fail
    lea rax, [rel os_cwd_buf]
    ret
.fail:
    xor rax, rax
    ret

; ── os__chdir ─────────────────────────────────────────────
global os__chdir
os__chdir:
    mov rax, 80             ; chdir
    syscall
    ret
