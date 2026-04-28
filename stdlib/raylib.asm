; ===============================
; raylib.asm (Velocity stdlib)
; Clean, ABI-safe wrappers
; ===============================

default rel

extern InitWindow
extern WindowShouldClose
extern BeginDrawing
extern EndDrawing
extern CloseWindow
extern ClearBackground
extern SetTargetFPS
extern DrawText
extern DrawRectangle

section .text

; ---- Window ----

global rl_init_window
rl_init_window:
    sub rsp, 8
    call InitWindow
    add rsp, 8
    ret

global rl_window_should_close
rl_window_should_close:
    sub rsp, 8
    call WindowShouldClose
    add rsp, 8
    ret

global rl_close_window
rl_close_window:
    sub rsp, 8
    call CloseWindow
    add rsp, 8
    ret

global rl_set_target_fps
rl_set_target_fps:
    sub rsp, 8
    call SetTargetFPS
    add rsp, 8
    ret

; ---- Drawing ----

global rl_begin_drawing
rl_begin_drawing:
    sub rsp, 8
    call BeginDrawing
    add rsp, 8
    ret

global rl_end_drawing
rl_end_drawing:
    sub rsp, 8
    call EndDrawing
    add rsp, 8
    ret

global rl_clear_background
rl_clear_background:
    sub rsp, 8
    call ClearBackground
    add rsp, 8
    ret

; ---- Shapes / Text ----

global rl_draw_text
rl_draw_text:
    sub rsp, 8
    call DrawText
    add rsp, 8
    ret

global rl_draw_rectangle
rl_draw_rectangle:
    sub rsp, 8
    call DrawRectangle
    add rsp, 8
    ret

; ---- Stack safety marker ----
section .note.GNU-stack noalloc noexec nowrite progbits
