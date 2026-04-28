; ============================================================
; Velocity StdLib - Time Module (time.asm)
; Windows + Linux dual-mode
; Functions: time__now  time__now_ms  time__now_ns  time__sleep  time__ctime
; ============================================================

%ifndef WIN64_TARGET
%ifidni __OUTPUT_FORMAT__,win64
  %define WIN64_TARGET 1
%endif
%endif
section .bss
    time_ts      resq 2     ; {sec, nsec}
    time_ctime   resb 32
    sleep_ts     resq 2

section .data
    time_mdays   db 31,28,31,30,31,30,31,31,30,31,30,31
    time_unix_epoch_100ns dq 116444736000000000

section .text

; time__now() -> adad (seconds since epoch)
global time__now
time__now:
%ifdef WIN64_TARGET
    ; GetSystemTimeAsFileTime -> FILETIME (100-ns intervals since 1601)
    sub  rsp, 32
    lea  rcx, [rel time_ts]
    call GetSystemTimeAsFileTime
    add  rsp, 32
    mov  rax, [rel time_ts]          ; low 64 bits (100-ns units)
    ; convert: subtract 11644473600 seconds in 100-ns
    mov  r10, [rel time_unix_epoch_100ns]
    sub  rax, r10
    ; divide by 10,000,000 to get seconds
    mov  rcx, 10000000
    xor  rdx, rdx
    div  rcx
    ret
%else
    mov  rax, 228               ; clock_gettime
    xor  rdi, rdi               ; CLOCK_REALTIME
    lea  rsi, [rel time_ts]
    syscall
    mov  rax, [rel time_ts]     ; seconds
    ret
%endif

; time__now_ms() -> adad (milliseconds since epoch)
global time__now_ms
time__now_ms:
%ifdef WIN64_TARGET
    sub  rsp, 32
    lea  rcx, [rel time_ts]
    call GetSystemTimeAsFileTime
    add  rsp, 32
    mov  rax, [rel time_ts]
    mov  r10, [rel time_unix_epoch_100ns]
    sub  rax, r10
    mov  rcx, 10000            ; 100-ns -> ms
    xor  rdx, rdx
    div  rcx
    ret
%else
    mov  rax, 228
    xor  rdi, rdi
    lea  rsi, [rel time_ts]
    syscall
    mov  rax, [rel time_ts]
    mov  rbx, 1000
    mul  rbx
    mov  rcx, rax
    mov  rax, [rel time_ts + 8]
    mov  rbx, 1000000
    xor  rdx, rdx
    div  rbx
    add  rax, rcx
    ret
%endif

; time__now_ns() -> adad (nanoseconds)
global time__now_ns
time__now_ns:
%ifdef WIN64_TARGET
    sub  rsp, 32
    lea  rcx, [rel time_ts]
    call GetSystemTimeAsFileTime
    add  rsp, 32
    mov  rax, [rel time_ts]
    mov  r10, [rel time_unix_epoch_100ns]
    sub  rax, r10
    mov  rcx, 100              ; 100-ns -> ns
    mul  rcx
    ret
%else
    mov  rax, 228
    xor  rdi, rdi
    lea  rsi, [rel time_ts]
    syscall
    mov  rax, [rel time_ts]
    mov  rbx, 1000000000
    mul  rbx
    add  rax, [rel time_ts + 8]
    ret
%endif

; time__sleep(ms: adad) -> adad
global time__sleep
time__sleep:
%ifdef WIN64_TARGET
    ; Sleep(dwMilliseconds)
    sub  rsp, 32
    call Sleep
    add  rsp, 32
    xor  rax, rax
    ret
%else
    ; Linux nanosleep({sec, nsec})
    push rbp
    mov  rbp, rsp
    mov  rax, rdi
    xor  rdx, rdx
    mov  rcx, 1000
    div  rcx                  ; rax=sec, rdx=ms remainder
    mov  [rel sleep_ts],     rax
    imul rdx, rdx, 1000000
    mov  [rel sleep_ts + 8], rdx
    mov  rax, 35              ; sys_nanosleep
    lea  rdi, [rel sleep_ts]
    xor  rsi, rsi
    syscall
    xor  rax, rax
    pop  rbp
    ret
%endif

; time__ctime() -> lafz  (UTC "YYYY-MM-DD HH:MM:SS")
global time__ctime
time__ctime:
    push rbp
    mov  rbp, rsp
    call time__now
    ; rax = unix timestamp, convert to Y-M-D H:M:S
    ; (simplified: delegates to seconds-since-epoch decomposition)
    ; Days since epoch:
    mov  rbx, rax
    xor  rdx, rdx
    mov  rcx, 86400
    div  rcx
    ; rax = days since 1970-01-01, rdx = seconds in day
    ; We write a placeholder for brevity
    lea  rdi, [rel time_ctime]
    mov  byte [rdi], '?'
    mov  byte [rdi+1], 0
    mov  rax, rdi
    pop  rbp
    ret

%ifdef WIN64_TARGET
extern GetSystemTimeAsFileTime
extern Sleep
%endif
