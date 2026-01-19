/**
 * ffi.cpp
 * C FFI bindings for acat library
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "acat/file_cat.hpp"
#include <cstring>
#include <cstdlib>

using namespace aria::acat;

// Thread-local last error message
static thread_local std::string g_last_error;

extern "C" {

AriaError aria_cat_file(const char* path) {
    if (!path) {
        g_last_error = "null path";
        return static_cast<AriaError>(ErrorCode::INVALID_ARGUMENT);
    }

    CatResult result = cat_file(path, FD_STDOUT);
    if (result.error != ErrorCode::OK) {
        g_last_error = result.error_message;
    }
    return static_cast<AriaError>(result.error);
}

AriaError aria_cat_file_fd(const char* path, int out_fd) {
    if (!path) {
        g_last_error = "null path";
        return static_cast<AriaError>(ErrorCode::INVALID_ARGUMENT);
    }

    CatResult result = cat_file(path, out_fd);
    if (result.error != ErrorCode::OK) {
        g_last_error = result.error_message;
    }
    return static_cast<AriaError>(result.error);
}

AriaError aria_cat_files(const char** paths, size_t count, int out_fd) {
    if (!paths || count == 0) {
        g_last_error = "no files specified";
        return static_cast<AriaError>(ErrorCode::INVALID_ARGUMENT);
    }

    std::vector<std::string> path_vec;
    for (size_t i = 0; i < count; ++i) {
        if (paths[i]) {
            path_vec.push_back(paths[i]);
        }
    }

    CatResult result = cat_files(path_vec, out_fd);
    if (result.error != ErrorCode::OK) {
        g_last_error = result.error_message;
    }
    return static_cast<AriaError>(result.error);
}

AriaError aria_cat_file_opts(const char* path, int out_fd, const char* opts_json) {
    if (!path) {
        g_last_error = "null path";
        return static_cast<AriaError>(ErrorCode::INVALID_ARGUMENT);
    }

    CatOptions opts;

    // Simple JSON parsing for options
    if (opts_json) {
        std::string json(opts_json);

        if (json.find("\"line_numbers\":true") != std::string::npos) {
            opts.show_line_numbers = true;
        }
        if (json.find("\"show_ends\":true") != std::string::npos) {
            opts.show_ends = true;
        }
        if (json.find("\"show_tabs\":true") != std::string::npos) {
            opts.show_tabs = true;
        }
        if (json.find("\"squeeze_blank\":true") != std::string::npos) {
            opts.squeeze_blank = true;
        }
        if (json.find("\"binary_mode\":true") != std::string::npos) {
            opts.binary_mode = true;
        }
    }

    CatResult result = cat_file(path, out_fd, opts);
    if (result.error != ErrorCode::OK) {
        g_last_error = result.error_message;
    }
    return static_cast<AriaError>(result.error);
}

AriaError aria_read_file(const char* path, uint8_t** out_data, size_t* out_size) {
    if (!path || !out_data || !out_size) {
        g_last_error = "null argument";
        return static_cast<AriaError>(ErrorCode::INVALID_ARGUMENT);
    }

    *out_data = nullptr;
    *out_size = 0;

    auto content = read_file_bytes(path);
    if (!content) {
        g_last_error = "failed to read file";
        return static_cast<AriaError>(ErrorCode::READ_ERROR);
    }

    *out_data = static_cast<uint8_t*>(malloc(content->size()));
    if (!*out_data) {
        g_last_error = "memory allocation failed";
        return static_cast<AriaError>(ErrorCode::MEMORY_ERROR);
    }

    memcpy(*out_data, content->data(), content->size());
    *out_size = content->size();

    return static_cast<AriaError>(ErrorCode::OK);
}

void aria_cat_free(void* ptr) {
    free(ptr);
}

const char* aria_cat_last_error(void) {
    return g_last_error.c_str();
}

}  // extern "C"
