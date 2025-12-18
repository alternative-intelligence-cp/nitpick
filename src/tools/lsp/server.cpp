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
    capabilities_.diagnosticProvider = true; // We can provide diagnostics!
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
        else if (method == "textDocument/didOpen") {
            handle_did_open(params);
        }
        else if (method == "textDocument/didChange") {
            handle_did_change(params);
        }
        else if (method == "textDocument/didClose") {
            handle_did_close(params);
        }
        else if (method == "textDocument/didSave") {
            handle_did_save(params);
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

// ============================================================================
// Document Synchronization
// ============================================================================

void Server::handle_did_open(const json& params) {
    // Extract document info
    // params: { textDocument: { uri, languageId, version, text } }
    
    if (!params.contains("textDocument")) {
        std::cerr << "didOpen: missing textDocument" << std::endl;
        return;
    }
    
    const json& doc = params["textDocument"];
    std::string uri = doc.value("uri", "");
    std::string text = doc.value("text", "");
    
    if (uri.empty()) {
        std::cerr << "didOpen: missing uri" << std::endl;
        return;
    }
    
    std::cerr << "Document opened: " << uri << " (" << text.size() << " bytes)" << std::endl;
    
    // Store in VFS
    vfs_.set_content(uri, text);
    
    // Publish diagnostics for this file
    publish_diagnostics(uri);
}

void Server::handle_did_change(const json& params) {
    // For Full Sync (TextDocumentSyncKind::Full = 1):
    // params: { textDocument: { uri, version }, contentChanges: [{ text }] }
    // contentChanges has exactly one element with full text
    
    if (!params.contains("textDocument") || !params.contains("contentChanges")) {
        std::cerr << "didChange: missing required fields" << std::endl;
        return;
    }
    
    std::string uri = params["textDocument"].value("uri", "");
    
    if (uri.empty()) {
        std::cerr << "didChange: missing uri" << std::endl;
        return;
    }
    
    // Get the new full text (Full Sync mode)
    const json& changes = params["contentChanges"];
    if (!changes.is_array() || changes.empty()) {
        std::cerr << "didChange: empty contentChanges" << std::endl;
        return;
    }
    
    std::string text = changes[0].value("text", "");
    
    std::cerr << "Document changed: " << uri << " (" << text.size() << " bytes)" << std::endl;
    
    // Update VFS
    vfs_.set_content(uri, text);
    
    // Publish updated diagnostics
    publish_diagnostics(uri);
}

void Server::handle_did_close(const json& params) {
    // params: { textDocument: { uri } }
    
    if (!params.contains("textDocument")) {
        std::cerr << "didClose: missing textDocument" << std::endl;
        return;
    }
    
    std::string uri = params["textDocument"].value("uri", "");
    
    if (uri.empty()) {
        std::cerr << "didClose: missing uri" << std::endl;
        return;
    }
    
    std::cerr << "Document closed: " << uri << std::endl;
    
    // Remove from VFS
    vfs_.remove(uri);
    
    // Clear diagnostics for this file
    clear_diagnostics(uri);
}

void Server::handle_did_save(const json& params) {
    // params: { textDocument: { uri }, text?: string }
    // We're using Full Sync, so we already have latest content from didChange
    // Just log it
    
    if (!params.contains("textDocument")) {
        std::cerr << "didSave: missing textDocument" << std::endl;
        return;
    }
    
    std::string uri = params["textDocument"].value("uri", "");
    std::cerr << "Document saved: " << uri << std::endl;
    
    // Could trigger additional actions here (e.g., run tests)
}

// ============================================================================
// Diagnostics Publishing
// ============================================================================

void Server::publish_diagnostics(const std::string& uri) {
    // Get file content from VFS
    auto content_opt = vfs_.get_content(uri);
    if (!content_opt.has_value()) {
        std::cerr << "Cannot publish diagnostics: file not in VFS: " << uri << std::endl;
        return;
    }
    
    std::string content = content_opt.value();
    
    try {
        // Create diagnostic engine
        aria::DiagnosticEngine diag_engine;
        
        // Lex the file
        aria::frontend::Lexer lexer(content);
        std::vector<aria::frontend::Token> tokens = lexer.tokenize();
        
        // Check for lexer errors
        if (!lexer.getErrors().empty()) {
            for (const auto& error : lexer.getErrors()) {
                // Parse error message to extract location
                // Format: "[Line X, Col Y] Error: message"
                size_t line_pos = error.find("Line ");
                size_t col_pos = error.find(", Col ");
                
                if (line_pos != std::string::npos && col_pos != std::string::npos) {
                    int line = std::stoi(error.substr(line_pos + 5, col_pos - line_pos - 5));
                    int col = std::stoi(error.substr(col_pos + 6));
                    
                    aria::SourceLocation loc(uri, line, col, 1);
                    
                    diag_engine.error(loc, error.substr(error.find("Error: ") + 7));
                }
            }
        }
        
        // Parse the tokens
        aria::Parser parser(tokens);
        try {
            auto ast = parser.parse();
            
            // Check for parser errors
            if (!parser.getErrors().empty()) {
                for (const auto& error : parser.getErrors()) {
                    // Similar parsing logic as above
                    size_t line_pos = error.find("line ");
                    size_t col_pos = error.find(", column ");
                    
                    if (line_pos != std::string::npos && col_pos != std::string::npos) {
                        int line = std::stoi(error.substr(line_pos + 5, col_pos - line_pos - 5));
                        int col = std::stoi(error.substr(col_pos + 9));
                        
                        aria::SourceLocation loc(uri, line, col, 1);
                        
                        // Extract error message
                        size_t msg_start = error.find(": ", col_pos);
                        if (msg_start != std::string::npos) {
                            diag_engine.error(loc, error.substr(msg_start + 2));
                        }
                    }
                }
            }
            
            // TODO: Semantic analysis would go here (Phase 7.3.5+)
            // TypeChecker, BorrowChecker, etc.
            
        } catch (const std::exception& e) {
            // Parser threw exception - create diagnostic
            aria::SourceLocation loc(uri, 1, 1, 1);
            diag_engine.error(loc, std::string("Parse error: ") + e.what());
        }
        
        // Convert diagnostics to LSP format
        json diagnostics_array = json::array();
        
        for (const auto& diag_ptr : diag_engine.diagnostics()) {
            diagnostics_array.push_back(convert_diagnostic_to_lsp(*diag_ptr));
        }
        
        // Build publishDiagnostics notification
        json notification = Transport::makeNotification("textDocument/publishDiagnostics", {
            {"uri", uri},
            {"diagnostics", diagnostics_array}
        });
        
        // Send to client
        transport_.write(notification);
        
        std::cerr << "Published " << diagnostics_array.size() << " diagnostics for " << uri << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error publishing diagnostics: " << e.what() << std::endl;
    }
}

void Server::clear_diagnostics(const std::string& uri) {
    // Send empty diagnostics array
    json notification = Transport::makeNotification("textDocument/publishDiagnostics", {
        {"uri", uri},
        {"diagnostics", json::array()}
    });
    
    transport_.write(notification);
    std::cerr << "Cleared diagnostics for " << uri << std::endl;
}

json Server::convert_diagnostic_to_lsp(const aria::Diagnostic& diag) {
    // Map severity (research_034 Table 2)
    int lsp_severity;
    switch (diag.level()) {
        case aria::DiagnosticLevel::NOTE:
            lsp_severity = 3; // Information
            break;
        case aria::DiagnosticLevel::WARNING:
            lsp_severity = 2; // Warning
            break;
        case aria::DiagnosticLevel::ERROR:
        case aria::DiagnosticLevel::FATAL:
            lsp_severity = 1; // Error
            break;
        default:
            lsp_severity = 1;
    }
    
    // Convert 1-based to 0-based indices (research_034 Section 5.1)
    const aria::SourceLocation& loc = diag.location();
    int lsp_line = loc.line - 1;
    int lsp_col = loc.column - 1;
    
    // Build LSP diagnostic
    json lsp_diag = {
        {"range", {
            {"start", {{"line", lsp_line}, {"character", lsp_col}}},
            {"end", {{"line", lsp_line}, {"character", lsp_col + loc.length}}}
        }},
        {"severity", lsp_severity},
        {"message", diag.message()},
        {"source", "aria"}
    };
    
    return lsp_diag;
}

} // namespace lsp
} // namespace aria
