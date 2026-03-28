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
#include <dirent.h>

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 1024
#endif

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

// ============================================================================
// Model format detection (magic bytes)
// ============================================================================

// Detect AI model file format from magic bytes / extension
// Returns: "onnx", "safetensors", "gguf", "pytorch", "hdf5",
//          "numpy", "tflite", "tensorrt", "unknown"
AriaString aria_shim_aifs_detect_format(const char* path) {
    unsigned char magic[16] = {0};
    int fd = open(path, O_RDONLY);
    if (fd < 0) return make_string("unknown");
    ssize_t n = read(fd, magic, 16);
    close(fd);
    if (n < 4) return make_string("unknown");

    // ONNX: protobuf with field tag 0x08 (ir_version) early on
    // Actually ONNX files start with protobuf — check for \x08 followed by small int
    // More reliable: check extension + protobuf header
    // GGUF: "GGUF" magic at offset 0
    if (n >= 4 && magic[0] == 'G' && magic[1] == 'G' && magic[2] == 'U' && magic[3] == 'F')
        return make_string("gguf");

    // HDF5: \x89HDF\r\n\x1a\n
    if (n >= 8 && magic[0] == 0x89 && magic[1] == 'H' && magic[2] == 'D' && magic[3] == 'F'
        && magic[4] == '\r' && magic[5] == '\n' && magic[6] == 0x1a && magic[7] == '\n')
        return make_string("hdf5");

    // NumPy: \x93NUMPY
    if (n >= 6 && magic[0] == 0x93 && magic[1] == 'N' && magic[2] == 'U'
        && magic[3] == 'M' && magic[4] == 'P' && magic[5] == 'Y')
        return make_string("numpy");

    // PyTorch (zip-based): PK\x03\x04 (zip magic)
    if (n >= 4 && magic[0] == 'P' && magic[1] == 'K' && magic[2] == 0x03 && magic[3] == 0x04) {
        // Could be safetensors (also starts with '{') or pytorch (zip)
        return make_string("pytorch");
    }

    // SafeTensors: starts with '{' (JSON header with length prefix)
    // The first 8 bytes are a little-endian uint64 header size, then '{'
    if (n >= 9 && magic[8] == '{') {
        return make_string("safetensors");
    }

    // TFLite: offset 4-7 = "TFL3"
    if (n >= 8 && magic[4] == 'T' && magic[5] == 'F' && magic[6] == 'L' && magic[7] == '3')
        return make_string("tflite");

    // TensorRT: serialized engine — no standard magic, fall through

    // Fallback: check extension
    const char* dot = strrchr(path, '.');
    if (dot) {
        if (strcmp(dot, ".onnx") == 0) return make_string("onnx");
        if (strcmp(dot, ".safetensors") == 0) return make_string("safetensors");
        if (strcmp(dot, ".gguf") == 0) return make_string("gguf");
        if (strcmp(dot, ".pt") == 0 || strcmp(dot, ".pth") == 0) return make_string("pytorch");
        if (strcmp(dot, ".h5") == 0 || strcmp(dot, ".hdf5") == 0) return make_string("hdf5");
        if (strcmp(dot, ".npy") == 0 || strcmp(dot, ".npz") == 0) return make_string("numpy");
        if (strcmp(dot, ".tflite") == 0) return make_string("tflite");
        if (strcmp(dot, ".engine") == 0 || strcmp(dot, ".trt") == 0) return make_string("tensorrt");
    }

    return make_string("unknown");
}

// Store detected format as xattr
int32_t aria_shim_aifs_set_format(const char* path, const char* format) {
    if (setxattr(path, "user.model.format", format, strlen(format), 0) != 0)
        return -1;
    return 0;
}

// Get stored format
AriaString aria_shim_aifs_get_format(const char* path) {
    char buf[64] = {0};
    ssize_t len = getxattr(path, "user.model.format", buf, sizeof(buf) - 1);
    if (len <= 0) return make_string("");
    buf[len] = '\0';
    return make_string(buf);
}

// ============================================================================
// Checksum (SHA-256 via /usr/bin/sha256sum — avoids libcrypto dependency)
// ============================================================================

// Compute and store SHA-256 checksum of a file in xattr
int32_t aria_shim_aifs_compute_checksum(const char* path) {
    // Use popen to call sha256sum (avoids linking -lcrypto for the shim)
    char cmd[MAX_PATH_LEN + 32];
    snprintf(cmd, sizeof(cmd), "sha256sum '%s' 2>/dev/null", path);
    FILE* fp = popen(cmd, "r");
    if (!fp) return -1;

    char hash[128] = {0};
    if (fgets(hash, sizeof(hash), fp) == NULL) {
        pclose(fp);
        return -1;
    }
    pclose(fp);

    // sha256sum output: "<hash>  <path>\n" — extract just the hash (64 hex chars)
    if (strlen(hash) < 64) return -1;
    hash[64] = '\0';

    if (setxattr(path, "user.checksum.sha256", hash, 64, 0) != 0)
        return -1;
    return 0;
}

// Get stored checksum
AriaString aria_shim_aifs_get_checksum(const char* path) {
    char buf[128] = {0};
    ssize_t len = getxattr(path, "user.checksum.sha256", buf, sizeof(buf) - 1);
    if (len <= 0) return make_string("");
    buf[len] = '\0';
    return make_string(buf);
}

// Verify checksum: compute fresh and compare with stored
// Returns: 1 = match, 0 = mismatch, -1 = error (no stored checksum)
int32_t aria_shim_aifs_verify_checksum(const char* path) {
    char stored[128] = {0};
    ssize_t slen = getxattr(path, "user.checksum.sha256", stored, sizeof(stored) - 1);
    if (slen < 64) return -1;
    stored[64] = '\0';

    char cmd[MAX_PATH_LEN + 32];
    snprintf(cmd, sizeof(cmd), "sha256sum '%s' 2>/dev/null", path);
    FILE* fp = popen(cmd, "r");
    if (!fp) return -1;

    char fresh[128] = {0};
    if (fgets(fresh, sizeof(fresh), fp) == NULL) {
        pclose(fp);
        return -1;
    }
    pclose(fp);
    if (strlen(fresh) < 64) return -1;
    fresh[64] = '\0';

    return (memcmp(stored, fresh, 64) == 0) ? 1 : 0;
}

// ============================================================================
// Sequential read optimization (posix_fadvise + readahead)
// ============================================================================

// Open file optimized for sequential read — sets POSIX_FADV_SEQUENTIAL
int64_t aria_shim_aifs_open_sequential(const char* path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    // Hint to kernel: we'll read this file sequentially
    struct stat st;
    if (fstat(fd, &st) == 0) {
        posix_fadvise(fd, 0, st.st_size, POSIX_FADV_SEQUENTIAL);
    }
    return (int64_t)fd;
}

// Read a chunk of bytes from fd into internal buffer, return bytes read
// The data is accessible via aria_shim_aifs_get_read_buffer()
static char*   g_read_chunk = NULL;
static int64_t g_read_chunk_len = 0;

int64_t aria_shim_aifs_read_chunk(int64_t fd, int64_t nbytes) {
    if (fd < 0 || nbytes <= 0) return -1;
    if (g_read_chunk) { free(g_read_chunk); g_read_chunk = NULL; }
    g_read_chunk = (char*)malloc((size_t)nbytes);
    if (!g_read_chunk) { g_read_chunk_len = 0; return -1; }
    ssize_t n = read((int)fd, g_read_chunk, (size_t)nbytes);
    if (n < 0) { free(g_read_chunk); g_read_chunk = NULL; g_read_chunk_len = 0; return -1; }
    g_read_chunk_len = (int64_t)n;
    return g_read_chunk_len;
}

// Get pointer to last read chunk as string (for Aria consumption)
AriaString aria_shim_aifs_get_read_buffer(void) {
    if (!g_read_chunk || g_read_chunk_len <= 0)
        return make_string("");
    // Make a copy as AriaString
    AriaString r;
    r.length = g_read_chunk_len;
    r.data = (char*)malloc((size_t)(r.length + 1));
    memcpy(r.data, g_read_chunk, (size_t)r.length);
    r.data[r.length] = '\0';
    return r;
}

// Seek in an open fd
int64_t aria_shim_aifs_seek(int64_t fd, int64_t offset) {
    if (fd < 0) return -1;
    off_t pos = lseek((int)fd, (off_t)offset, SEEK_SET);
    return (int64_t)pos;
}

// Trigger kernel readahead for a range
int32_t aria_shim_aifs_readahead(int64_t fd, int64_t offset, int64_t count) {
    if (fd < 0) return -1;
    // readahead() is Linux-specific
    return readahead((int)fd, (off_t)offset, (size_t)count) == 0 ? 0 : -1;
}

// ============================================================================
// Directory scanning / model index
// ============================================================================

// Scan a directory and return newline-separated list of files with AI metadata
// Each line: "<path>\t<format>\t<dtype>\t<shape>\t<size>\t<checksum>"
// Only includes files that have at least one AI xattr set
static char*   g_scan_buf = NULL;
static int64_t g_scan_len = 0;

AriaString aria_shim_aifs_scan_models(const char* dir_path) {
    if (g_scan_buf) { free(g_scan_buf); g_scan_buf = NULL; g_scan_len = 0; }

    DIR* d = opendir(dir_path);
    if (!d) return make_string("");

    size_t cap = 4096, len = 0;
    g_scan_buf = (char*)malloc(cap);
    if (!g_scan_buf) { closedir(d); return make_string(""); }
    g_scan_buf[0] = '\0';

    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char fullpath[MAX_PATH_LEN];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_path, ent->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        // Check if file has any AI metadata
        char fmt[64] = "", dtype[64] = "", shape[256] = "", cksum[128] = "";
        ssize_t fl = getxattr(fullpath, "user.model.format", fmt, sizeof(fmt) - 1);
        ssize_t dl = getxattr(fullpath, "user.tensor.dtype", dtype, sizeof(dtype) - 1);
        ssize_t sl = getxattr(fullpath, "user.tensor.shape", shape, sizeof(shape) - 1);
        ssize_t cl = getxattr(fullpath, "user.checksum.sha256", cksum, sizeof(cksum) - 1);

        if (fl <= 0 && dl <= 0 && sl <= 0 && cl <= 0) continue;

        if (fl > 0) fmt[fl] = '\0'; else strcpy(fmt, "");
        if (dl > 0) dtype[dl] = '\0'; else strcpy(dtype, "");
        if (sl > 0) shape[sl] = '\0'; else strcpy(shape, "");
        if (cl > 0) cksum[cl] = '\0'; else strcpy(cksum, "");

        // Format: path\tformat\tdtype\tshape\tsize\tchecksum\n
        char line[MAX_PATH_LEN + 512];
        int llen = snprintf(line, sizeof(line), "%s\t%s\t%s\t%s\t%ld\t%s\n",
                           fullpath, fmt, dtype, shape, (long)st.st_size, cksum);

        while (len + (size_t)llen + 1 > cap) {
            cap *= 2;
            char* tmp = (char*)realloc(g_scan_buf, cap);
            if (!tmp) { closedir(d); goto done; }
            g_scan_buf = tmp;
        }
        memcpy(g_scan_buf + len, line, (size_t)llen);
        len += (size_t)llen;
        g_scan_buf[len] = '\0';
    }
    closedir(d);

done:
    g_scan_len = (int64_t)len;
    AriaString r;
    r.length = g_scan_len;
    r.data = (char*)malloc((size_t)(r.length + 1));
    if (r.data) {
        memcpy(r.data, g_scan_buf ? g_scan_buf : "", (size_t)r.length);
        r.data[r.length] = '\0';
    }
    return r;
}

// ============================================================================
// Custom xattr (arbitrary key-value metadata)
// ============================================================================

// Set arbitrary metadata on a file (prefixed with user.aria.)
int32_t aria_shim_aifs_set_meta(const char* path, const char* key, const char* value) {
    char xattr_name[256];
    snprintf(xattr_name, sizeof(xattr_name), "user.aria.%s", key);
    if (setxattr(path, xattr_name, value, strlen(value), 0) != 0)
        return -1;
    return 0;
}

// Get arbitrary metadata
AriaString aria_shim_aifs_get_meta(const char* path, const char* key) {
    char xattr_name[256];
    snprintf(xattr_name, sizeof(xattr_name), "user.aria.%s", key);
    char buf[1024] = {0};
    ssize_t len = getxattr(path, xattr_name, buf, sizeof(buf) - 1);
    if (len <= 0) return make_string("");
    buf[len] = '\0';
    return make_string(buf);
}

#ifdef __cplusplus
}
#endif
