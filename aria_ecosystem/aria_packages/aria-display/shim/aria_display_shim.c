/*
 * aria_display_shim.c  —  POSIX terminal display shim for aria-display.
 *
 * All functions return int32_t (0=success, -1=error) so the Aria
 * extern block uses int32 uniformly and callers use drop() to discard
 * results — matching the proven ttt_ffi.aria pattern.
 *
 * aria_display_get_size() returns int64_t with cols in high 32 bits
 * and rows in low 32 bits, avoiding pointer args across the FFI boundary.
 *
 * Future backends (X11, Win32, Cocoa) expose the same signatures.
 *
 * Build shared: cc -O2 -shared -fPIC -Wall -o libaria_display.so aria_display_shim.c
 * Build object: cc -O2 -c -Wall -o aria_display_shim.o aria_display_shim.c
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

static struct termios _orig_termios;
static int            _raw_mode_active = 0;

/* ---- raw mode ---- */
int32_t aria_display_enter_raw(void) {
    if (_raw_mode_active) return 0;
    if (tcgetattr(STDIN_FILENO, &_orig_termios) < 0) return -1;
    struct termios raw = _orig_termios;
    raw.c_lflag &= (unsigned)~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= (unsigned)~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= (unsigned)~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) return -1;
    _raw_mode_active = 1; return 0;
}
int32_t aria_display_exit_raw(void) {
    if (!_raw_mode_active) return 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &_orig_termios) < 0) return -1;
    _raw_mode_active = 0; return 0;
}
int32_t aria_display_is_raw(void) { return _raw_mode_active; }

/* ---- cursor ---- */
int32_t aria_display_move(int32_t col, int32_t row)
    { printf("\x1b[%d;%dH", row, col); return 0; }
int32_t aria_display_home(void)
    { printf("\x1b[H"); return 0; }
int32_t aria_display_hide_cursor(void)
    { printf("\x1b[?25l"); return 0; }
int32_t aria_display_show_cursor(void)
    { printf("\x1b[?25h"); return 0; }

/* ---- screen ---- */
int32_t aria_display_clear(void)
    { printf("\x1b[2J\x1b[H"); return 0; }
int32_t aria_display_clear_eol(void)
    { printf("\x1b[K"); return 0; }
int32_t aria_display_clear_eos(void)
    { printf("\x1b[J"); return 0; }

/* ---- color/attr ---- */
/* Colors: 0=black 1=red 2=green 3=yellow 4=blue 5=magenta 6=cyan 7=white
           8-15=bright variants  -1=default */
int32_t aria_display_set_fg(int32_t c) {
    if (c<0){printf("\x1b[39m");}
    else if(c<8){printf("\x1b[%dm",30+c);}
    else{printf("\x1b[%dm",90+(c-8));}
    return 0;
}
int32_t aria_display_set_bg(int32_t c) {
    if (c<0){printf("\x1b[49m");}
    else if(c<8){printf("\x1b[%dm",40+c);}
    else{printf("\x1b[%dm",100+(c-8));}
    return 0;
}
int32_t aria_display_reset_attr(void)
    { printf("\x1b[0m"); return 0; }
int32_t aria_display_set_bold(int32_t on)
    { printf(on ? "\x1b[1m" : "\x1b[22m"); return 0; }
int32_t aria_display_set_reverse(int32_t on)
    { printf(on ? "\x1b[7m" : "\x1b[27m"); return 0; }

/* ---- drawing ---- */
int32_t aria_display_hline(int32_t col,int32_t row,int32_t fg,int32_t bg,int32_t ch,int32_t w){
    aria_display_move(col,row);
    aria_display_set_fg(fg); aria_display_set_bg(bg);
    for(int32_t i=0;i<w;i++) putchar(ch);
    return 0;
}
int32_t aria_display_vline(int32_t col,int32_t row,int32_t fg,int32_t bg,int32_t ch,int32_t h){
    aria_display_set_fg(fg); aria_display_set_bg(bg);
    for(int32_t i=0;i<h;i++){ aria_display_move(col,row+i); putchar(ch); }
    return 0;
}
int32_t aria_display_box(int32_t col,int32_t row,int32_t fg,int32_t bg,int32_t w,int32_t h){
    aria_display_set_fg(fg); aria_display_set_bg(bg);
    aria_display_move(col,row); putchar('+');
    for(int32_t i=0;i<w-2;i++) putchar('-'); putchar('+');
    for(int32_t r=1;r<h-1;r++){
        aria_display_move(col,row+r); putchar('|');
        aria_display_move(col+w-1,row+r); putchar('|');
    }
    aria_display_move(col,row+h-1); putchar('+');
    for(int32_t i=0;i<w-2;i++) putchar('-'); putchar('+');
    return 0;
}

/* ---- size: returns cols<<32 | rows ---- */
int64_t aria_display_get_size(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0)
        return ((int64_t)80<<32)|(int64_t)24;
    return ((int64_t)(int32_t)ws.ws_col<<32)|(int64_t)(int32_t)ws.ws_row;
}

/* ---- flush ---- */
int32_t aria_display_flush(void) { fflush(stdout); return 0; }
