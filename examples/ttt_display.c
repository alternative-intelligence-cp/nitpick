/**
 * ttt_display.c  —  C FFI shim for Aria Tic-Tac-Toe display
 *
 * Aria calls these functions via the extern block in ttt_ffi.aria.
 * The shim maintains board state across calls, writes move history
 * to /tmp/aria_ttt.dat, then launches ttt_display.py (curses TUI).
 *
 * Compile:
 *   gcc -shared -fPIC -o libttt_display.so ttt_display.c
 *
 * Link with ariac:
 *   ariac ttt_ffi.aria -L. -lttt_display -Wl,-rpath,$(pwd) -o ttt_ffi
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   /* readlink */

/* Board: positions 0-8, values 0=empty 1=X -1=O  */
static int64_t s_board[9];

/* Move history recorded in order */
static int64_t s_pos_history[20];
static int64_t s_val_history[20];
static int64_t s_move_count;

/* ── API ─────────────────────────────────────────────────────────────── */

/**
 * ttt_ffi_init — clear board and history.
 * Called once at program start.
 * Returns 0.
 */
int32_t ttt_ffi_init(void) {
    memset(s_board, 0, sizeof(s_board));
    memset(s_pos_history, 0, sizeof(s_pos_history));
    memset(s_val_history, 0, sizeof(s_val_history));
    s_move_count = 0;
    return 0;
}

/**
 * ttt_ffi_set — place a piece on the board and record the move.
 *   pos: 0-8
 *   val: 1 = X,  -1 = O
 * Returns 0 on success, -1 if pos out of range.
 */
int32_t ttt_ffi_set(int64_t pos, int64_t val) {
    if (pos < 0 || pos > 8 || s_move_count >= 20) return -1;
    s_board[pos] = val;
    s_pos_history[s_move_count] = pos;
    s_val_history[s_move_count] = val;
    s_move_count++;
    return 0;
}

/**
 * ttt_ffi_show — write history to /tmp/aria_ttt.dat then launch
 * ttt_display.py (curses TUI).  Blocks until the script exits.
 *
 * The script path is resolved from the running binary's directory
 * via /proc/self/exe so this works regardless of cwd.
 *
 * Returns the exit code of the Python process.
 */
int32_t ttt_ffi_show(void) {
    /* Write move history */
    FILE *f = fopen("/tmp/aria_ttt.dat", "w");
    if (!f) {
        perror("ttt_ffi_show: fopen");
        return -1;
    }
    for (int64_t i = 0; i < s_move_count; i++) {
        fprintf(f, "%ld %ld\n", (long)s_pos_history[i], (long)s_val_history[i]);
    }
    fclose(f);

    /* Resolve binary directory via /proc/self/exe */
    char exe_path[512] = {0};
    char script[600];
    ssize_t n = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (n > 0) {
        exe_path[n] = '\0';
        /* strip binary name, append script name */
        char *slash = strrchr(exe_path, '/');
        if (slash) *slash = '\0';
        snprintf(script, sizeof(script), "%s/ttt_display.py", exe_path);
    } else {
        /* fallback: current directory */
        snprintf(script, sizeof(script), "ttt_display.py");
    }

    char cmd[700];
    snprintf(cmd, sizeof(cmd), "python3 %s /tmp/aria_ttt.dat", script);
    return (int32_t)system(cmd);
}
