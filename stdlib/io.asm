; ============================================================
; Velocity Standard Library - I/O Module (io.asm)
; Kashmiri Edition v2 - Windows + Linux portable
;
; Compile with:
;   Linux:   nasm -f elf64
;   Windows: nasm -f win64
;
; Functions:
;   io__chhaap(fmt, ...)       - printf-like (%d %s %c %f %b %%)
;   io__chhaapLine()           - print newline
;   io__chhaapSpace()          - print space
;   io__stdin()                - read line as string
;   io__input()                - read integer from stdin (deprecated)
;   io__open(path, mode)       - open file, mode: 0=read 1=write 2=append
;   io__close(fd_or_handle)    - close file, returns 0 on success
;   io__fopen / io__fclose     - aliases of open/close
; ============================================================

%ifndef WIN64_TARGET
%ifidni __OUTPUT_FORMAT__,win64
  %define WIN64_TARGET 1
%endif
%endif

%ifdef WIN64_TARGET
  %define WINDOWS 1
%else
  %define WINDOWS 0
%endif

section .bss
    io_num_buf  resb 32
    io_inp_buf  resb 4096
    io_char_buf resb 1

section .data
    io_newline  db 10
    io_space    db 32
    io_minus    db '-'
    io_dot      db '.'
    io_zero     db '0'
    io_poz_str  db 'poz', 0
    io_apuz_str db 'apuz', 0
    io_fmt_d    db '%d', 0
    io_one_mil  dq 1000000.0

section .text
%ifdef WIN64_TARGET
extern GetStdHandle
extern WriteFile
extern ReadFile
extern ExitProcess
extern CreateFileA
extern CloseHandle

; helper: write rax=ptr, rcx=len to stdout
__io_write_win:
    ; rcx = ptr, rdx = len already expected
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    ; 32-byte shadow + 8-byte arg5 + 8-byte written + padding (16-byte aligned)
    sub  rsp, 56
    mov  r12, rcx        ; save ptr
    mov  r13, rdx        ; save len
    ; GetStdHandle(STD_OUTPUT_HANDLE = -11)
    mov  rcx, -11
    call GetStdHandle
    mov  rbx, rax        ; handle
    ; WriteFile(handle, ptr, len, &written, NULL)
    mov  qword [rsp + 40], 0
    mov  rcx, rbx
    mov  rdx, r12
    mov  r8,  r13
    lea  r9,  [rsp + 40]
    mov  qword [rsp + 32], 0   ; lpOverlapped = NULL (5th arg)
    call WriteFile
    add  rsp, 56
    pop  r13
    pop  r12
    pop  rbx
    mov  rsp, rbp
    pop  rbp
    ret

%else ; Linux
; helper: write rsi=ptr, rdx=len to stdout (fd=1)
__io_write_linux:
    push rbp
    mov  rbp, rsp
    mov  rax, 1
    mov  rdi, 1
    syscall
    pop  rbp
    ret
%endif

; -- io__chhaapLine -----------------------------------------
global io__chhaapLine
io__chhaapLine:
    push rbp
    mov  rbp, rsp
%ifdef WIN64_TARGET
    sub  rsp, 64
    mov  rcx, io_newline
    mov  rdx, 1
    call __io_write_win
%else
    lea  rsi, [rel io_newline]
    mov  rdx, 1
    call __io_write_linux
%endif
    xor  rax, rax
    mov  rsp, rbp
    pop  rbp
    ret

; -- io__chhaapSpace ----------------------------------------
global io__chhaapSpace
io__chhaapSpace:
    push rbp
    mov  rbp, rsp
%ifdef WIN64_TARGET
    sub  rsp, 64
    mov  rcx, io_space
    mov  rdx, 1
    call __io_write_win
%else
    lea  rsi, [rel io_space]
    mov  rdx, 1
    call __io_write_linux
%endif
    xor  rax, rax
    mov  rsp, rbp
    pop  rbp
    ret

; -- io__print_str_raw (internal: rdi/rcx=ptr) --------------
__io_print_str_raw:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    ; compute length
%ifdef WIN64_TARGET
    mov  r12, rcx
%else
    mov  r12, rdi
%endif
    xor  rbx, rbx
.len_loop:
    mov  al, [r12 + rbx]
    test al, al
    jz   .len_done
    inc  rbx
    jmp  .len_loop
.len_done:
    test rbx, rbx
    jz   .done
%ifdef WIN64_TARGET
    mov  rcx, r12
    mov  rdx, rbx
    call __io_write_win
%else
    mov  rsi, r12
    mov  rdx, rbx
    call __io_write_linux
%endif
.done:
    pop  r12
    pop  rbx
    mov  rsp, rbp
    pop  rbp
    ret

; -- io__print_int (internal: rdi/rcx = i64) ----------------
__io_print_int:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    sub  rsp, 8          ; keep call alignment + scratch slot
%ifdef WIN64_TARGET
    mov  r12, rcx
%else
    mov  r12, rdi
%endif
    lea  r13, [rel io_num_buf + 31]
    mov  byte [r13], 0
    test r12, r12
    jns  .positive
    ; print '-'
    mov  [rsp], r12
%ifdef WIN64_TARGET
    lea  rcx, [rel io_minus]
    mov  rdx, 1
    call __io_write_win
%else
    lea  rsi, [rel io_minus]
    mov  rdx, 1
    call __io_write_linux
%endif
    mov  r12, [rsp]
    neg  r12
.positive:
    test r12, r12
    jnz  .convert
    dec  r13
    mov  byte [r13], '0'
    jmp  .print
.convert:
    xor  rbx, rbx
.conv_loop:
    test r12, r12
    jz   .print
    mov  rax, r12
    xor  rdx, rdx
    mov  rcx, 10
    div  rcx
    mov  r12, rax
    dec  r13
    add  dl, '0'
    mov  [r13], dl
    inc  rbx
    jmp  .conv_loop
.print:
    ; count length
    lea  rbx, [rel io_num_buf + 31]
    sub  rbx, r13
%ifdef WIN64_TARGET
    mov  rcx, r13
    mov  rdx, rbx
    call __io_write_win
%else
    mov  rsi, r13
    mov  rdx, rbx
    call __io_write_linux
%endif
    add  rsp, 8
    pop  r13
    pop  r12
    pop  rbx
    mov  rsp, rbp
    pop  rbp
    ret

; -- io__print_float (internal: xmm0 = f64) -----------------
__io_print_float:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    sub  rsp, 40         ; keep call alignment + scratch space
    ; check negative
    xorpd xmm1, xmm1
    ucomisd xmm0, xmm1
    jae  .pos_f
    ; print minus
    movsd [rsp + 32], xmm0
%ifdef WIN64_TARGET
    lea  rcx, [rel io_minus]
    mov  rdx, 1
    call __io_write_win
%else
    lea  rsi, [rel io_minus]
    mov  rdx, 1
    call __io_write_linux
%endif
    movsd xmm0, [rsp + 32]
    ; negate
    xorpd xmm1, xmm1
    subsd xmm1, xmm0
    movapd xmm0, xmm1
.pos_f:
    ; extract integer part
    cvttsd2si rax, xmm0
    mov  r12, rax
    ; print integer part
%ifdef WIN64_TARGET
    mov  rcx, r12
%else
    mov  rdi, r12
%endif
    call __io_print_int
    ; print '.'
%ifdef WIN64_TARGET
    lea  rcx, [rel io_dot]
    mov  rdx, 1
    call __io_write_win
%else
    lea  rsi, [rel io_dot]
    mov  rdx, 1
    call __io_write_linux
%endif
    ; print 6 decimal digits
    cvtsi2sd xmm1, r12
    subsd xmm0, xmm1
    mulsd xmm0, [rel io_one_mil]
    cvttsd2si rax, xmm0
    test rax, rax
    jns  .frac_ok
    neg  rax
.frac_ok:
    ; pad to 6 digits
    lea  r13, [rel io_num_buf + 31]
    mov  byte [r13], 0
    mov  rcx, 6
.frac_loop:
    xor  rdx, rdx
    mov  rbx, 10
    div  rbx
    dec  r13
    add  dl, '0'
    mov  [r13], dl
    loop .frac_loop
    lea  rbx, [rel io_num_buf + 31]
    sub  rbx, r13
%ifdef WIN64_TARGET
    mov  rcx, r13
    mov  rdx, rbx
    call __io_write_win
%else
    mov  rsi, r13
    mov  rdx, rbx
    call __io_write_linux
%endif
    add  rsp, 40
    pop  r13
    pop  r12
    pop  rbx
    mov  rsp, rbp
    pop  rbp
    ret

; -- io__chhaap (fmt, ...) ----------------------------------
; Linux  SysV: rdi=fmt  rsi=arg1  rdx=arg2  rcx=arg3  r8=arg4  r9=arg5
; Windows MS:  rcx=fmt  rdx=arg1  r8=arg2   r9=arg3
global io__chhaap
io__chhaap:
    push rbp
    mov  rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15

    sub  rsp, 72        ; args spill area (+ alignment so call sites are ABI-correct)
%ifdef WIN64_TARGET
    mov  r12, rcx       ; fmt
    mov  [rsp + 0],  rdx
    mov  [rsp + 8],  r8
    mov  [rsp + 16], r9
    mov  qword [rsp + 24], 0
    mov  qword [rsp + 32], 0
%else
    mov  r12, rdi       ; fmt
    mov  [rsp + 0],  rsi
    mov  [rsp + 8],  rdx
    mov  [rsp + 16], rcx
    mov  [rsp + 24], r8
    mov  [rsp + 32], r9
%endif
    xor  r13, r13       ; arg index

.loop:
    mov  al, [r12]
    test al, al
    jz   .done
    inc  r12
    cmp  al, '%'
    jne  .print_char

    mov  al, [r12]
    test al, al
    jz   .done
    inc  r12

    cmp  al, '%'
    je   .print_pct
    cmp  al, 'd'
    je   .fmt_d
    cmp  al, 'i'
    je   .fmt_d
    cmp  al, 's'
    je   .fmt_s
    cmp  al, 'c'
    je   .fmt_c
    cmp  al, 'f'
    je   .fmt_f
    cmp  al, 'b'
    je   .fmt_b
    jmp  .loop

.print_pct:
    mov  al, '%'
    jmp  .write_char

.fmt_d:
    mov  rbx, [rsp + r13*8]
    inc  r13
%ifdef WIN64_TARGET
    mov  rcx, rbx
%else
    mov  rdi, rbx
%endif
    call __io_print_int
    jmp  .loop

.fmt_s:
    mov  rbx, [rsp + r13*8]
    inc  r13
    test rbx, rbx
    jz   .loop
%ifdef WIN64_TARGET
    mov  rcx, rbx
%else
    mov  rdi, rbx
%endif
    call __io_print_str_raw
    jmp  .loop

.fmt_c:
    mov  rbx, [rsp + r13*8]
    inc  r13
    mov  byte [rel io_char_buf], bl
%ifdef WIN64_TARGET
    lea  rcx, [rel io_char_buf]
    mov  rdx, 1
    call __io_write_win
%else
    lea  rsi, [rel io_char_buf]
    mov  rdx, 1
    call __io_write_linux
%endif
    jmp  .loop

.fmt_f:
    mov  rax, [rsp + r13*8]
    inc  r13
    movq xmm0, rax
    call __io_print_float
    jmp  .loop

.fmt_b:
    mov  rbx, [rsp + r13*8]
    inc  r13
    test rbx, rbx
    jz   .fmt_b_false
%ifdef WIN64_TARGET
    lea  rcx, [rel io_poz_str]
%else
    lea  rdi, [rel io_poz_str]
%endif
    call __io_print_str_raw
    jmp  .loop
.fmt_b_false:
%ifdef WIN64_TARGET
    lea  rcx, [rel io_apuz_str]
%else
    lea  rdi, [rel io_apuz_str]
%endif
    call __io_print_str_raw
    jmp  .loop

.print_char:
.write_char:
    mov  [rel io_char_buf], al
%ifdef WIN64_TARGET
    lea  rcx, [rel io_char_buf]
    mov  rdx, 1
    call __io_write_win
%else
    lea  rsi, [rel io_char_buf]
    mov  rdx, 1
    call __io_write_linux
%endif
    jmp  .loop

.done:
    xor  rax, rax
    add  rsp, 72
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    mov  rsp, rbp
    pop  rbp
    ret

; -- io__stdin (read line -> lafz pointer) -----------------
global io__stdin
io__stdin:
    push rbp
    mov  rbp, rsp
%ifdef WIN64_TARGET
    ; ReadFile(stdin_handle, buf, len-1, &read, NULL)
    sub  rsp, 64
    push rbx
    push r12
    mov  rcx, -10        ; STD_INPUT_HANDLE
    call GetStdHandle
    mov  rbx, rax
    lea  r12, [rel io_inp_buf]
    sub  rsp, 8
    mov  qword [rsp], 0
    mov  rcx, rbx
    mov  rdx, r12
    mov  r8,  4095
    lea  r9,  [rsp]
    push qword 0
    call ReadFile
    mov  rax, [rsp + 8]  ; bytes read
    add  rsp, 16
    ; null-terminate, strip \r\n
    lea  rbx, [rel io_inp_buf]
    mov  rcx, rax
    test rcx, rcx
    jz   .stdin_done_win
    dec  rcx
    mov  al, [rbx + rcx]
    cmp  al, 10
    je   .strip_lf_win
    inc  rcx
    jmp  .null_win
.strip_lf_win:
    test rcx, rcx
    jz   .null_win
    dec  rcx
    mov  al, [rbx + rcx]
    cmp  al, 13
    jne  .null_win_inc
    jmp  .null_win
.null_win_inc:
    inc  rcx
.null_win:
    mov  byte [rbx + rcx], 0
.stdin_done_win:
    lea  rax, [rel io_inp_buf]
    pop  r12
    pop  rbx
    add  rsp, 64
%else
    ; Linux: sys_read(0, buf, 4095)
    mov  rax, 0
    mov  rdi, 0
    lea  rsi, [rel io_inp_buf]
    mov  rdx, 4095
    syscall
    ; strip trailing \n
    test rax, rax
    jle  .stdin_empty
    dec  rax
    lea  rdx, [rel io_inp_buf]
    mov  byte [rdx + rax], 0
.stdin_empty:
    lea  rax, [rel io_inp_buf]
%endif
    mov  rsp, rbp
    pop  rbp
    ret

; -- io__input (deprecated: read integer) ------------------
global io__input
io__input:
    push rbp
    mov  rbp, rsp
    call io__stdin
    ; convert string to integer (simple atoi)
    mov  rbx, rax
    xor  rax, rax
    xor  rcx, rcx
    mov  cl, [rbx]
    test cl, cl
    jz   .inp_done
.inp_loop:
    mov  cl, [rbx]
    test cl, cl
    jz   .inp_done
    cmp  cl, '0'
    jl   .inp_done
    cmp  cl, '9'
    jg   .inp_done
    imul rax, rax, 10
    sub  cl, '0'
    add  rax, rcx
    inc  rbx
    jmp  .inp_loop
.inp_done:
    mov  rsp, rbp
    pop  rbp
    ret

; -- io__open(path: lafz, mode: adad) -> adad ---------------
; mode:
;   0 = read
;   1 = write (create/truncate)
;   2 = append (create if missing)
global io__open
io__open:
    push rbp
    mov  rbp, rsp
%ifdef WIN64_TARGET
    ; rcx = path, rdx = mode
    mov  r10, 0x80000000   ; GENERIC_READ
    mov  r11, 3            ; OPEN_EXISTING
    cmp  rdx, 1
    jne  .open_mode2_win
    mov  r10, 0x40000000   ; GENERIC_WRITE
    mov  r11, 2            ; CREATE_ALWAYS
    jmp  .open_cfg_done_win
.open_mode2_win:
    cmp  rdx, 2
    jne  .open_cfg_done_win
    mov  r10, 0x00000004   ; FILE_APPEND_DATA
    mov  r11, 4            ; OPEN_ALWAYS
.open_cfg_done_win:
    sub  rsp, 56
    mov  [rsp + 32], r11          ; dwCreationDisposition
    mov  qword [rsp + 40], 0x80   ; FILE_ATTRIBUTE_NORMAL
    mov  qword [rsp + 48], 0      ; hTemplateFile
    mov  rdx, r10                 ; dwDesiredAccess
    mov  r8,  3                   ; FILE_SHARE_READ | FILE_SHARE_WRITE
    xor  r9,  r9                  ; lpSecurityAttributes = NULL
    call CreateFileA
    add  rsp, 56
    cmp  rax, -1                  ; INVALID_HANDLE_VALUE
    jne  .open_done
    mov  rax, -1
%else
    ; rdi = path, rsi = mode
    ; Linux flags:
    ;   read   = 0
    ;   write  = O_WRONLY|O_CREAT|O_TRUNC  = 577
    ;   append = O_WRONLY|O_CREAT|O_APPEND = 1089
    cmp  rsi, 1
    jne  .open_mode2_linux
    mov  rsi, 577
    jmp  .open_linux_do
.open_mode2_linux:
    cmp  rsi, 2
    jne  .open_mode0_linux
    mov  rsi, 1089
    jmp  .open_linux_do
.open_mode0_linux:
    xor  rsi, rsi
.open_linux_do:
    mov  rax, 2            ; sys_open
    mov  rdx, 420          ; 0644
    syscall
%endif
.open_done:
    mov  rsp, rbp
    pop  rbp
    ret

; -- io__close(fd_or_handle: adad) -> adad ------------------
global io__close
io__close:
    push rbp
    mov  rbp, rsp
%ifdef WIN64_TARGET
    ; rcx = HANDLE
    sub  rsp, 32
    call CloseHandle
    add  rsp, 32
    test rax, rax
    jnz  .close_ok_win
    mov  rax, -1
    jmp  .close_done
.close_ok_win:
    xor  rax, rax
%else
    ; rdi = fd
    mov  rax, 3            ; sys_close
    syscall                ; returns 0 or -errno
%endif
.close_done:
    mov  rsp, rbp
    pop  rbp
    ret

global io__fopen
io__fopen:
    jmp  io__open

global io__fclose
io__fclose:
    jmp  io__close

; -- io__read(fd_or_handle: adad, buf: lafz, len: adad) -> adad
; Reads up to len bytes from fd into buf. Returns bytes read, or -1 on error.
global io__read
io__read:
    push rbp
    mov  rbp, rsp
%ifdef WIN64_TARGET
    ; rcx=HANDLE, rdx=buf, r8=len
    sub  rsp, 48
    ; ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, NULL)
    ; We'll use r9 as the &bytesRead slot on the stack
    lea  r9, [rbp - 16]         ; lpNumberOfBytesRead
    mov  qword [rbp - 16], 0
    xor  rax, rax
    push rax                    ; NULL for lpOverlapped (5th arg via stack)
    call ReadFile
    add  rsp, 8
    add  rsp, 48
    test rax, rax
    jz   .read_err_win
    mov  rax, [rbp - 16]        ; bytes actually read
    jmp  .read_done
.read_err_win:
    mov  rax, -1
%else
    ; rdi=fd, rsi=buf, rdx=len
    mov  rax, 0                 ; sys_read
    syscall
%endif
.read_done:
    mov  rsp, rbp
    pop  rbp
    ret

; -- io__write(fd_or_handle: adad, buf: lafz, len: adad) -> adad
; Writes len bytes from buf to fd. Returns bytes written, or -1 on error.
global io__write
io__write:
    push rbp
    mov  rbp, rsp
%ifdef WIN64_TARGET
    ; rcx=HANDLE, rdx=buf, r8=len
    sub  rsp, 48
    lea  r9, [rbp - 16]
    mov  qword [rbp - 16], 0
    xor  rax, rax
    push rax
    call WriteFile
    add  rsp, 8
    add  rsp, 48
    test rax, rax
    jz   .write_err_win
    mov  rax, [rbp - 16]
    jmp  .write_done
.write_err_win:
    mov  rax, -1
%else
    ; rdi=fd, rsi=buf, rdx=len
    mov  rax, 1                 ; sys_write
    syscall
%endif
.write_done:
    mov  rsp, rbp
    pop  rbp
    ret

