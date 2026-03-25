/**
 * Hexstream shim — Aria bindings for AriaX six-stream I/O
 *
 * FD 3 = stddbg  (debug output)
 * FD 4 = stddati (data input)
 * FD 5 = stddato (data output)
 *
 * Provides read/write operations for hexstream FDs,
 * plus setup helpers for pipes and redirection.
 *
 * Works on any POSIX system — FDs 3-5 are just conventions.
 * On AriaX with kernel patches, FDs are auto-opened and
 * preserved across exec().
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STDDBG  3
#define STDDATI 4
#define STDDATO 5

typedef struct { char* data; int64_t length; } AriaString;

static AriaString hs_make_string(const char* s) {
    AriaString r;
    r.length = (int64_t)strlen(s);
    r.data = (char*)malloc((size_t)(r.length + 1));
    memcpy(r.data, s, (size_t)(r.length + 1));
    return r;
}

// ============================================================================
// Setup / Initialization
// ============================================================================

// Ensure FDs 3-5 are open. Opens to /dev/null if not.
// Returns 0 on success, -1 on error.
int32_t aria_shim_hexstream_init(void) {
    for (int fd = STDDBG; fd <= STDDATO; fd++) {
        if (fcntl(fd, F_GETFD) == -1) {
            // FD not open — open /dev/null
            int newfd = open("/dev/null", O_RDWR);
            if (newfd < 0) return -1;
            if (newfd != fd) {
                if (dup2(newfd, fd) < 0) {
                    close(newfd);
                    return -1;
                }
                close(newfd);
            }
        }
    }
    return 0;
}

// Redirect a hexstream FD to a file
int32_t aria_shim_hexstream_redirect_to_file(int32_t fd, const char* path) {
    if (fd < STDDBG || fd > STDDATO) return -1;
    int newfd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (newfd < 0) return -1;
    if (dup2(newfd, fd) < 0) {
        close(newfd);
        return -1;
    }
    close(newfd);
    return 0;
}

// Redirect a hexstream FD to read from a file
int32_t aria_shim_hexstream_redirect_from_file(int32_t fd, const char* path) {
    if (fd < STDDBG || fd > STDDATO) return -1;
    int newfd = open(path, O_RDONLY);
    if (newfd < 0) return -1;
    if (dup2(newfd, fd) < 0) {
        close(newfd);
        return -1;
    }
    close(newfd);
    return 0;
}

// ============================================================================
// Write operations
// ============================================================================

// Write a string to a hexstream FD
int32_t aria_shim_hexstream_write(int32_t fd, const char* data) {
    if (fd < STDDBG || fd > STDDATO) return -1;
    size_t len = strlen(data);
    ssize_t n = write(fd, data, len);
    return (n == (ssize_t)len) ? 0 : -1;
}

// Write a string + newline to stddbg (FD 3)
int32_t aria_shim_hexstream_debug(const char* msg) {
    size_t len = strlen(msg);
    write(STDDBG, msg, len);
    write(STDDBG, "\n", 1);
    return 0;
}

// Write an int64 as 8 bytes to a data stream
int32_t aria_shim_hexstream_write_int64(int32_t fd, int64_t value) {
    if (fd < STDDBG || fd > STDDATO) return -1;
    ssize_t n = write(fd, &value, sizeof(int64_t));
    return (n == sizeof(int64_t)) ? 0 : -1;
}

// ============================================================================
// Read operations
// ============================================================================

// Read a line from a hexstream FD (up to limit bytes)
AriaString aria_shim_hexstream_read_line(int32_t fd) {
    if (fd < STDDBG || fd > STDDATO) return hs_make_string("");
    char buf[4096];
    int pos = 0;
    while (pos < 4095) {
        ssize_t n = read(fd, &buf[pos], 1);
        if (n <= 0) break;
        if (buf[pos] == '\n') break;
        pos++;
    }
    buf[pos] = '\0';
    return hs_make_string(buf);
}

// Read 8 bytes as int64 from a data stream
int64_t aria_shim_hexstream_read_int64(int32_t fd) {
    if (fd < STDDBG || fd > STDDATO) return -1;
    int64_t value;
    ssize_t n = read(fd, &value, sizeof(int64_t));
    return (n == sizeof(int64_t)) ? value : -1;
}

// ============================================================================
// Query
// ============================================================================

// Check if a hexstream FD is open
int32_t aria_shim_hexstream_is_open(int32_t fd) {
    if (fd < STDDBG || fd > STDDATO) return 0;
    return (fcntl(fd, F_GETFD) != -1) ? 1 : 0;
}

// Get the FD number for a named stream
int32_t aria_shim_hexstream_fd(const char* name) {
    if (strcmp(name, "stddbg") == 0)  return STDDBG;
    if (strcmp(name, "stddati") == 0) return STDDATI;
    if (strcmp(name, "stddato") == 0) return STDDATO;
    return -1;
}

#ifdef __cplusplus
}
#endif
