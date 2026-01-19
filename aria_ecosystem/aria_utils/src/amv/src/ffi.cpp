/**
 * ffi.cpp
 * C FFI bindings for amv library
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "amv/file_move.hpp"
#include <cstring>
#include <cstdlib>

using namespace aria::amv;

static thread_local std::string g_last_error;

extern "C" {

AriaError aria_mv_file(const char* src, const char* dst) {
    if (!src || !dst) {
        g_last_error = "null argument";
        return static_cast<AriaError>(ErrorCode::SYSTEM_ERROR);
    }

    MoveResult result = move_file(src, dst);
    if (result.error != ErrorCode::OK) {
        g_last_error = result.error_message;
    }
    return static_cast<AriaError>(result.error);
}

AriaError aria_mv_file_opts(const char* src, const char* dst, const char* opts_json) {
    if (!src || !dst) {
        g_last_error = "null argument";
        return static_cast<AriaError>(ErrorCode::SYSTEM_ERROR);
    }

    MoveOptions opts;

    if (opts_json) {
        std::string json(opts_json);
        if (json.find("\"force\":true") != std::string::npos) {
            opts.force = true;
        }
        if (json.find("\"no_clobber\":true") != std::string::npos) {
            opts.no_clobber = true;
        }
        if (json.find("\"update\":true") != std::string::npos) {
            opts.update = true;
        }
        if (json.find("\"backup\":true") != std::string::npos) {
            opts.backup = true;
        }
    }

    MoveResult result = move_file(src, dst, opts);
    if (result.error != ErrorCode::OK) {
        g_last_error = result.error_message;
    }
    return static_cast<AriaError>(result.error);
}

AriaError aria_mv_files(const char** sources, size_t count, const char* dst_dir) {
    if (!sources || count == 0 || !dst_dir) {
        g_last_error = "invalid arguments";
        return static_cast<AriaError>(ErrorCode::SYSTEM_ERROR);
    }

    std::vector<std::string> src_vec;
    for (size_t i = 0; i < count; ++i) {
        if (sources[i]) {
            src_vec.push_back(sources[i]);
        }
    }

    MoveResult result = move_files(src_vec, dst_dir);
    if (result.error != ErrorCode::OK) {
        g_last_error = result.error_message;
    }
    return static_cast<AriaError>(result.error);
}

AriaError aria_mv_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) {
        g_last_error = "null argument";
        return static_cast<AriaError>(ErrorCode::SYSTEM_ERROR);
    }

    MoveResult result = rename_file(old_path, new_path);
    if (result.error != ErrorCode::OK) {
        g_last_error = result.error_message;
    }
    return static_cast<AriaError>(result.error);
}

const char* aria_mv_last_error(void) {
    return g_last_error.c_str();
}

}  // extern "C"
