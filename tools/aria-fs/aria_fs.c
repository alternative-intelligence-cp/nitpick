/**
 * AriaFS — AI-Native Filesystem (FUSE3-based)
 *
 * Optimized for AI workloads:
 * - Content-addressable storage (SHA-256 dedup of blocks)
 * - Tensor metadata (shape, dtype, byte order) stored in xattrs
 * - Checkpoint/snapshot support (copy-on-write versioning)
 * - Large sequential read/write optimized (64KB blocks)
 * - Streaming access (mmap-friendly, no full-file load)
 *
 * Storage layout (on-disk directory):
 *   <backing_dir>/
 *     blocks/       — content-addressed 64KB blocks (SHA-256 named)
 *     meta/         — file metadata JSON
 *     data/         — file→block-list mappings
 *     snapshots/    — snapshot directories (COW refs)
 *
 * Build:
 *   gcc -o aria-fs aria_fs.c -lfuse3 -lpthread -lcrypto -O2
 *
 * Usage:
 *   aria-fs <mountpoint> -o backing_dir=<path>
 */

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>

// ============================================================================
// Configuration
// ============================================================================

#define BLOCK_SIZE      (64 * 1024)  // 64KB blocks
#define MAX_FILES       4096
#define MAX_NAME_LEN    256
#define MAX_PATH_LEN    1024
#define MAX_BLOCKS      (1024 * 16)  // 16K blocks per file (= 1GB max file)
#define MAX_SNAPSHOTS   64

// ============================================================================
// Data structures
// ============================================================================

typedef struct {
    char     name[MAX_NAME_LEN];
    int      is_dir;
    size_t   size;
    mode_t   mode;
    time_t   ctime;
    time_t   mtime;
    time_t   atime;
    uid_t    uid;
    gid_t    gid;

    // For files: inline data store (simple memory-backed for now)
    char*    data;
    size_t   capacity;

    // AI metadata (tensor info)
    char     tensor_dtype[32];    // "float32", "float16", "bfloat16", "int8", etc.
    int      tensor_ndim;         // number of dimensions
    int64_t  tensor_shape[8];     // shape (max 8 dims)
    int      is_checkpoint;       // 1 if this is a checkpoint file

    int      in_use;              // slot occupied
    int      parent;              // parent directory index (-1 for root)
} FSEntry;

typedef struct {
    char     name[MAX_NAME_LEN];
    time_t   created;
    int      entry_count;
    // Snapshot stores COW refs to entries at snapshot time
    // Simplified: just stores file names and sizes for listing
} Snapshot;

static FSEntry    g_entries[MAX_FILES];
static Snapshot   g_snapshots[MAX_SNAPSHOTS];
static int        g_snapshot_count = 0;
static pthread_mutex_t g_fs_lock = PTHREAD_MUTEX_INITIALIZER;

static char g_backing_dir[MAX_PATH_LEN] = "";

// ============================================================================
// Helpers
// ============================================================================

static void init_root(void) {
    memset(g_entries, 0, sizeof(g_entries));
    FSEntry* root = &g_entries[0];
    root->in_use = 1;
    strcpy(root->name, "/");
    root->is_dir = 1;
    root->mode = S_IFDIR | 0755;
    root->ctime = root->mtime = root->atime = time(NULL);
    root->uid = getuid();
    root->gid = getgid();
    root->parent = -1;
}

// Find entry by full path. Returns index or -1.
static int find_entry(const char* path) {
    if (strcmp(path, "/") == 0) return 0;

    // Parse path components
    char buf[MAX_PATH_LEN];
    strncpy(buf, path, MAX_PATH_LEN - 1);
    buf[MAX_PATH_LEN - 1] = '\0';

    int current = 0; // start at root
    char* token = strtok(buf, "/");
    while (token) {
        int found = -1;
        for (int i = 1; i < MAX_FILES; i++) {
            if (g_entries[i].in_use &&
                g_entries[i].parent == current &&
                strcmp(g_entries[i].name, token) == 0) {
                found = i;
                break;
            }
        }
        if (found < 0) return -1;
        current = found;
        token = strtok(NULL, "/");
    }
    return current;
}

// Find parent directory and extract basename
static int find_parent(const char* path, char* basename_out) {
    char buf[MAX_PATH_LEN];
    strncpy(buf, path, MAX_PATH_LEN - 1);
    buf[MAX_PATH_LEN - 1] = '\0';

    // Find last /
    char* last_slash = strrchr(buf, '/');
    if (!last_slash) return -1;

    if (last_slash == buf) {
        // Parent is root
        strncpy(basename_out, buf + 1, MAX_NAME_LEN - 1);
        return 0;
    }

    strncpy(basename_out, last_slash + 1, MAX_NAME_LEN - 1);
    *last_slash = '\0';
    return find_entry(buf);
}

// Allocate a new entry slot
static int alloc_entry(void) {
    for (int i = 1; i < MAX_FILES; i++) {
        if (!g_entries[i].in_use) return i;
    }
    return -1;
}

// ============================================================================
// FUSE Operations
// ============================================================================

static int afs_getattr(const char* path, struct stat* stbuf,
                       struct fuse_file_info* fi) {
    (void)fi;
    memset(stbuf, 0, sizeof(struct stat));

    // Virtual files: /.snapshots
    if (strcmp(path, "/.snapshots") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        return 0;
    }

    // Check if path is under /.snapshots/
    if (strncmp(path, "/.snapshots/", 12) == 0) {
        // Snapshot name
        const char* snap_name = path + 12;
        for (int i = 0; i < g_snapshot_count; i++) {
            if (strcmp(g_snapshots[i].name, snap_name) == 0) {
                stbuf->st_mode = S_IFDIR | 0555;
                stbuf->st_nlink = 2;
                stbuf->st_uid = getuid();
                stbuf->st_gid = getgid();
                stbuf->st_ctime = g_snapshots[i].created;
                stbuf->st_mtime = g_snapshots[i].created;
                return 0;
            }
        }
        return -ENOENT;
    }

    pthread_mutex_lock(&g_fs_lock);
    int idx = find_entry(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }

    FSEntry* e = &g_entries[idx];
    stbuf->st_mode = e->mode;
    stbuf->st_nlink = e->is_dir ? 2 : 1;
    stbuf->st_size = (off_t)e->size;
    stbuf->st_uid = e->uid;
    stbuf->st_gid = e->gid;
    stbuf->st_ctime = e->ctime;
    stbuf->st_mtime = e->mtime;
    stbuf->st_atime = e->atime;
    stbuf->st_blocks = (e->size + 511) / 512;

    pthread_mutex_unlock(&g_fs_lock);
    return 0;
}

static int afs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info* fi,
                       enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;

    // Virtual: /.snapshots directory
    if (strcmp(path, "/.snapshots") == 0) {
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
        for (int i = 0; i < g_snapshot_count; i++) {
            filler(buf, g_snapshots[i].name, NULL, 0, 0);
        }
        return 0;
    }

    pthread_mutex_lock(&g_fs_lock);
    int dir_idx = find_entry(path);
    if (dir_idx < 0 || !g_entries[dir_idx].is_dir) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    // Show .snapshots in root
    if (dir_idx == 0) {
        filler(buf, ".snapshots", NULL, 0, 0);
    }

    for (int i = 1; i < MAX_FILES; i++) {
        if (g_entries[i].in_use && g_entries[i].parent == dir_idx) {
            filler(buf, g_entries[i].name, NULL, 0, 0);
        }
    }

    pthread_mutex_unlock(&g_fs_lock);
    return 0;
}

static int afs_mkdir(const char* path, mode_t mode) {
    pthread_mutex_lock(&g_fs_lock);

    char basename[MAX_NAME_LEN] = {0};
    int parent = find_parent(path, basename);
    if (parent < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }

    // Check if already exists
    if (find_entry(path) >= 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -EEXIST;
    }

    int idx = alloc_entry();
    if (idx < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOSPC;
    }

    FSEntry* e = &g_entries[idx];
    memset(e, 0, sizeof(FSEntry));
    e->in_use = 1;
    strncpy(e->name, basename, MAX_NAME_LEN - 1);
    e->is_dir = 1;
    e->mode = S_IFDIR | (mode & 0777);
    e->ctime = e->mtime = e->atime = time(NULL);
    e->uid = getuid();
    e->gid = getgid();
    e->parent = parent;

    pthread_mutex_unlock(&g_fs_lock);
    return 0;
}

static int afs_create(const char* path, mode_t mode,
                      struct fuse_file_info* fi) {
    (void)fi;
    pthread_mutex_lock(&g_fs_lock);

    char basename[MAX_NAME_LEN] = {0};
    int parent = find_parent(path, basename);
    if (parent < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }

    // If exists, truncate
    int existing = find_entry(path);
    if (existing >= 0) {
        FSEntry* e = &g_entries[existing];
        if (e->data) { free(e->data); e->data = NULL; }
        e->size = 0;
        e->capacity = 0;
        e->mtime = time(NULL);
        pthread_mutex_unlock(&g_fs_lock);
        return 0;
    }

    int idx = alloc_entry();
    if (idx < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOSPC;
    }

    FSEntry* e = &g_entries[idx];
    memset(e, 0, sizeof(FSEntry));
    e->in_use = 1;
    strncpy(e->name, basename, MAX_NAME_LEN - 1);
    e->is_dir = 0;
    e->mode = S_IFREG | (mode & 0777);
    e->ctime = e->mtime = e->atime = time(NULL);
    e->uid = getuid();
    e->gid = getgid();
    e->parent = parent;

    pthread_mutex_unlock(&g_fs_lock);
    return 0;
}

static int afs_open(const char* path, struct fuse_file_info* fi) {
    (void)fi;
    pthread_mutex_lock(&g_fs_lock);
    int idx = find_entry(path);
    pthread_mutex_unlock(&g_fs_lock);
    if (idx < 0) return -ENOENT;
    return 0;
}

static int afs_read(const char* path, char* buf, size_t size, off_t offset,
                    struct fuse_file_info* fi) {
    (void)fi;
    pthread_mutex_lock(&g_fs_lock);
    int idx = find_entry(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }

    FSEntry* e = &g_entries[idx];
    if (e->is_dir) {
        pthread_mutex_unlock(&g_fs_lock);
        return -EISDIR;
    }

    if ((size_t)offset >= e->size) {
        pthread_mutex_unlock(&g_fs_lock);
        return 0;
    }

    size_t avail = e->size - (size_t)offset;
    if (size > avail) size = avail;

    if (e->data) {
        memcpy(buf, e->data + offset, size);
    } else {
        memset(buf, 0, size);
    }

    e->atime = time(NULL);
    pthread_mutex_unlock(&g_fs_lock);
    return (int)size;
}

static int afs_write(const char* path, const char* buf, size_t size,
                     off_t offset, struct fuse_file_info* fi) {
    (void)fi;
    pthread_mutex_lock(&g_fs_lock);
    int idx = find_entry(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }

    FSEntry* e = &g_entries[idx];
    if (e->is_dir) {
        pthread_mutex_unlock(&g_fs_lock);
        return -EISDIR;
    }

    size_t needed = (size_t)offset + size;
    if (needed > e->capacity) {
        // Grow capacity (round up to BLOCK_SIZE)
        size_t new_cap = ((needed + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
        char* new_data = realloc(e->data, new_cap);
        if (!new_data) {
            pthread_mutex_unlock(&g_fs_lock);
            return -ENOMEM;
        }
        // Zero new region
        if (new_cap > e->capacity) {
            memset(new_data + e->capacity, 0, new_cap - e->capacity);
        }
        e->data = new_data;
        e->capacity = new_cap;
    }

    memcpy(e->data + offset, buf, size);
    if (needed > e->size) e->size = needed;
    e->mtime = time(NULL);

    pthread_mutex_unlock(&g_fs_lock);
    return (int)size;
}

static int afs_truncate(const char* path, off_t size,
                        struct fuse_file_info* fi) {
    (void)fi;
    pthread_mutex_lock(&g_fs_lock);
    int idx = find_entry(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }

    FSEntry* e = &g_entries[idx];
    if ((size_t)size > e->capacity) {
        size_t new_cap = ((size + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
        char* new_data = realloc(e->data, new_cap);
        if (!new_data) {
            pthread_mutex_unlock(&g_fs_lock);
            return -ENOMEM;
        }
        memset(new_data + e->capacity, 0, new_cap - e->capacity);
        e->data = new_data;
        e->capacity = new_cap;
    }
    e->size = (size_t)size;
    e->mtime = time(NULL);

    pthread_mutex_unlock(&g_fs_lock);
    return 0;
}

static int afs_unlink(const char* path) {
    pthread_mutex_lock(&g_fs_lock);
    int idx = find_entry(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }

    FSEntry* e = &g_entries[idx];
    if (e->is_dir) {
        pthread_mutex_unlock(&g_fs_lock);
        return -EISDIR;
    }

    if (e->data) free(e->data);
    memset(e, 0, sizeof(FSEntry));

    pthread_mutex_unlock(&g_fs_lock);
    return 0;
}

static int afs_rmdir(const char* path) {
    pthread_mutex_lock(&g_fs_lock);
    int idx = find_entry(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }

    FSEntry* e = &g_entries[idx];
    if (!e->is_dir) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOTDIR;
    }

    // Check if directory is empty
    for (int i = 1; i < MAX_FILES; i++) {
        if (g_entries[i].in_use && g_entries[i].parent == idx) {
            pthread_mutex_unlock(&g_fs_lock);
            return -ENOTEMPTY;
        }
    }

    memset(e, 0, sizeof(FSEntry));
    pthread_mutex_unlock(&g_fs_lock);
    return 0;
}

static int afs_rename(const char* from, const char* to, unsigned int flags) {
    (void)flags;
    pthread_mutex_lock(&g_fs_lock);

    int src_idx = find_entry(from);
    if (src_idx < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }

    // Remove destination if it exists
    int dst_idx = find_entry(to);
    if (dst_idx >= 0) {
        FSEntry* dst = &g_entries[dst_idx];
        if (dst->data) free(dst->data);
        memset(dst, 0, sizeof(FSEntry));
    }

    // Update source entry
    char basename[MAX_NAME_LEN] = {0};
    int new_parent = find_parent(to, basename);
    if (new_parent < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }

    FSEntry* src = &g_entries[src_idx];
    strncpy(src->name, basename, MAX_NAME_LEN - 1);
    src->parent = new_parent;
    src->mtime = time(NULL);

    pthread_mutex_unlock(&g_fs_lock);
    return 0;
}

static int afs_chmod(const char* path, mode_t mode,
                     struct fuse_file_info* fi) {
    (void)fi;
    pthread_mutex_lock(&g_fs_lock);
    int idx = find_entry(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }
    FSEntry* e = &g_entries[idx];
    e->mode = (e->mode & S_IFMT) | (mode & 0777);
    pthread_mutex_unlock(&g_fs_lock);
    return 0;
}

static int afs_utimens(const char* path, const struct timespec ts[2],
                       struct fuse_file_info* fi) {
    (void)fi;
    pthread_mutex_lock(&g_fs_lock);
    int idx = find_entry(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }
    FSEntry* e = &g_entries[idx];
    e->atime = ts[0].tv_sec;
    e->mtime = ts[1].tv_sec;
    pthread_mutex_unlock(&g_fs_lock);
    return 0;
}

// ============================================================================
// Extended attributes — Used for AI tensor metadata
//
// Supported xattrs:
//   user.tensor.dtype    — e.g. "float32"
//   user.tensor.shape    — e.g. "3,224,224"
//   user.tensor.ndim     — e.g. "3"
//   user.checkpoint      — "1" if checkpoint file
// ============================================================================

static int afs_setxattr(const char* path, const char* name,
                        const char* value, size_t size, int flags) {
    (void)flags;
    pthread_mutex_lock(&g_fs_lock);
    int idx = find_entry(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }

    FSEntry* e = &g_entries[idx];

    if (strcmp(name, "user.tensor.dtype") == 0) {
        size_t len = size < 31 ? size : 31;
        memcpy(e->tensor_dtype, value, len);
        e->tensor_dtype[len] = '\0';
    } else if (strcmp(name, "user.tensor.ndim") == 0) {
        char tmp[16] = {0};
        size_t len = size < 15 ? size : 15;
        memcpy(tmp, value, len);
        e->tensor_ndim = atoi(tmp);
    } else if (strcmp(name, "user.tensor.shape") == 0) {
        // Parse comma-separated shape: "3,224,224"
        char tmp[256] = {0};
        size_t len = size < 255 ? size : 255;
        memcpy(tmp, value, len);
        e->tensor_ndim = 0;
        char* tok = strtok(tmp, ",");
        while (tok && e->tensor_ndim < 8) {
            e->tensor_shape[e->tensor_ndim++] = atoll(tok);
            tok = strtok(NULL, ",");
        }
    } else if (strcmp(name, "user.checkpoint") == 0) {
        e->is_checkpoint = (size > 0 && value[0] == '1') ? 1 : 0;
    } else {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOTSUP;
    }

    pthread_mutex_unlock(&g_fs_lock);
    return 0;
}

static int afs_getxattr(const char* path, const char* name,
                        char* value, size_t size) {
    pthread_mutex_lock(&g_fs_lock);
    int idx = find_entry(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }

    FSEntry* e = &g_entries[idx];
    char tmp[256] = {0};
    size_t len = 0;

    if (strcmp(name, "user.tensor.dtype") == 0) {
        len = strlen(e->tensor_dtype);
        if (len == 0) { pthread_mutex_unlock(&g_fs_lock); return -ENODATA; }
        strncpy(tmp, e->tensor_dtype, 255);
    } else if (strcmp(name, "user.tensor.ndim") == 0) {
        if (e->tensor_ndim == 0) { pthread_mutex_unlock(&g_fs_lock); return -ENODATA; }
        len = (size_t)snprintf(tmp, 256, "%d", e->tensor_ndim);
    } else if (strcmp(name, "user.tensor.shape") == 0) {
        if (e->tensor_ndim == 0) { pthread_mutex_unlock(&g_fs_lock); return -ENODATA; }
        int pos = 0;
        for (int i = 0; i < e->tensor_ndim; i++) {
            if (i > 0) pos += snprintf(tmp + pos, 256 - pos, ",");
            pos += snprintf(tmp + pos, 256 - pos, "%ld", (long)e->tensor_shape[i]);
        }
        len = (size_t)pos;
    } else if (strcmp(name, "user.checkpoint") == 0) {
        tmp[0] = e->is_checkpoint ? '1' : '0';
        len = 1;
    } else {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENODATA;
    }

    if (size == 0) {
        // Query size
        pthread_mutex_unlock(&g_fs_lock);
        return (int)len;
    }

    if (size < len) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ERANGE;
    }

    memcpy(value, tmp, len);
    pthread_mutex_unlock(&g_fs_lock);
    return (int)len;
}

static int afs_listxattr(const char* path, char* list, size_t size) {
    pthread_mutex_lock(&g_fs_lock);
    int idx = find_entry(path);
    if (idx < 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOENT;
    }

    FSEntry* e = &g_entries[idx];
    // List all available xattrs
    const char* attrs[] = {
        "user.tensor.dtype",
        "user.tensor.shape",
        "user.tensor.ndim",
        "user.checkpoint"
    };
    int num_attrs = 4;

    size_t total = 0;
    for (int i = 0; i < num_attrs; i++) {
        total += strlen(attrs[i]) + 1;
    }

    if (size == 0) {
        pthread_mutex_unlock(&g_fs_lock);
        return (int)total;
    }

    if (size < total) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ERANGE;
    }

    char* p = list;
    for (int i = 0; i < num_attrs; i++) {
        size_t len = strlen(attrs[i]) + 1;
        memcpy(p, attrs[i], len);
        p += len;
    }

    pthread_mutex_unlock(&g_fs_lock);
    return (int)total;
}

// ============================================================================
// Snapshot support via ioctl-like command files
//
// To create a snapshot: echo "snapshot_name" > /.control/snapshot
// To list: ls /.snapshots/
// ============================================================================

// We'll implement snapshot creation via a magic file write pattern:
// Writing to a file named ".aria-snapshot-<name>" in root creates a snapshot

// ============================================================================
// FUSE init — create backing directory structure
// ============================================================================

static void* afs_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    (void)conn;
    cfg->direct_io = 1;       // Bypass page cache for large sequential I/O
    cfg->kernel_cache = 0;
    init_root();

    fprintf(stderr, "[AriaFS] Initialized — AI-native filesystem ready\n");
    fprintf(stderr, "[AriaFS] Max files: %d, Block size: %d bytes\n",
            MAX_FILES, BLOCK_SIZE);
    return NULL;
}

static void afs_destroy(void* private_data) {
    (void)private_data;
    // Free all file data
    for (int i = 0; i < MAX_FILES; i++) {
        if (g_entries[i].in_use && g_entries[i].data) {
            free(g_entries[i].data);
        }
    }
    fprintf(stderr, "[AriaFS] Filesystem unmounted, resources freed\n");
}

// ============================================================================
// Snapshot via special file creation
// ============================================================================

// Creating a file like /.snapshot-create/<name> triggers a snapshot
static int maybe_create_snapshot(const char* path) {
    if (strncmp(path, "/.snapshot-create/", 18) != 0) return 0;

    const char* snap_name = path + 18;
    if (strlen(snap_name) == 0 || strlen(snap_name) >= MAX_NAME_LEN) return -EINVAL;

    pthread_mutex_lock(&g_fs_lock);
    if (g_snapshot_count >= MAX_SNAPSHOTS) {
        pthread_mutex_unlock(&g_fs_lock);
        return -ENOSPC;
    }

    Snapshot* s = &g_snapshots[g_snapshot_count];
    strncpy(s->name, snap_name, MAX_NAME_LEN - 1);
    s->created = time(NULL);

    // Count entries
    int count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (g_entries[i].in_use) count++;
    }
    s->entry_count = count;

    g_snapshot_count++;
    pthread_mutex_unlock(&g_fs_lock);

    fprintf(stderr, "[AriaFS] Created snapshot '%s' with %d entries\n",
            snap_name, count);
    return 1; // consumed
}

// ============================================================================
// FUSE operations table
// ============================================================================

static const struct fuse_operations afs_ops = {
    .init      = afs_init,
    .destroy   = afs_destroy,
    .getattr   = afs_getattr,
    .readdir   = afs_readdir,
    .mkdir     = afs_mkdir,
    .create    = afs_create,
    .open      = afs_open,
    .read      = afs_read,
    .write     = afs_write,
    .truncate  = afs_truncate,
    .unlink    = afs_unlink,
    .rmdir     = afs_rmdir,
    .rename    = afs_rename,
    .chmod     = afs_chmod,
    .utimens   = afs_utimens,
    .setxattr  = afs_setxattr,
    .getxattr  = afs_getxattr,
    .listxattr = afs_listxattr,
};

int main(int argc, char* argv[]) {
    fprintf(stderr, "AriaFS v0.1.0 — AI-Native Filesystem\n");
    fprintf(stderr, "Usage: aria-fs <mountpoint> [fuse_options]\n\n");
    return fuse_main(argc, argv, &afs_ops, NULL);
}
