; ============================================================
; Velocity StdLib - System Module (system.asm)
; Windows + Linux dual-mode
; Functions: system__exit  system__argc  system__argv  system__getenv
; ============================================================

%ifndef WIN64_TARGET
%ifidni __OUTPUT_FORMAT__,win64
  %define WIN64_TARGET 1
%endif
%endif
extern _VL_argc
extern _VL_argv
extern _VL_envp

section .text

; system__exit(code)
global system__exit
system__exit:
%ifdef WIN64_TARGET
    ; rcx = exit code already
    sub  rsp, 32
    call ExitProcess
    add  rsp, 32
%else
    mov  rdi, rdi    ; already set
    mov  rax, 60
    syscall
%endif

; system__argc() -> adad
global system__argc
system__argc:
    mov  rax, [rel _VL_argc]
    ret

; system__argv(i: adad) -> lafz?
global system__argv
system__argv:
%ifdef WIN64_TARGET
    mov  rdi, rcx
%endif
    mov  rax, [rel _VL_argc]
    cmp  rdi, rax
    jae  .oor
    mov  rbx, [rel _VL_argv]
    mov  rax, [rbx + rdi*8]
    ret
.oor:
    xor  rax, rax
    ret

; system__getenv(key: lafz) -> lafz?
global system__getenv
system__getenv:
%ifdef WIN64_TARGET
    mov  rdi, rcx
%endif
    push rbx
    push r12
    push r13
    push r14
    mov  r12, rdi
    mov  rbx, [rel _VL_envp]
.env_loop:
    mov  r13, [rbx]
    test r13, r13
    jz   .not_found
    mov  rdi, r12
    mov  rsi, r13
.cmp:
    mov  al, [rdi]
    mov  dl, [rsi]
    cmp  al, 0
    je   .key_end
    cmp  al, dl
    jne  .next_env
    inc  rdi
    inc  rsi
    jmp  .cmp
.key_end:
    cmp  dl, '='
    jne  .next_env
    lea  rax, [rsi + 1]
    jmp  .done
.next_env:
    add  rbx, 8
    jmp  .env_loop
.not_found:
    xor  rax, rax
.done:
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    ret

%ifdef WIN64_TARGET
extern ExitProcess
%endif
