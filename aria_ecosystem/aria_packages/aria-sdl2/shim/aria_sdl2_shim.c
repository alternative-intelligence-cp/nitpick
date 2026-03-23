/*  aria_sdl2_shim.c — flat-parameter bridge between Aria FFI and SDL2
 *
 *  SDL2 uses opaque pointers (SDL_Window*, SDL_Renderer*) and event structs
 *  that cannot be passed directly through Aria's extern FFI.  This shim
 *  stores the window/renderer internally and exposes a flat scalar API.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_sdl2_shim.so aria_sdl2_shim.c \
 *       $(pkg-config --cflags --libs sdl2)
 */

#include <stdint.h>
#include <SDL2/SDL.h>

/* ── internal state ──────────────────────────────────────────────────
 *  For simplicity the shim manages a single window + renderer pair.
 *  This is fine for the 99 % single-window use case.
 * ──────────────────────────────────────────────────────────────────── */

static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Event     g_event;

/* ── init / quit ─────────────────────────────────────────────────────── */

int32_t aria_sdl2_init(int32_t flags) {
    return SDL_Init((Uint32)flags);
}

void aria_sdl2_quit(void) {
    if (g_renderer) { SDL_DestroyRenderer(g_renderer); g_renderer = NULL; }
    if (g_window)   { SDL_DestroyWindow(g_window);     g_window   = NULL; }
    SDL_Quit();
}

/* ── window ──────────────────────────────────────────────────────────── */

int32_t aria_sdl2_create_window(const char *title,
                                int32_t x, int32_t y,
                                int32_t w, int32_t h,
                                int32_t flags) {
    g_window = SDL_CreateWindow(title, x, y, w, h, (Uint32)flags);
    return g_window ? 1 : 0;
}

void aria_sdl2_set_window_title(const char *title) {
    if (g_window) SDL_SetWindowTitle(g_window, title);
}

void aria_sdl2_set_window_size(int32_t w, int32_t h) {
    if (g_window) SDL_SetWindowSize(g_window, w, h);
}

void aria_sdl2_set_window_position(int32_t x, int32_t y) {
    if (g_window) SDL_SetWindowPosition(g_window, x, y);
}

int32_t aria_sdl2_get_window_width(void) {
    int w = 0;
    if (g_window) SDL_GetWindowSize(g_window, &w, NULL);
    return (int32_t)w;
}

int32_t aria_sdl2_get_window_height(void) {
    int h = 0;
    if (g_window) SDL_GetWindowSize(g_window, NULL, &h);
    return (int32_t)h;
}

void aria_sdl2_show_window(void) {
    if (g_window) SDL_ShowWindow(g_window);
}

void aria_sdl2_hide_window(void) {
    if (g_window) SDL_HideWindow(g_window);
}

void aria_sdl2_maximize_window(void) {
    if (g_window) SDL_MaximizeWindow(g_window);
}

void aria_sdl2_minimize_window(void) {
    if (g_window) SDL_MinimizeWindow(g_window);
}

/* ── renderer ────────────────────────────────────────────────────────── */

int32_t aria_sdl2_create_renderer(int32_t flags) {
    if (!g_window) return 0;
    g_renderer = SDL_CreateRenderer(g_window, -1, (Uint32)flags);
    return g_renderer ? 1 : 0;
}

void aria_sdl2_set_render_draw_color(int32_t r, int32_t g, int32_t b, int32_t a) {
    if (g_renderer) SDL_SetRenderDrawColor(g_renderer,
        (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
}

void aria_sdl2_render_clear(void) {
    if (g_renderer) SDL_RenderClear(g_renderer);
}

void aria_sdl2_render_present(void) {
    if (g_renderer) SDL_RenderPresent(g_renderer);
}

/* ── drawing ─────────────────────────────────────────────────────────── */

void aria_sdl2_render_draw_point(int32_t x, int32_t y) {
    if (g_renderer) SDL_RenderDrawPoint(g_renderer, x, y);
}

void aria_sdl2_render_draw_line(int32_t x1, int32_t y1,
                                int32_t x2, int32_t y2) {
    if (g_renderer) SDL_RenderDrawLine(g_renderer, x1, y1, x2, y2);
}

void aria_sdl2_render_draw_rect(int32_t x, int32_t y,
                                int32_t w, int32_t h) {
    SDL_Rect r = { x, y, w, h };
    if (g_renderer) SDL_RenderDrawRect(g_renderer, &r);
}

void aria_sdl2_render_fill_rect(int32_t x, int32_t y,
                                int32_t w, int32_t h) {
    SDL_Rect r = { x, y, w, h };
    if (g_renderer) SDL_RenderFillRect(g_renderer, &r);
}

/* ── events ──────────────────────────────────────────────────────────── */

int32_t aria_sdl2_poll_event(void) {
    return SDL_PollEvent(&g_event);
}

int32_t aria_sdl2_event_type(void) {
    return (int32_t)g_event.type;
}

/* keyboard event fields */
int32_t aria_sdl2_event_key_scancode(void) {
    return (int32_t)g_event.key.keysym.scancode;
}

int32_t aria_sdl2_event_key_keycode(void) {
    return (int32_t)g_event.key.keysym.sym;
}

int32_t aria_sdl2_event_key_mod(void) {
    return (int32_t)g_event.key.keysym.mod;
}

int32_t aria_sdl2_event_key_repeat(void) {
    return (int32_t)g_event.key.repeat;
}

/* mouse motion event fields */
int32_t aria_sdl2_event_mouse_x(void) {
    return (int32_t)g_event.motion.x;
}

int32_t aria_sdl2_event_mouse_y(void) {
    return (int32_t)g_event.motion.y;
}

int32_t aria_sdl2_event_mouse_xrel(void) {
    return (int32_t)g_event.motion.xrel;
}

int32_t aria_sdl2_event_mouse_yrel(void) {
    return (int32_t)g_event.motion.yrel;
}

/* mouse button event fields */
int32_t aria_sdl2_event_mouse_button(void) {
    return (int32_t)g_event.button.button;
}

int32_t aria_sdl2_event_mouse_clicks(void) {
    return (int32_t)g_event.button.clicks;
}

int32_t aria_sdl2_event_mouse_button_x(void) {
    return (int32_t)g_event.button.x;
}

int32_t aria_sdl2_event_mouse_button_y(void) {
    return (int32_t)g_event.button.y;
}

/* mouse wheel event fields */
int32_t aria_sdl2_event_wheel_x(void) {
    return (int32_t)g_event.wheel.x;
}

int32_t aria_sdl2_event_wheel_y(void) {
    return (int32_t)g_event.wheel.y;
}

/* window event fields */
int32_t aria_sdl2_event_window_event_id(void) {
    return (int32_t)g_event.window.event;
}

int32_t aria_sdl2_event_window_data1(void) {
    return (int32_t)g_event.window.data1;
}

int32_t aria_sdl2_event_window_data2(void) {
    return (int32_t)g_event.window.data2;
}

/* ── keyboard state ──────────────────────────────────────────────────── */

int32_t aria_sdl2_get_keyboard_state(int32_t scancode) {
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    if (!state) return 0;
    return (int32_t)state[scancode];
}

/* ── mouse state ─────────────────────────────────────────────────────── */

int32_t aria_sdl2_get_mouse_state_x(void) {
    int x = 0;
    SDL_GetMouseState(&x, NULL);
    return (int32_t)x;
}

int32_t aria_sdl2_get_mouse_state_y(void) {
    int y = 0;
    SDL_GetMouseState(NULL, &y);
    return (int32_t)y;
}

int32_t aria_sdl2_get_mouse_state_buttons(void) {
    return (int32_t)SDL_GetMouseState(NULL, NULL);
}

/* ── timing ──────────────────────────────────────────────────────────── */

int32_t aria_sdl2_get_ticks(void) {
    return (int32_t)SDL_GetTicks();
}

void aria_sdl2_delay(int32_t ms) {
    SDL_Delay((Uint32)ms);
}

/* ── misc ────────────────────────────────────────────────────────────── */

const char *aria_sdl2_get_error(void) {
    return SDL_GetError();
}

int32_t aria_sdl2_set_render_draw_blend_mode(int32_t mode) {
    if (!g_renderer) return -1;
    return SDL_SetRenderDrawBlendMode(g_renderer, (SDL_BlendMode)mode);
}
