/*  aria_raylib_shim.c — flat-parameter bridge between Aria FFI and raylib
 *
 *  Raylib uses small C structs (Color, Vector2, Rectangle) that cannot be
 *  passed directly through Aria's extern FFI.  This shim decomposes every
 *  struct parameter into scalar arguments and reconstructs them before
 *  calling the real raylib function.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_raylib_shim.so aria_raylib_shim.c \
 *       $(pkg-config --cflags --libs raylib)
 */

#include <stdint.h>
#include "raylib.h"

/* ── helpers ─────────────────────────────────────────────────────────── */

static inline Color mk_color(int32_t r, int32_t g, int32_t b, int32_t a) {
    return (Color){ (unsigned char)r, (unsigned char)g,
                    (unsigned char)b, (unsigned char)a };
}

/* ── window ──────────────────────────────────────────────────────────── */

void aria_rl_init_window(int32_t w, int32_t h, const char *title) {
    InitWindow(w, h, title);
}

void aria_rl_close_window(void) {
    CloseWindow();
}

int32_t aria_rl_window_should_close(void) {
    return WindowShouldClose() ? 1 : 0;
}

int32_t aria_rl_is_window_ready(void) {
    return IsWindowReady() ? 1 : 0;
}

int32_t aria_rl_is_window_fullscreen(void) {
    return IsWindowFullscreen() ? 1 : 0;
}

void aria_rl_toggle_fullscreen(void) {
    ToggleFullscreen();
}

void aria_rl_set_window_size(int32_t w, int32_t h) {
    SetWindowSize(w, h);
}

void aria_rl_set_window_title(const char *title) {
    SetWindowTitle(title);
}

void aria_rl_set_window_position(int32_t x, int32_t y) {
    SetWindowPosition(x, y);
}

int32_t aria_rl_get_screen_width(void) {
    return GetScreenWidth();
}

int32_t aria_rl_get_screen_height(void) {
    return GetScreenHeight();
}

/* ── drawing lifecycle ───────────────────────────────────────────────── */

void aria_rl_begin_drawing(void) {
    BeginDrawing();
}

void aria_rl_end_drawing(void) {
    EndDrawing();
}

void aria_rl_clear_background(int32_t r, int32_t g, int32_t b, int32_t a) {
    ClearBackground(mk_color(r, g, b, a));
}

/* ── timing ──────────────────────────────────────────────────────────── */

void aria_rl_set_target_fps(int32_t fps) {
    SetTargetFPS(fps);
}

int32_t aria_rl_get_fps(void) {
    return GetFPS();
}

float aria_rl_get_frame_time(void) {
    return GetFrameTime();
}

double aria_rl_get_time(void) {
    return GetTime();
}

/* ── shapes: pixel ───────────────────────────────────────────────────── */

void aria_rl_draw_pixel(int32_t x, int32_t y,
                        int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawPixel(x, y, mk_color(r, g, b, a));
}

/* ── shapes: line ────────────────────────────────────────────────────── */

void aria_rl_draw_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                       int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawLine(x1, y1, x2, y2, mk_color(r, g, b, a));
}

/* ── shapes: circle ──────────────────────────────────────────────────── */

void aria_rl_draw_circle(int32_t cx, int32_t cy, float radius,
                         int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawCircle(cx, cy, radius, mk_color(r, g, b, a));
}

void aria_rl_draw_circle_lines(int32_t cx, int32_t cy, float radius,
                               int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawCircleLines(cx, cy, radius, mk_color(r, g, b, a));
}

void aria_rl_draw_circle_gradient(int32_t cx, int32_t cy, float radius,
                                  int32_t ir, int32_t ig, int32_t ib, int32_t ia,
                                  int32_t or_, int32_t og, int32_t ob, int32_t oa) {
    DrawCircleGradient(cx, cy, radius,
                       mk_color(ir, ig, ib, ia),
                       mk_color(or_, og, ob, oa));
}

/* ── shapes: rectangle ───────────────────────────────────────────────── */

void aria_rl_draw_rectangle(int32_t x, int32_t y, int32_t w, int32_t h,
                            int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawRectangle(x, y, w, h, mk_color(r, g, b, a));
}

void aria_rl_draw_rectangle_lines(int32_t x, int32_t y, int32_t w, int32_t h,
                                  int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawRectangleLines(x, y, w, h, mk_color(r, g, b, a));
}

void aria_rl_draw_rectangle_gradient_v(int32_t x, int32_t y, int32_t w, int32_t h,
                                       int32_t tr, int32_t tg, int32_t tb, int32_t ta,
                                       int32_t br, int32_t bg, int32_t bb, int32_t ba) {
    DrawRectangleGradientV(x, y, w, h,
                           mk_color(tr, tg, tb, ta),
                           mk_color(br, bg, bb, ba));
}

/* ── shapes: triangle ────────────────────────────────────────────────── */

void aria_rl_draw_triangle(float x1, float y1, float x2, float y2,
                           float x3, float y3,
                           int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawTriangle((Vector2){x1,y1}, (Vector2){x2,y2}, (Vector2){x3,y3},
                 mk_color(r, g, b, a));
}

void aria_rl_draw_triangle_lines(float x1, float y1, float x2, float y2,
                                 float x3, float y3,
                                 int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawTriangleLines((Vector2){x1,y1}, (Vector2){x2,y2}, (Vector2){x3,y3},
                      mk_color(r, g, b, a));
}

/* ── text ────────────────────────────────────────────────────────────── */

void aria_rl_draw_text(const char *text, int32_t x, int32_t y,
                       int32_t font_size,
                       int32_t r, int32_t g, int32_t b, int32_t a) {
    DrawText(text, x, y, font_size, mk_color(r, g, b, a));
}

void aria_rl_draw_fps(int32_t x, int32_t y) {
    DrawFPS(x, y);
}

int32_t aria_rl_measure_text(const char *text, int32_t font_size) {
    return MeasureText(text, font_size);
}

/* ── keyboard input ──────────────────────────────────────────────────── */

int32_t aria_rl_is_key_pressed(int32_t key) {
    return IsKeyPressed(key) ? 1 : 0;
}

int32_t aria_rl_is_key_down(int32_t key) {
    return IsKeyDown(key) ? 1 : 0;
}

int32_t aria_rl_is_key_released(int32_t key) {
    return IsKeyReleased(key) ? 1 : 0;
}

int32_t aria_rl_is_key_up(int32_t key) {
    return IsKeyUp(key) ? 1 : 0;
}

int32_t aria_rl_get_key_pressed(void) {
    return GetKeyPressed();
}

/* ── mouse input ─────────────────────────────────────────────────────── */

int32_t aria_rl_get_mouse_x(void) {
    return GetMouseX();
}

int32_t aria_rl_get_mouse_y(void) {
    return GetMouseY();
}

int32_t aria_rl_is_mouse_button_pressed(int32_t button) {
    return IsMouseButtonPressed(button) ? 1 : 0;
}

int32_t aria_rl_is_mouse_button_down(int32_t button) {
    return IsMouseButtonDown(button) ? 1 : 0;
}

int32_t aria_rl_is_mouse_button_released(int32_t button) {
    return IsMouseButtonReleased(button) ? 1 : 0;
}

float aria_rl_get_mouse_wheel_move(void) {
    return GetMouseWheelMove();
}

void aria_rl_set_mouse_position(int32_t x, int32_t y) {
    SetMousePosition(x, y);
}

void aria_rl_set_mouse_cursor(int32_t cursor) {
    SetMouseCursor(cursor);
}
