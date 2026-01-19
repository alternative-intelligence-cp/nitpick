/**
 * ffi.cpp
 * C FFI bindings for acp library
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "acp/file_copy.hpp"
#include <cstring>
#include <cstdlib>

using namespace aria::acp;

static thread_local std::string g_last_error;

extern "C" {

AriaError aria_cp_file(const char* src, const char* dst) {
    if (!src || !dst) {
        g_last_error = "null argument";
        return static_cast<AriaError>(ErrorCode::SYSTEM_ERROR);
    }

    CopyResult result = copy_file(src, dst);
    if (result.error != ErrorCode::OK) {
        g_last_error = result.error_message;
    }
    return static_cast<AriaError>(result.error);
}

AriaError aria_cp_file_opts(const char* src, const char* dst, const char* opts_json) {
    if (!src || !dst) {
        g_last_error = "null argument";
        return static_cast<AriaError>(ErrorCode::SYSTEM_ERROR);
    }

    CopyOptions opts;

    if (opts_json) {
        std::string json(opts_json);
        if (json.find("\"recursive\":true") != std::string::npos) {
            opts.recursive = true;
        }
        if (json.find("\"force\":true") != std::string::npos) {
            opts.force = true;
        }
        if (json.find("\"no_clobber\":true") != std::string::npos) {
            opts.no_clobber = true;
        }
        if (json.find("\"update\":true") != std::string::npos) {
            opts.update = true;
        }
        if (json.find("\"verbose\":true") != std::string::npos) {
            opts.verbose = true;
        }
    }

    CopyResult result = copy_file(src, dst, opts);
    if (result.error != ErrorCode::OK) {
        g_last_error = result.error_message;
    }
    return static_cast<AriaError>(result.error);
}

AriaError aria_cp_files(const char** sources, size_t count, const char* dst_dir) {
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

    CopyResult result = copy_files(src_vec, dst_dir);
    if (result.error != ErrorCode::OK) {
        g_last_error = result.error_message;
    }
    return static_cast<AriaError>(result.error);
}

AriaError aria_cp_dir(const char* src, const char* dst) {
    if (!src || !dst) {
        g_last_error = "null argument";
        return static_cast<AriaError>(ErrorCode::SYSTEM_ERROR);
    }

    CopyOptions opts;
    opts.recursive = true;

    CopyResult result = copy_directory(src, dst, opts);
    if (result.error != ErrorCode::OK) {
        g_last_error = result.error_message;
    }
    return static_cast<AriaError>(result.error);
}

const char* aria_cp_last_error(void) {
    return g_last_error.c_str();
}

}  // extern "C"
