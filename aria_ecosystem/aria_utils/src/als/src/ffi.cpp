/**
 * ffi.cpp
 * C FFI bridge for als (filesystem enumeration) library
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "als/file_list.hpp"
#include <cstdlib>
#include <cstring>
#include <new>

using namespace aria::als;

extern "C" {

// ============================================================================
// Result Structures (Aria-compatible)
// ============================================================================

typedef struct {
    char* path;             // Caller must free
    char* name;             // Caller must free
    uint64_t inode;
    int64_t size;           // TBB64_ERR if unknown
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint8_t file_type;
    int64_t mtime;
    int stat_failed;
} AriaFileEntry;

typedef struct {
    AriaFileEntry* entries;
    size_t count;
    int error_code;
    char* error_message;

    int64_t total_size;
    uint64_t file_count;
    uint64_t dir_count;
    uint64_t error_count;
} AriaListResult;

typedef struct {
    int type;               // FileType enum or -1 for any
    const char* name_glob;  // NULL for any
    int32_t uid;            // -1 for any
    int64_t min_size;       // -1 for any
    int64_t max_size;       // -1 for any
    int include_hidden;
    int follow_symlinks;
    int recursive;
    int32_t max_depth;      // -1 for unlimited
} AriaListOptions;

// ============================================================================
// Helper Functions
// ============================================================================

static char* safe_strdup(const std::string& s) {
    if (s.empty()) return nullptr;
    char* result = (char*)malloc(s.size() + 1);
    if (result) {
        memcpy(result, s.c_str(), s.size() + 1);
    }
    return result;
}

static AriaFileEntry convert_entry(const FileEntry& entry) {
    AriaFileEntry result;
    result.path = safe_strdup(entry.path);
    result.name = safe_strdup(entry.name);
    result.inode = entry.inode;
    result.size = entry.size;
    result.mode = entry.mode;
    result.uid = entry.uid;
    result.gid = entry.gid;
    result.file_type = static_cast<uint8_t>(entry.file_type);
    result.mtime = entry.mtime;
    result.stat_failed = entry.stat_failed ? 1 : 0;
    return result;
}

// ============================================================================
// Main API
// ============================================================================

/**
 * List directory contents
 */
AriaListResult aria_als_list(const char* path, const AriaListOptions* options) {
    AriaListResult result = {nullptr, 0, 0, nullptr, 0, 0, 0, 0};

    if (!path) {
        result.error_code = static_cast<int>(ErrorCode::NOT_FOUND);
        result.error_message = safe_strdup("Path is NULL");
        return result;
    }

    try {
        TraversalOptions opts;

        if (options) {
            if (options->type >= 0) {
                opts.filter.type = static_cast<FileType>(options->type);
            }
            if (options->name_glob && options->name_glob[0]) {
                opts.filter.name_glob = options->name_glob;
            }
            if (options->uid >= 0) {
                opts.filter.uid = static_cast<uint32_t>(options->uid);
            }
            if (options->min_size >= 0) {
                opts.filter.min_size = options->min_size;
            }
            if (options->max_size >= 0) {
                opts.filter.max_size = options->max_size;
            }
            opts.filter.include_hidden = (options->include_hidden != 0);
            opts.filter.follow_symlinks = (options->follow_symlinks != 0);
            opts.recursive = (options->recursive != 0);
            opts.max_depth = options->max_depth;
        }

        ListResult list_result = list_directory(path, opts);

        if (list_result.error != ErrorCode::OK) {
            result.error_code = static_cast<int>(list_result.error);
            result.error_message = safe_strdup(list_result.error_message);
            return result;
        }

        // Copy entries
        result.count = list_result.entries.size();
        if (result.count > 0) {
            result.entries = (AriaFileEntry*)malloc(result.count * sizeof(AriaFileEntry));
            if (!result.entries) {
                result.count = 0;
                result.error_code = static_cast<int>(ErrorCode::SYSTEM_ERROR);
                result.error_message = safe_strdup("Memory allocation failed");
                return result;
            }

            for (size_t i = 0; i < result.count; ++i) {
                result.entries[i] = convert_entry(list_result.entries[i]);
            }
        }

        result.total_size = list_result.total_size;
        result.file_count = list_result.file_count;
        result.dir_count = list_result.dir_count;
        result.error_count = list_result.error_count;

    } catch (const std::exception& e) {
        result.error_code = static_cast<int>(ErrorCode::UNKNOWN_ERROR);
        result.error_message = safe_strdup(e.what());
    }

    return result;
}

/**
 * Free list result
 */
void aria_als_free(AriaListResult* result) {
    if (!result) return;

    if (result->entries) {
        for (size_t i = 0; i < result->count; ++i) {
            free(result->entries[i].path);
            free(result->entries[i].name);
        }
        free(result->entries);
        result->entries = nullptr;
    }

    if (result->error_message) {
        free(result->error_message);
        result->error_message = nullptr;
    }

    result->count = 0;
}

/**
 * Get single file info
 */
AriaFileEntry aria_als_stat(const char* path, int follow_symlinks, int* error_code) {
    AriaFileEntry result = {nullptr, nullptr, 0, TBB64_ERR, 0, 0, 0, 0, 0, 1};

    if (!path) {
        if (error_code) *error_code = static_cast<int>(ErrorCode::NOT_FOUND);
        return result;
    }

    auto [entry, err] = get_file_info(path, follow_symlinks != 0);

    if (err != ErrorCode::OK) {
        if (error_code) *error_code = static_cast<int>(err);
        return result;
    }

    result = convert_entry(entry);
    if (error_code) *error_code = 0;
    return result;
}

/**
 * Free single entry
 */
void aria_als_free_entry(AriaFileEntry* entry) {
    if (!entry) return;
    free(entry->path);
    free(entry->name);
    entry->path = nullptr;
    entry->name = nullptr;
}

/**
 * Format size for display
 */
char* aria_als_format_size(int64_t size) {
    return safe_strdup(format_size(size));
}

/**
 * Format mode for display
 */
char* aria_als_format_mode(uint32_t mode) {
    return safe_strdup(format_mode(mode));
}

/**
 * Get file type character
 */
char aria_als_type_char(uint8_t file_type) {
    return type_to_char(static_cast<FileType>(file_type));
}

/**
 * Get username from UID
 */
char* aria_als_uid_to_name(uint32_t uid) {
    return safe_strdup(uid_to_name(uid));
}

/**
 * Get group name from GID
 */
char* aria_als_gid_to_name(uint32_t gid) {
    return safe_strdup(gid_to_name(gid));
}

/**
 * Check if size is TBB error
 */
int aria_als_is_tbb_err(int64_t size) {
    return is_tbb_err(size) ? 1 : 0;
}

} // extern "C"
