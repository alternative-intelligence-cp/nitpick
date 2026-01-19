/**
 * ffi.cpp
 * C FFI bindings for acurl library
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "acurl/http_client.hpp"
#include <cstring>
#include <memory>

using namespace aria::acurl;

extern "C" {

// Global client for simple operations
static std::unique_ptr<HttpClient> g_simple_client;

AriaHttpClientHandle aria_acurl_new(void) {
    try {
        HttpClient* client = new HttpClient();
        return static_cast<AriaHttpClientHandle>(client);
    } catch (...) {
        return nullptr;
    }
}

void aria_acurl_free(AriaHttpClientHandle handle) {
    if (handle) {
        HttpClient* client = static_cast<HttpClient*>(handle);
        delete client;
    }
}

AriaHttpResponse aria_acurl_perform(AriaHttpClientHandle handle,
                                    const AriaHttpRequest* request) {
    AriaHttpResponse resp = {};

    if (!handle || !request || !request->url) {
        resp.error = static_cast<AriaError>(ErrorCode::INVALID_URL);
        return resp;
    }

    HttpClient* client = static_cast<HttpClient*>(handle);

    HttpRequest req;
    req.url = request->url;

    // Parse method
    if (request->method) {
        std::string m = request->method;
        if (m == "GET") req.method = HttpMethod::GET;
        else if (m == "HEAD") req.method = HttpMethod::HEAD;
        else if (m == "POST") req.method = HttpMethod::POST;
        else if (m == "PUT") req.method = HttpMethod::PUT;
        else if (m == "DELETE") req.method = HttpMethod::DELETE;
        else if (m == "PATCH") req.method = HttpMethod::PATCH;
        else if (m == "OPTIONS") req.method = HttpMethod::OPTIONS;
    }

    // Body
    if (request->body && request->body_len > 0) {
        req.body = std::string(request->body, request->body_len);
    }

    // Timeout
    if (request->timeout > 0) {
        req.connect_timeout = request->timeout;
    }

    // Configure stddato
    if (request->stddato_fd >= 0) {
        client->set_stddato_fd(request->stddato_fd);
    }

    auto [response, error] = client->perform(req);

    resp.status_code = response.status_code;
    resp.content_length = response.content_length;
    resp.bytes_transferred = client->get_stats().bytes_downloaded;
    resp.time_us = response.total_time_us;
    resp.error = static_cast<AriaError>(error);

    return resp;
}

AriaHttpResponse aria_acurl_get(const char* url) {
    AriaHttpResponse resp = {};

    if (!url) {
        resp.error = static_cast<AriaError>(ErrorCode::INVALID_URL);
        return resp;
    }

    // Ensure streams are set up
    ErrorCode err = ensure_hex_streams();
    if (err != ErrorCode::OK) {
        resp.error = static_cast<AriaError>(err);
        return resp;
    }

    // Perform hex_get
    auto result = hex_get(url, false);

    resp.status_code = result.response.status_code;
    resp.content_length = result.response.content_length;
    resp.bytes_transferred = result.bytes_to_stddato;
    resp.time_us = result.response.total_time_us;
    resp.error = static_cast<AriaError>(result.error);

    return resp;
}

AriaError aria_acurl_ensure_streams(void) {
    return static_cast<AriaError>(ensure_hex_streams());
}

}  // extern "C"
