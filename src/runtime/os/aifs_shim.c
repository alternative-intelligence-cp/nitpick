/**
 * AriaFS shim — Aria bindings for AI filesystem metadata operations
 *
 * Provides handle-based API for:
 * - Setting/getting tensor metadata (dtype, shape) via xattrs
 * - Checkpoint marking
 * - File I/O helpers for large sequential reads/writes
 *
 * The actual filesystem is accessed via the mount point (standard POSIX I/O).
 * This shim provides metadata operations that use Linux extended attributes.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

// AriaString ABI (for returning strings)
typedef struct { char* data; int64_t length; } AriaString;

static AriaString make_string(const char* s) {
    AriaString r;
    r.length = (int64_t)strlen(s);
    r.data = (char*)malloc((size_t)(r.length + 1));
    memcpy(r.data, s, (size_t)(r.length + 1));
    return r;
}

// ============================================================================
// Tensor metadata operations (via xattrs)
// ============================================================================

// Set tensor dtype on a file: "float32", "float16", "bfloat16", "int8", etc.
int32_t aria_shim_aifs_set_dtype(const char* path, const char* dtype) {
    if (setxattr(path, "user.tensor.dtype", dtype, strlen(dtype), 0) != 0)
        return -1;
    return 0;
}

// Get tensor dtype from a file
AriaString aria_shim_aifs_get_dtype(const char* path) {
    char buf[64] = {0};
    ssize_t len = getxattr(path, "user.tensor.dtype", buf, sizeof(buf) - 1);
    if (len <= 0) return make_string("");
    buf[len] = '\0';
    return make_string(buf);
}

// Set tensor shape as comma-separated: "3,224,224"
int32_t aria_shim_aifs_set_shape(const char* path, const char* shape) {
    if (setxattr(path, "user.tensor.shape", shape, strlen(shape), 0) != 0)
        return -1;
    return 0;
}

// Get tensor shape
AriaString aria_shim_aifs_get_shape(const char* path) {
    char buf[256] = {0};
    ssize_t len = getxattr(path, "user.tensor.shape", buf, sizeof(buf) - 1);
    if (len <= 0) return make_string("");
    buf[len] = '\0';
    return make_string(buf);
}

// Mark/unmark as checkpoint
int32_t aria_shim_aifs_set_checkpoint(const char* path, int32_t is_ckpt) {
    const char* val = is_ckpt ? "1" : "0";
    if (setxattr(path, "user.checkpoint", val, 1, 0) != 0)
        return -1;
    return 0;
}

// Check if file is a checkpoint
int32_t aria_shim_aifs_is_checkpoint(const char* path) {
    char buf[4] = {0};
    ssize_t len = getxattr(path, "user.checkpoint", buf, sizeof(buf) - 1);
    if (len <= 0) return 0;
    return (buf[0] == '1') ? 1 : 0;
}

// ============================================================================
// Streaming I/O helpers for large files
// ============================================================================

// Open a file for reading, returns fd (handle)
int64_t aria_shim_aifs_open_read(const char* path) {
    int fd = open(path, O_RDONLY);
    return (int64_t)fd;
}

// Open a file for writing (create/truncate), returns fd
int64_t aria_shim_aifs_open_write(const char* path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    return (int64_t)fd;
}

// Open for append
int64_t aria_shim_aifs_open_append(const char* path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    return (int64_t)fd;
}

// Close fd
int32_t aria_shim_aifs_close(int64_t fd) {
    if (fd < 0) return -1;
    return close((int)fd) == 0 ? 0 : -1;
}

// Read up to nbytes, returns number of bytes read
int64_t aria_shim_aifs_read_bytes(int64_t fd, int64_t nbytes) {
    // This is a "skip/seek" style read — just advances position
    // For Aria, actual data reads use the file I/O stdlib
    if (fd < 0 || nbytes <= 0) return -1;
    char* buf = (char*)malloc((size_t)nbytes);
    if (!buf) return -1;
    ssize_t n = read((int)fd, buf, (size_t)nbytes);
    free(buf);
    return (int64_t)n;
}

// Get file size
int64_t aria_shim_aifs_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_size;
}

#ifdef __cplusplus
}
#endif
