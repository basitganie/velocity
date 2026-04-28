; ============================================================
; Velocity StdLib - OS Module (os.asm)
; Windows + Linux dual-mode
; Functions: os__getcwd  os__chdir
; ============================================================

%ifndef WIN64_TARGET
%ifidni __OUTPUT_FORMAT__,win64
  %define WIN64_TARGET 1
%endif
%endif
section .bss
    os_cwd_buf resb 4096

section .text

; os__getcwd() -> lafz?
global os__getcwd
os__getcwd:
%ifdef WIN64_TARGET
    sub  rsp, 32
    lea  rcx, [rel os_cwd_buf]
    mov  rdx, 4096
    call GetCurrentDirectoryA
    add  rsp, 32
    test rax, rax
    jz   .fail
    lea  rax, [rel os_cwd_buf]
    ret
.fail:
    xor  rax, rax
    ret
%else
    mov  rax, 79
    lea  rdi, [rel os_cwd_buf]
    mov  rsi, 4096
    syscall
    cmp  rax, 0
    jl   .fail
    lea  rax, [rel os_cwd_buf]
    ret
.fail:
    xor  rax, rax
    ret
%endif

; os__chdir(path: lafz) -> adad
global os__chdir
os__chdir:
%ifdef WIN64_TARGET
    ; rcx = path (already set)
    sub  rsp, 32
    call SetCurrentDirectoryA
    add  rsp, 32
    ; convert BOOL to 0/nonzero
    test rax, rax
    jnz  .ok
    mov  rax, -1
    ret
.ok:
    xor  rax, rax
    ret
%else
    mov  rax, 80
    syscall
    ret
%endif

%ifdef WIN64_TARGET
extern GetCurrentDirectoryA
extern SetCurrentDirectoryA
%endif
