/**
 * ffi.cpp
 * C FFI bindings for adap library
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "adap/dap_protocol.hpp"
#include <cstring>
#include <memory>

using namespace npk::adap;

extern "C" {

// Handler wrapper for C callbacks
struct FFIHandlerData {
    AriaDAPRequestHandler handler;
    void* user_data;
};

AriaDAPServerHandle aria_dap_new(int in_fd, int out_fd) {
    try {
        DAPServer* server = new DAPServer(in_fd, out_fd);
        return static_cast<AriaDAPServerHandle>(server);
    } catch (...) {
        return nullptr;
    }
}

void aria_dap_free(AriaDAPServerHandle handle) {
    if (handle) {
        DAPServer* server = static_cast<DAPServer*>(handle);
        delete server;
    }
}

void aria_dap_register_handler(AriaDAPServerHandle handle,
                                const char* command,
                                AriaDAPRequestHandler handler,
                                void* user_data) {
    if (!handle || !command || !handler) return;

    DAPServer* server = static_cast<DAPServer*>(handle);

    // Capture handler data
    auto ffi_data = std::make_shared<FFIHandlerData>();
    ffi_data->handler = handler;
    ffi_data->user_data = user_data;

    server->register_handler(command, [ffi_data](const Request& req, Response& resp) {
        // Serialize arguments to JSON
        std::string args_json = JsonValue(req.arguments).to_string();

        char* response_json = nullptr;
        ffi_data->handler(req.command.c_str(), args_json.c_str(),
                          &response_json, ffi_data->user_data);

        if (response_json) {
            // Parse response JSON and update resp.body
            auto parsed = JsonValue::parse(response_json);
            if (parsed && parsed->is_object()) {
                resp.body = parsed->as_object();

                // Check for error field
                if (resp.body.count("error") > 0) {
                    resp.success = false;
                    auto& err = resp.body["error"];
                    if (err.is_string()) {
                        resp.message = err.as_string();
                    }
                    resp.body.erase("error");
                }
            }
            aria_dap_free_string(response_json);
        }
    });
}

int aria_dap_run(AriaDAPServerHandle handle) {
    if (!handle) return -1;

    DAPServer* server = static_cast<DAPServer*>(handle);
    return server->run();
}

void aria_dap_send_event(AriaDAPServerHandle handle,
                          const char* event,
                          const char* body_json) {
    if (!handle || !event) return;

    DAPServer* server = static_cast<DAPServer*>(handle);

    JsonObject body;
    if (body_json) {
        auto parsed = JsonValue::parse(body_json);
        if (parsed && parsed->is_object()) {
            body = parsed->as_object();
        }
    }

    server->send_event(event, body);
}

void aria_dap_shutdown(AriaDAPServerHandle handle) {
    if (!handle) return;

    DAPServer* server = static_cast<DAPServer*>(handle);
    server->shutdown();
}

void aria_dap_free_string(char* str) {
    if (str) {
        free(str);
    }
}

// Additional utility functions

void aria_dap_enable_tracing(AriaDAPServerHandle handle, int enable) {
    if (!handle) return;
    DAPServer* server = static_cast<DAPServer*>(handle);
    server->enable_tracing(enable != 0);
}

int aria_dap_is_shutdown(AriaDAPServerHandle handle) {
    if (!handle) return 1;
    DAPServer* server = static_cast<DAPServer*>(handle);
    return server->is_shutdown() ? 1 : 0;
}

}  // extern "C"
