; Velocity SDL Wrapper (native module)
; Minimal thin wrappers around SDL2 so Velocity programs can drive a window.

section .bss
    sdl_event resb 64

section .text
    extern SDL_Init
    extern SDL_Quit
    extern SDL_CreateWindow
    extern SDL_DestroyWindow
    extern SDL_CreateRenderer
    extern SDL_DestroyRenderer
    extern SDL_SetRenderDrawColor
    extern SDL_RenderClear
    extern SDL_RenderPresent
    extern SDL_PollEvent
    extern SDL_Delay

global sdl_native__init
sdl_native__init:
    push rbp
    mov rbp, rsp
    call SDL_Init
    pop rbp
    ret

global sdl_native__quit
sdl_native__quit:
    push rbp
    mov rbp, rsp
    call SDL_Quit
    pop rbp
    ret

global sdl_native__create_window
sdl_native__create_window:
    push rbp
    mov rbp, rsp
    call SDL_CreateWindow
    pop rbp
    ret

global sdl_native__destroy_window
sdl_native__destroy_window:
    push rbp
    mov rbp, rsp
    call SDL_DestroyWindow
    pop rbp
    ret

global sdl_native__create_renderer
sdl_native__create_renderer:
    push rbp
    mov rbp, rsp
    call SDL_CreateRenderer
    pop rbp
    ret

global sdl_native__destroy_renderer
sdl_native__destroy_renderer:
    push rbp
    mov rbp, rsp
    call SDL_DestroyRenderer
    pop rbp
    ret

global sdl_native__set_render_draw_color
sdl_native__set_render_draw_color:
    push rbp
    mov rbp, rsp
    call SDL_SetRenderDrawColor
    pop rbp
    ret

global sdl_native__render_clear
sdl_native__render_clear:
    push rbp
    mov rbp, rsp
    call SDL_RenderClear
    pop rbp
    ret

global sdl_native__render_present
sdl_native__render_present:
    push rbp
    mov rbp, rsp
    call SDL_RenderPresent
    pop rbp
    ret

global sdl_native__delay
sdl_native__delay:
    push rbp
    mov rbp, rsp
    call SDL_Delay
    pop rbp
    ret

global sdl_native__poll_event
sdl_native__poll_event:
    push rbp
    mov rbp, rsp
    lea rdi, [rel sdl_event]
    call SDL_PollEvent
    test rax, rax
    jz .no_event
    lea rax, [rel sdl_event]
    jmp .end
.no_event:
    xor rax, rax
.end:
    pop rbp
    ret

global sdl_native__event_type
sdl_native__event_type:
    push rbp
    mov rbp, rsp
    mov eax, [rel sdl_event]
    pop rbp
    ret

global sdl_native__event_key_sym
sdl_native__event_key_sym:
    push rbp
    mov rbp, rsp
    mov eax, [rel sdl_event + 20]
    pop rbp
    ret
