/**
 * libalife.c  —  ANSI display shim for alife.aria (Conway's Game of Life)
 *
 * Provides five functions called from Aria via the extern FFI block:
 *   alife_init()                  — hide cursor, clear screen, register cleanup
 *   alife_put(row, col, alive)    — update shadow grid (called per cell)
 *   alife_show(gen)               — render full frame with ANSI color
 *   alife_sleep(ms)               — nanosleep wrapper
 *   alife_done()                  — restore cursor, final newline
 *
 * Build:
 *   gcc -shared -fPIC -o libalife.so libalife.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>

#define ROWS 16
#define COLS 32

static int shadow[ROWS][COLS];
static int started = 0;

/* ── cleanup (called from atexit and signal handler) ─────────────────────── */
static void restore_terminal(void) {
    printf("\033[?25h\033[0m\n");
    fflush(stdout);
}

static void on_signal(int sig) {
    (void)sig;
    restore_terminal();
    _exit(0);
}

/* ── public API ──────────────────────────────────────────────────────────── */

int alife_init(void) {
    memset(shadow, 0, sizeof(shadow));
    started = 1;
    atexit(restore_terminal);
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);
    printf("\033[?25l");   /* hide cursor */
    printf("\033[2J");     /* clear screen */
    fflush(stdout);
    return 0;
}

int alife_put(int64_t row, int64_t col, int64_t alive) {
    if (row >= 0 && row < ROWS && col >= 0 && col < COLS)
        shadow[(int)row][(int)col] = (int)alive;
    return 0;
}

int alife_show(int64_t gen) {
    /* Top-left border: 32 cols × 2 chars wide + 2 vertical bars = 66 chars */
    static const char *BAR =
        "------------------------------------------------------------------";

    printf("\033[H");   /* home cursor */

    /* Header */
    printf("\033[1;36m  Conway's Game of Life\033[0m"
           "  \033[0;33m(Aria + C FFI + \033[1;33m-Wl,-rpath,$ORIGIN\033[0;33m)\033[0m"
           "   \033[1;32mgen = %-5lld\033[0m\n\n", (long long)gen);

    /* Top border */
    printf("  \033[1;34m+%s+\033[0m\n", BAR);

    for (int r = 0; r < ROWS; r++) {
        printf("  \033[1;34m|\033[0m");
        for (int c = 0; c < COLS; c++) {
            if (shadow[r][c])
                printf("\033[1;92m##\033[0m");   /* bright green block */
            else
                printf("\033[0;90m..\033[0m");   /* dark grey dots */
        }
        printf("\033[1;34m|\033[0m\n");
    }

    /* Bottom border */
    printf("  \033[1;34m+%s+\033[0m\n", BAR);
    printf("\n  \033[0;37mCtrl-C to exit\033[0m\n");

    fflush(stdout);
    return 0;
}

int alife_sleep(int64_t ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
    return 0;
}

int alife_done(void) {
    if (started) {
        printf("\033[?25h\033[0m\n");
        fflush(stdout);
        started = 0;
    }
    return 0;
}
