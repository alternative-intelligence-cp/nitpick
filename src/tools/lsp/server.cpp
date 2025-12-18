#include "tools/lsp/server.h"
#include <iostream>

namespace aria {
namespace lsp {

// JSON-RPC error codes (from spec)
constexpr int ERROR_PARSE_ERROR = -32700;
constexpr int ERROR_INVALID_REQUEST = -32600;
constexpr int ERROR_METHOD_NOT_FOUND = -32601;
constexpr int ERROR_INVALID_PARAMS = -32602;
constexpr int ERROR_INTERNAL_ERROR = -32603;
constexpr int ERROR_SERVER_NOT_INITIALIZED = -32002;
constexpr int ERROR_SERVER_ERROR_START = -32099;

Server::Server() : state_(ServerState::UNINITIALIZED) {
    // Start with minimal capabilities
    // We'll enable more as we implement features
    capabilities_.textDocumentSync = 1; // Full sync for now
}

Server::~Server() = default;

void Server::run() {
    std::cerr << "Aria Language Server starting..." << std::endl;
    
    // Main message loop
    while (state_.load() != ServerState::EXITED) {
        auto msg_opt = transport_.read();
        
        if (!msg_opt.has_value()) {
            // EOF or error - client disconnected
            std::cerr << "Client disconnected" << std::endl;
            break;
        }
        
        dispatch_message(msg_opt.value());
    }
    
    std::cerr << "Aria Language Server exiting..." << std::endl;
}

void Server::dispatch_message(const JsonRpcMessage& msg) {
    try {
        switch (msg.type) {
            case MessageType::REQUEST: {
                if (!msg.id.has_value() || !msg.method.has_value()) {
                    std::cerr << "Malformed request" << std::endl;
                    return;
                }
                
                json params = msg.content.contains("params") ? msg.content["params"] : json::object();
                handle_request(msg.id.value(), msg.method.value(), params);
                break;
            }
            
            case MessageType::NOTIFICATION: {
                if (!msg.method.has_value()) {
                    std::cerr << "Malformed notification" << std::endl;
                    return;
                }
                
                json params = msg.content.contains("params") ? msg.content["params"] : json::object();
                handle_notification(msg.method.value(), params);
                break;
            }
            
            case MessageType::RESPONSE: {
                // We don't send requests to client yet, so ignore responses
                std::cerr << "Ignoring response from client" << std::endl;
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error dispatching message: " << e.what() << std::endl;
    }
}

void Server::handle_request(const json& id, const std::string& method, const json& params) {
    std::cerr << "Request: " << method << std::endl;
    
    try {
        json result;
        
        if (method == "initialize") {
            result = handle_initialize(params);
        }
        else if (method == "shutdown") {
            result = handle_shutdown(params);
        }
        else {
            // Method not found
            json error = Transport::makeError(id, ERROR_METHOD_NOT_FOUND, 
                                              "Method not found: " + method);
            transport_.write(error);
            return;
        }
        
        // Send success response
        json response = Transport::makeResponse(id, result);
        transport_.write(response);
        
    } catch (const std::exception& e) {
        // Internal error
        json error = Transport::makeError(id, ERROR_INTERNAL_ERROR, 
                                          std::string("Internal error: ") + e.what());
        transport_.write(error);
    }
}

void Server::handle_notification(const std::string& method, const json& params) {
    std::cerr << "Notification: " << method << std::endl;
    
    try {
        if (method == "initialized") {
            handle_initialized(params);
        }
        else if (method == "exit") {
            handle_exit(params);
        }
        else {
            // Unknown notification - ignore per spec (don't send response)
            std::cerr << "Unknown notification: " << method << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling notification: " << e.what() << std::endl;
        // Don't send response for notifications
    }
}

json Server::handle_initialize(const json& params) {
    std::cerr << "Handling initialize request" << std::endl;
    
    // Transition from UNINITIALIZED to INITIALIZED
    ServerState expected = ServerState::UNINITIALIZED;
    if (!state_.compare_exchange_strong(expected, ServerState::INITIALIZED)) {
        throw std::runtime_error("Server already initialized");
    }
    
    // Extract client info for logging
    if (params.contains("clientInfo")) {
        std::string clientName = params["clientInfo"].value("name", "unknown");
        std::string clientVersion = params["clientInfo"].value("version", "unknown");
        std::cerr << "Client: " << clientName << " " << clientVersion << std::endl;
    }
    
    // Build initialize result with our capabilities
    json result = {
        {"capabilities", capabilities_.to_json()},
        {"serverInfo", {
            {"name", "aria-ls"},
            {"version", "0.1.0"}
        }}
    };
    
    return result;
}

void Server::handle_initialized(const json& params) {
    std::cerr << "Client confirmed initialization" << std::endl;
    // Client is ready to receive requests/notifications from server
    // We could send workspace/configuration requests here if needed
}

json Server::handle_shutdown(const json& params) {
    std::cerr << "Handling shutdown request" << std::endl;
    
    // Transition to SHUTTING_DOWN
    state_.store(ServerState::SHUTTING_DOWN);
    
    // Return null per spec
    return nullptr;
}

void Server::handle_exit(const json& params) {
    std::cerr << "Handling exit notification" << std::endl;
    
    // Transition to EXITED - this will end the run() loop
    state_.store(ServerState::EXITED);
}

json Server::error_response(int code, const std::string& message) {
    return {
        {"code", code},
        {"message", message}
    };
}

} // namespace lsp
} // namespace aria
