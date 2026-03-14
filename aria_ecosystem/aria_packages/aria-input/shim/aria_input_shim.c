/*
 * aria_input_shim.c — POSIX raw-stdin keyboard input for Aria virtual console
 *
 * All functions return int32_t:
 *   0   = success / "no key ready"
 *  -1   = error
 *   >0  = key / button value where documented
 *
 * Button bit-mask (same as SNES pad layout):
 *   bit 0  RIGHT   bit 1  LEFT    bit 2  DOWN    bit 3  UP
 *   bit 4  A       bit 5  B       bit 6  X       bit 7  Y
 *   bit 8  L       bit 9  R       bit 10 START   bit 11 SELECT
 *
 * Keyboard mapping (configurable via aria_input_map_key):
 *   Arrow keys  -> D-pad
 *   WASD        -> D-pad (default)
 *   JKL;        -> A B X Y (default)
 *   Q E         -> L R (default)
 *   Return      -> START
 *   Tab/Shift   -> SELECT (using ESC char in raw mode: Shift not reachable,
 *                   so we use '[' for SELECT)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>

/* ---- internal state ---- */

static int  g_input_raw = 0;
static struct termios g_saved_termios;

/* Key-to-button map: 256 entries, each entry is a bitmask of buttons */
static uint16_t g_keymap[256];

static void _default_keymap(void);

__attribute__((constructor))
static void aria_input_lib_init(void) {
    _default_keymap();
}

static void _default_keymap(void) {
    memset(g_keymap, 0, sizeof(g_keymap));
    /* WASD d-pad */
    g_keymap[(unsigned char)'w'] = (1 << 3); /* UP    */
    g_keymap[(unsigned char)'a'] = (1 << 1); /* LEFT  */
    g_keymap[(unsigned char)'s'] = (1 << 2); /* DOWN  */
    g_keymap[(unsigned char)'d'] = (1 << 0); /* RIGHT */
    /* Face buttons */
    g_keymap[(unsigned char)'j'] = (1 << 4); /* A     */
    g_keymap[(unsigned char)'k'] = (1 << 5); /* B     */
    g_keymap[(unsigned char)'l'] = (1 << 6); /* X     */
    g_keymap[(unsigned char)';'] = (1 << 7); /* Y     */
    /* Shoulder */
    g_keymap[(unsigned char)'q'] = (1 << 8); /* L     */
    g_keymap[(unsigned char)'e'] = (1 << 9); /* R     */
    /* Start / Select */
    g_keymap[(unsigned char)'\r'] = (1 << 10); /* START   */
    g_keymap[(unsigned char)'['] = (1 << 11); /* SELECT  */
}

/* ---- public API ---- */

/* Enter non-canonical (raw) input mode; disables echo */
int32_t aria_input_enter_raw(void) {
    if (g_input_raw) return 0;
    if (!isatty(STDIN_FILENO)) return -1;
    if (tcgetattr(STDIN_FILENO, &g_saved_termios) != 0) return -1;
    struct termios raw = g_saved_termios;
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;   /* non-blocking */
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return -1;
    g_input_raw = 1;
    return 0;
}

/* Exit raw input mode, restore saved settings */
int32_t aria_input_exit_raw(void) {
    if (!g_input_raw) return 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_saved_termios) != 0) return -1;
    g_input_raw = 0;
    return 0;
}

/* Returns 1 if currently in raw mode, 0 otherwise */
int32_t aria_input_is_raw(void) {
    return g_input_raw ? 1 : 0;
}

/*
 * Read a single raw byte from stdin (non-blocking).
 * Returns the byte value (0-255) if a key is available, -1 if no data or error.
 * NB: escape sequences (arrow keys) arrive as multiple bytes; the caller
 * should use aria_input_read_event which handles them.
 */
int32_t aria_input_read_raw_byte(void) {
    if (!g_input_raw) return -1;
    unsigned char ch;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n == 1) return (int32_t)ch;
    return -1;
}

/*
 * Read a full key event with escape-sequence handling.
 * Returns a virtual keycode (VK_* values below):
 *   0-127  = ASCII key value
 *   200    = UP arrow
 *   201    = DOWN arrow
 *   202    = LEFT arrow
 *   203    = RIGHT arrow
 *   204    = F1 .. 215 = F12
 *   -1     = no key available
 *   -2     = unrecognized escape sequence
 */
int32_t aria_input_read_key(void) {
    if (!g_input_raw) return -1;
    unsigned char buf[8];
    ssize_t n = read(STDIN_FILENO, buf, 1);
    if (n <= 0) return -1;

    if (buf[0] != 0x1B) {
        /* Regular ASCII */
        return (int32_t)(buf[0]);
    }
    /* ESC: try to read the rest of the sequence with a short timeout */
    /* Use poll to wait up to 10ms for more bytes */
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
    int pret = poll(&pfd, 1, 10);
    if (pret <= 0) {
        /* Bare ESC key */
        return 0x1B;
    }
    n = read(STDIN_FILENO, buf + 1, sizeof(buf) - 1);
    if (n <= 0) return 0x1B;
    /* CSI sequences: ESC [ ... */
    if (buf[1] == '[') {
        if (n >= 2) {
            switch (buf[2]) {
                case 'A': return 200; /* UP    */
                case 'B': return 201; /* DOWN  */
                case 'C': return 203; /* RIGHT */
                case 'D': return 202; /* LEFT  */
                default:  break;
            }
        }
    }
    return -2; /* unrecognized */
}

/*
 * Map a virtual keycode (from aria_input_read_key) to a button bitmask.
 * Arrow keycodes 200-203 are mapped to d-pad internally.
 * Regular ASCII uses g_keymap[].
 * Returns the button bitmask (0 if unmapped).
 */
int32_t aria_input_key_to_buttons(int32_t vk) {
    /* Arrow keys hardcoded */
    if (vk == 200) return (1 << 3); /* UP    */
    if (vk == 201) return (1 << 2); /* DOWN  */
    if (vk == 202) return (1 << 1); /* LEFT  */
    if (vk == 203) return (1 << 0); /* RIGHT */
    if (vk >= 0 && vk < 256) {
        return (int32_t)g_keymap[(unsigned char)vk];
    }
    return 0;
}

/*
 * Map a single ASCII character (0-255) to a button bit or 0 if unmapped.
 * Convenience wrapper for Aria code that already has raw bytes.
 */
int32_t aria_input_map_byte(int32_t byte_val) {
    if (byte_val < 0 || byte_val > 255) return 0;
    return (int32_t)g_keymap[(unsigned char)byte_val];
}

/*
 * Override the default mapping: map ascii_byte to button_mask.
 * Pass 0 as button_mask to clear the mapping for that byte.
 * Returns 0 on success, -1 if ascii_byte is out of range.
 */
int32_t aria_input_map_key(int32_t ascii_byte, int32_t button_mask) {
    if (ascii_byte < 0 || ascii_byte > 255) return -1;
    g_keymap[(unsigned char)ascii_byte] = (uint16_t)(button_mask & 0xFFF);
    return 0;
}

/*
 * Check if stdin has data available (non-blocking poll, 0ms timeout).
 * Returns 1 if data available, 0 if not, -1 on error.
 */
int32_t aria_input_poll(void) {
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
    int ret = poll(&pfd, 1, 0);
    if (ret < 0) return -1;
    return (ret > 0 && (pfd.revents & POLLIN)) ? 1 : 0;
}

/*
 * Flush (discard) any pending bytes in stdin.
 * Returns 0.
 */
int32_t aria_input_flush_pending(void) {
    tcflush(STDIN_FILENO, TCIFLUSH);
    return 0;
}
