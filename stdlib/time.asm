; ============================================================
; Velocity Standard Library - Time Module (time.asm)
; Linux syscalls only (no libc)
;
; Functions:
;   time__now()      -> adad  (seconds since epoch)
;   time__now_ms()   -> adad  (milliseconds since epoch)
;   time__now_ns()   -> adad  (nanoseconds since epoch)
;   time__sleep(ms)  -> adad  (sleep milliseconds)
;   time__ctime()    -> lafz  (UTC "YYYY-MM-DD HH:MM:SS")
; ============================================================

section .bss
    time_ts    resq 2   ; timespec {sec, nsec}
    sleep_ts   resq 2
    ctime_buf  resb 32

section .data
    month_days db 31,28,31,30,31,30,31,31,30,31,30,31

section .text

; ── time__now ──────────────────────────────────────────────
; rax = seconds since epoch
global time__now
time__now:
    mov rax, 228         ; clock_gettime
    mov rdi, 0           ; CLOCK_REALTIME
    lea rsi, [rel time_ts]
    syscall
    mov rax, [rel time_ts]
    ret

; ── time__now_ms ───────────────────────────────────────────
; rax = milliseconds since epoch
global time__now_ms
time__now_ms:
    mov rax, 228
    mov rdi, 0
    lea rsi, [rel time_ts]
    syscall
    mov rax, [rel time_ts]      ; sec
    mov rbx, 1000
    mul rbx                     ; rdx:rax = sec*1000
    mov rcx, rax                ; rcx = sec*1000
    mov rax, [rel time_ts + 8]  ; nsec
    mov rbx, 1000000
    xor rdx, rdx
    div rbx                     ; rax = nsec/1e6
    add rax, rcx
    ret

; ── time__now_ns ───────────────────────────────────────────
; rax = nanoseconds since epoch
global time__now_ns
time__now_ns:
    mov rax, 228
    mov rdi, 0
    lea rsi, [rel time_ts]
    syscall
    mov rax, [rel time_ts]      ; sec
    mov rbx, 1000000000
    mul rbx                     ; rdx:rax = sec*1e9
    mov rcx, rax
    mov rax, [rel time_ts + 8]  ; nsec
    add rax, rcx
    ret

; ── time__sleep ────────────────────────────────────────────
; rdi = ms
global time__sleep
time__sleep:
    push rbp
    mov  rbp, rsp
    mov  rax, rdi
    mov  rbx, 1000
    xor  rdx, rdx
    div  rbx                    ; rax = sec, rdx = ms
    mov  [rel sleep_ts], rax
    mov  rax, rdx
    mov  rbx, 1000000
    mul  rbx                    ; rdx:rax = ms*1e6
    mov  [rel sleep_ts + 8], rax
    mov  rax, 35                ; nanosleep
    lea  rdi, [rel sleep_ts]
    xor  rsi, rsi
    syscall
    mov  rax, 0
    pop  rbp
    ret

; ── time__ctime ────────────────────────────────────────────
; returns pointer to static buffer "YYYY-MM-DD HH:MM:SS"
; UTC conversion (no timezone)
global time__ctime
time__ctime:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; get seconds
    mov rax, 228
    mov rdi, 0
    lea rsi, [rel time_ts]
    syscall
    mov rax, [rel time_ts]      ; seconds since epoch

    ; days = sec / 86400, rem = sec % 86400
    mov rbx, 86400
    xor rdx, rdx
    div rbx
    mov r12, rax                ; days
    mov r13, rdx                ; rem

    ; hour/min/sec from rem
    mov rax, r13
    mov rbx, 3600
    xor rdx, rdx
    div rbx
    mov r14, rax                ; hour
    mov r15, rdx                ; rem

    mov rax, r15
    mov rbx, 60
    xor rdx, rdx
    div rbx
    mov r15, rax                ; min
    mov r13, rdx                ; sec

    ; year = 1970
    mov rbx, 1970
.year_loop:
    mov rax, rbx
    call .is_leap
    mov rcx, 365
    add rcx, rax                ; days in year
    cmp r12, rcx
    jb .year_done
    sub r12, rcx
    inc rbx
    jmp .year_loop
.year_done:
    mov r8, rbx                 ; year

    ; month loop
    xor r9, r9                  ; month index 0..11
.month_loop:
    lea rbx, [rel month_days]
    movzx rax, byte [rbx + r9]
    mov rcx, rax
    cmp r9, 1
    jne .month_check
    mov rax, r8
    call .is_leap
    add rcx, rax
.month_check:
    cmp r12, rcx
    jb .month_done
    sub r12, rcx
    inc r9
    jmp .month_loop
.month_done:
    mov r10, r9
    inc r10                     ; month 1..12
    mov r11, r12
    inc r11                     ; day 1..31

    ; build string
    lea rdi, [rel ctime_buf]
    mov rax, r8
    call .write_4
    mov byte [rdi], '-'
    inc rdi
    mov rax, r10
    call .write_2
    mov byte [rdi], '-'
    inc rdi
    mov rax, r11
    call .write_2
    mov byte [rdi], ' '
    inc rdi
    mov rax, r14
    call .write_2
    mov byte [rdi], ':'
    inc rdi
    mov rax, r15
    call .write_2
    mov byte [rdi], ':'
    inc rdi
    mov rax, r13
    call .write_2
    mov byte [rdi], 0

    lea rax, [rel ctime_buf]

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

; rax=year, returns rax=1 if leap else 0
.is_leap:
    push rbx
    mov rsi, rax
    mov rbx, 4
    xor rdx, rdx
    div rbx
    cmp rdx, 0
    jne .not_leap
    mov rax, rsi
    mov rbx, 100
    xor rdx, rdx
    div rbx
    cmp rdx, 0
    jne .leap
    mov rax, rsi
    mov rbx, 400
    xor rdx, rdx
    div rbx
    cmp rdx, 0
    je .leap
.not_leap:
    xor rax, rax
    pop rbx
    ret
.leap:
    mov rax, 1
    pop rbx
    ret

; rax=value, writes 2 digits at [rdi], advances rdi by 2
.write_2:
    push rbx
    mov rbx, 10
    xor rdx, rdx
    div rbx                    ; rax=tens, rdx=ones
    add al, '0'
    mov [rdi], al
    inc rdi
    mov al, dl
    add al, '0'
    mov [rdi], al
    inc rdi
    pop rbx
    ret

; rax=value, writes 4 digits at [rdi], advances rdi by 4
.write_4:
    push rbx
    mov rbx, 1000
    xor rdx, rdx
    div rbx                    ; rax=thousands, rdx=rem
    add al, '0'
    mov [rdi], al
    inc rdi

    mov rax, rdx
    mov rbx, 100
    xor rdx, rdx
    div rbx                    ; rax=hundreds, rdx=rem
    add al, '0'
    mov [rdi], al
    inc rdi

    mov rax, rdx
    mov rbx, 10
    xor rdx, rdx
    div rbx                    ; rax=tens, rdx=ones
    add al, '0'
    mov [rdi], al
    inc rdi

    mov al, dl
    add al, '0'
    mov [rdi], al
    inc rdi

    pop rbx
    ret
