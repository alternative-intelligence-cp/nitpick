#include "tools/lsp/server.h"
#include <iostream>
#include <sstream>

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

Server::Server(size_t worker_count) 
    : state_(ServerState::UNINITIALIZED), thread_pool_(worker_count) {
    
    // Start with minimal capabilities
    // We'll enable more as we implement features
    capabilities_.textDocumentSync = 1; // Full sync for now
    capabilities_.diagnosticProvider = false; // We use push diagnostics (publishDiagnostics), not pull
    capabilities_.hoverProvider = true; // Show type info on hover
    capabilities_.definitionProvider = true; // Go to definition
    capabilities_.completionProvider = true; // Code completion
    capabilities_.documentSymbolProvider = true; // Document outline
    capabilities_.referencesProvider = true; // Find all references
    capabilities_.signatureHelpProvider = true; // Parameter hints
    
    // Set result callback for thread pool
    thread_pool_.set_result_callback([this](const json& id, const json& result) {
        send_response(id, result);
    });
}

Server::~Server() {
    thread_pool_.shutdown();
}

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
    
    // Special case: initialize and shutdown must run on I/O thread (synchronous)
    if (method == "initialize") {
        try {
            json result = handle_initialize(params);
            json response = Transport::makeResponse(id, result);
            transport_.write(response);
        } catch (const std::exception& e) {
            json error = Transport::makeError(id, ERROR_INTERNAL_ERROR, 
                                              std::string("Initialize error: ") + e.what());
            transport_.write(error);
        }
        return;
    }
    
    if (method == "shutdown") {
        try {
            json result = handle_shutdown(params);
            json response = Transport::makeResponse(id, result);
            transport_.write(response);
        } catch (const std::exception& e) {
            json error = Transport::makeError(id, ERROR_INTERNAL_ERROR, 
                                              std::string("Shutdown error: ") + e.what());
            transport_.write(error);
        }
        return;
    }
    
    // All other requests: Submit to thread pool
    TaskType task_type = classify_method(method);
    TaskPriority priority = get_priority(method);
    std::string uri = params.contains("textDocument") && params["textDocument"].contains("uri")
                      ? params["textDocument"]["uri"].get<std::string>() 
                      : "";
    
    // Create task with captured parameters
    Task task(task_type, priority, uri, [this, method, params, id]() -> json {
        try {
            if (method == "textDocument/hover") {
                return handle_hover(params);
            }
            else if (method == "textDocument/definition") {
                return handle_definition(params);
            }
            else if (method == "textDocument/completion") {
                return handle_completion(params);
            }
            else if (method == "textDocument/documentSymbol") {
                return handle_document_symbol(params);
            }
            else if (method == "textDocument/references") {
                return handle_references(params);
            }
            else if (method == "textDocument/signatureHelp") {
                return handle_signature_help(params);
            }
            else {
                // Method not found
                return {
                    {"error", {
                        {"code", ERROR_METHOD_NOT_FOUND},
                        {"message", "Method not found: " + method}
                    }}
                };
            }
        } catch (const std::exception& e) {
            return {
                {"error", {
                    {"code", ERROR_INTERNAL_ERROR},
                    {"message", std::string("Request handler error: ") + e.what()}
                }}
            };
        }
    });
    
    task.request_id = id;
    thread_pool_.submit(std::move(task));
}

void Server::handle_notification(const std::string& method, const json& params) {
    std::cerr << "Notification: " << method << std::endl;
    
    // Special cases: Run on I/O thread (synchronous)
    if (method == "initialized") {
        handle_initialized(params);
        return;
    }
    
    if (method == "exit") {
        handle_exit(params);
        return;
    }
    
    // Special case: $/cancelRequest
    if (method == "$/cancelRequest") {
        if (params.contains("id")) {
            thread_pool_.cancel_request(params["id"]);
        }
        return;
    }
    
    // Document notifications: Submit to thread pool
    TaskType task_type = classify_method(method);
    TaskPriority priority = get_priority(method);
    std::string uri = params.contains("textDocument") && params["textDocument"].contains("uri")
                      ? params["textDocument"]["uri"].get<std::string>() 
                      : "";
    
    // Create task (notifications have no request_id)
    Task task(task_type, priority, uri, [this, method, params]() -> json {
        try {
            if (method == "textDocument/didOpen") {
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
                std::cerr << "Unknown notification: " << method << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Notification handler error: " << e.what() << std::endl;
        }
        return nullptr; // Notifications don't return results
    });
    
    thread_pool_.submit(std::move(task));
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
            {"version", "0.3.3"}
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

json Server::handle_hover(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    int line = params["position"]["line"];       // 0-based
    int character = params["position"]["character"]; // 0-based
    
    auto content = vfs_.get_content(uri);
    if (!content) return nullptr;
    
    // Lex to find token at cursor position
    aria::frontend::Lexer lexer(*content);
    std::vector<aria::frontend::Token> tokens = lexer.tokenize();
    
    int aria_line = line + 1;
    int aria_col = character + 1;
    
    // Find token under cursor
    std::string hovered_lexeme;
    aria::frontend::TokenType hovered_type{};
    int tok_col = 0;
    int tok_len = 0;
    
    for (const auto& token : tokens) {
        if (token.line == aria_line &&
            token.column <= aria_col &&
            aria_col < token.column + static_cast<int>(token.lexeme.length())) {
            hovered_lexeme = token.lexeme;
            hovered_type = token.type;
            tok_col = token.column;
            tok_len = static_cast<int>(token.lexeme.length());
            break;
        }
    }
    
    if (hovered_lexeme.empty()) return nullptr;
    
    // Parse to get AST and build declaration index
    aria::Parser parser(tokens);
    ASTNodePtr ast;
    try { ast = parser.parse(); } catch (...) { /* fall through to token-only hover */ }
    
    std::string hover_text;
    
    if (ast) {
        auto decls = collect_declarations(ast);
        
        // Look for matching declaration
        for (const auto& decl : decls) {
            if (decl.name == hovered_lexeme) {
                hover_text = "**" + decl.kind + "** `" + decl.name + "`\n\n";
                hover_text += "```aria\n" + decl.signature + "\n```";
                break;
            }
        }
    }
    
    // Fallback: show token info for keywords, types, etc.
    if (hover_text.empty()) {
        // Check for builtin type keywords
        static const std::unordered_map<std::string, std::string> builtins = {
            {"int8", "Signed 8-bit integer (-128 to 127)"},
            {"int16", "Signed 16-bit integer"},
            {"int32", "Signed 32-bit integer"},
            {"int64", "Signed 64-bit integer (default integer type)"},
            {"uint8", "Unsigned 8-bit integer (0 to 255)"},
            {"uint16", "Unsigned 16-bit integer"},
            {"uint32", "Unsigned 32-bit integer"},
            {"uint64", "Unsigned 64-bit integer"},
            {"float32", "32-bit IEEE 754 floating point"},
            {"float64", "64-bit IEEE 754 floating point (default float type)"},
            {"flt32", "Alias for float32"},
            {"flt64", "Alias for float64"},
            {"bool", "Boolean type (true or false)"},
            {"string", "UTF-8 string type"},
            {"void", "No return value"},
            {"NIL", "Null/absent value — safe null via optional types"},
            {"ERR", "Error sentinel — use with pass/fail for result types"},
            {"unknown", "Indeterminate value — must be wrapped in ok() to propagate"},
            {"pass", "Return success result — pass(value)"},
            {"fail", "Return error result — fail(code)"},
            {"wild", "Opt-out of garbage collection (manual memory management)"},
            {"defer", "Deferred cleanup — runs when scope exits (RAII)"},
            {"pick", "Pattern matching expression"},
        };
        
        auto it = builtins.find(hovered_lexeme);
        if (it != builtins.end()) {
            hover_text = "**" + it->first + "**\n\n" + it->second;
        } else {
            hover_text = "**Token:** `" + hovered_lexeme + "`";
        }
    }
    
    return json{
        {"contents", {{"kind", "markdown"}, {"value", hover_text}}},
        {"range", {
            {"start", {{"line", line}, {"character", tok_col - 1}}},
            {"end", {{"line", line}, {"character", tok_col - 1 + tok_len}}}
        }}
    };
}

json Server::handle_definition(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    int line = params["position"]["line"];       // 0-based
    int character = params["position"]["character"]; // 0-based
    
    auto content = vfs_.get_content(uri);
    if (!content) return nullptr;
    
    // Lex to find identifier at cursor
    aria::frontend::Lexer lexer(*content);
    std::vector<aria::frontend::Token> tokens = lexer.tokenize();
    
    int aria_line = line + 1;
    int aria_col = character + 1;
    
    std::string target_name;
    for (const auto& token : tokens) {
        if (token.type == aria::frontend::TokenType::TOKEN_IDENTIFIER &&
            token.line == aria_line &&
            token.column <= aria_col &&
            aria_col < token.column + static_cast<int>(token.lexeme.length())) {
            target_name = token.lexeme;
            break;
        }
    }
    
    if (target_name.empty()) return nullptr;
    
    // Parse to get AST and find declaration
    aria::Parser parser(tokens);
    ASTNodePtr ast;
    try { ast = parser.parse(); } catch (...) { return nullptr; }
    if (!ast) return nullptr;
    
    auto decls = collect_declarations(ast);
    
    for (const auto& decl : decls) {
        if (decl.name == target_name) {
            return json{
                {"uri", uri},
                {"range", {
                    {"start", {{"line", decl.line - 1}, {"character", decl.column - 1}}},
                    {"end", {{"line", decl.line - 1}, {"character", decl.column - 1 + static_cast<int>(decl.name.length())}}}
                }}
            };
        }
    }
    
    return nullptr;
}

json Server::handle_completion(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    
    auto content = vfs_.get_content(uri);
    if (!content) return json::array();
    
    json items = json::array();
    
    // Aria keywords
    static const std::vector<std::pair<std::string, std::string>> keywords = {
        {"func", "Function declaration"},
        {"struct", "Struct declaration"},
        {"enum", "Enum declaration"},
        {"trait", "Trait declaration"},
        {"impl", "Trait implementation"},
        {"Type", "Type (composable unit) declaration"},
        {"const", "Constant declaration"},
        {"pub", "Public visibility modifier"},
        {"extern", "External/FFI declaration"},
        {"use", "Import module"},
        {"if", "Conditional branch"},
        {"else", "Alternative branch"},
        {"while", "While loop"},
        {"for", "For loop"},
        {"loop", "Counted loop: loop(start, limit, step)"},
        {"till", "Count-up loop: till(limit, step)"},
        {"when", "When/then/end loop"},
        {"pick", "Pattern matching"},
        {"pass", "Return success: pass(value)"},
        {"fail", "Return error: fail(code)"},
        {"defer", "Deferred cleanup (RAII)"},
        {"return", "Return from function"},
        {"break", "Exit loop"},
        {"continue", "Next loop iteration"},
        {"wild", "Manual memory management (opt-out GC)"},
        {"stack", "Stack allocation qualifier"},
        {"fixed", "Runtime immutability qualifier"},
        {"drop", "Discard return value"},
        {"ok", "Wrap value for safe propagation"},
        {"async", "Asynchronous function"},
        {"await", "Await async result"},
        {"move", "Transfer ownership"},
        {"NIL", "Null/absent value"},
        {"ERR", "Error sentinel"},
        {"unknown", "Indeterminate value"},
        {"true", "Boolean true"},
        {"false", "Boolean false"},
    };
    
    for (const auto& [kw, desc] : keywords) {
        items.push_back({
            {"label", kw},
            {"kind", 14},  // CompletionItemKind::Keyword
            {"detail", desc}
        });
    }
    
    // Type keywords
    static const std::vector<std::string> types = {
        "int8", "int16", "int32", "int64",
        "uint8", "uint16", "uint32", "uint64",
        "float32", "float64", "flt32", "flt64",
        "bool", "string", "void",
    };
    
    for (const auto& t : types) {
        items.push_back({
            {"label", t},
            {"kind", 25},  // CompletionItemKind::TypeParameter
            {"detail", "Built-in type"}
        });
    }
    
    // Add declared symbols from the file
    aria::frontend::Lexer lexer(*content);
    auto tokens = lexer.tokenize();
    aria::Parser parser(tokens);
    
    try {
        auto ast = parser.parse();
        if (ast) {
            auto decls = collect_declarations(ast);
            for (const auto& decl : decls) {
                int kind = 6; // Variable
                if (decl.kind == "function") kind = 3;       // Function
                else if (decl.kind == "struct") kind = 22;   // Struct  
                else if (decl.kind == "enum") kind = 13;     // Enum
                else if (decl.kind == "trait") kind = 8;     // Interface
                else if (decl.kind == "type") kind = 7;      // Class
                else if (decl.kind == "constant") kind = 21; // Constant
                
                items.push_back({
                    {"label", decl.name},
                    {"kind", kind},
                    {"detail", decl.signature}
                });
            }
        }
    } catch (...) {}
    
    return items;
}

// ============================================================================
// Document Symbol Handler (Outline View)
// ============================================================================

json Server::handle_document_symbol(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    auto content = vfs_.get_content(uri);
    if (!content) return json::array();
    
    aria::frontend::Lexer lexer(*content);
    auto tokens = lexer.tokenize();
    aria::Parser parser(tokens);
    
    json symbols = json::array();
    
    try {
        auto ast = parser.parse();
        if (ast) {
            auto decls = collect_declarations(ast);
            for (const auto& decl : decls) {
                // Skip parameters and local variables for outline
                if (decl.kind == "parameter" || decl.kind == "variable") continue;
                
                int kind = 13; // Variable (default)
                if (decl.kind == "function") kind = 12;      // Function
                else if (decl.kind == "struct") kind = 23;    // Struct
                else if (decl.kind == "enum") kind = 10;      // Enum
                else if (decl.kind == "trait") kind = 11;     // Interface
                else if (decl.kind == "type") kind = 5;       // Class
                else if (decl.kind == "constant") kind = 14;  // Constant
                
                int line0 = decl.line > 0 ? decl.line - 1 : 0;
                int col0 = decl.column > 0 ? decl.column - 1 : 0;
                int name_len = static_cast<int>(decl.name.size());
                
                json range = json{
                    {"start", {{"line", line0}, {"character", col0}}},
                    {"end", {{"line", line0}, {"character", col0 + name_len}}}
                };
                
                json sym = json{
                    {"name", decl.name},
                    {"kind", kind},
                    {"detail", decl.signature},
                    {"range", range},
                    {"selectionRange", range}
                };
                symbols.push_back(sym);
            }
        }
    } catch (...) {}
    
    return symbols;
}

// ============================================================================
// References Handler (Find All References)
// ============================================================================

json Server::handle_references(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    int line = params["position"]["line"];
    int character = params["position"]["character"];
    
    auto content = vfs_.get_content(uri);
    if (!content) return json::array();
    
    // Find the identifier at the cursor position
    aria::frontend::Lexer lexer(*content);
    auto tokens = lexer.tokenize();
    
    int aria_line = line + 1;
    int aria_col = character + 1;
    
    std::string target_name;
    for (const auto& tok : tokens) {
        if (tok.type == aria::frontend::TokenType::TOKEN_IDENTIFIER &&
            tok.line == aria_line &&
            tok.column <= aria_col &&
            aria_col < tok.column + static_cast<int>(tok.lexeme.length())) {
            target_name = tok.lexeme;
            break;
        }
    }
    
    if (target_name.empty()) return json::array();
    
    // Find all occurrences of the identifier in the file
    json locations = json::array();
    for (const auto& tok : tokens) {
        if (tok.type == aria::frontend::TokenType::TOKEN_IDENTIFIER && tok.lexeme == target_name) {
            int tok_line = tok.line - 1;
            int tok_col = tok.column - 1;
            json loc = json{
                {"uri", uri},
                {"range", {
                    {"start", {{"line", tok_line}, {"character", tok_col}}},
                    {"end", {{"line", tok_line}, {"character", tok_col + static_cast<int>(tok.lexeme.length())}}}
                }}
            };
            locations.push_back(loc);
        }
    }
    
    return locations;
}

// ============================================================================
// Signature Help Handler (Parameter Hints)
// ============================================================================

json Server::handle_signature_help(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    int line = params["position"]["line"];
    int character = params["position"]["character"];
    
    auto content = vfs_.get_content(uri);
    if (!content) return json(nullptr);
    
    // Walk backwards from cursor to find the function name before the open paren
    // Also count commas to determine active parameter
    const std::string& text = *content;
    
    // Split into lines
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string ln;
    while (std::getline(stream, ln)) {
        lines.push_back(ln);
    }
    
    if (line < 0 || line >= (int)lines.size()) return json(nullptr);
    
    const std::string& current_line = lines[line];
    if (character < 0 || character > (int)current_line.size()) return json(nullptr);
    
    // Find the matching open paren by scanning backwards
    int paren_depth = 0;
    int comma_count = 0;
    int scan_pos = character - 1;
    int func_end = -1;
    
    while (scan_pos >= 0) {
        char ch = current_line[scan_pos];
        if (ch == ')') {
            paren_depth++;
        } else if (ch == '(') {
            if (paren_depth == 0) {
                func_end = scan_pos;
                break;
            }
            paren_depth--;
        } else if (ch == ',' && paren_depth == 0) {
            comma_count++;
        }
        scan_pos--;
    }
    
    if (func_end < 0) return json(nullptr);
    
    // Extract function name before the paren
    int name_end = func_end;
    while (name_end > 0 && current_line[name_end - 1] == ' ') name_end--;
    int name_start = name_end;
    while (name_start > 0 && (std::isalnum(current_line[name_start - 1]) || current_line[name_start - 1] == '_'))
        name_start--;
    
    std::string func_name = current_line.substr(name_start, name_end - name_start);
    if (func_name.empty()) return json(nullptr);
    
    // Look up the function in AST declarations
    aria::frontend::Lexer lexer2(text);
    auto tokens = lexer2.tokenize();
    aria::Parser parser(tokens);
    
    try {
        auto ast = parser.parse();
        if (ast) {
            auto decls = collect_declarations(ast);
            for (const auto& decl : decls) {
                if (decl.kind == "function" && decl.name == func_name) {
                    // Build parameter labels from signature
                    // Signature format: "func:name = retType(params)"
                    json param_labels = json::array();
                    size_t paren_open = decl.signature.find('(');
                    size_t paren_close = decl.signature.rfind(')');
                    if (paren_open != std::string::npos && paren_close != std::string::npos && paren_close > paren_open + 1) {
                        std::string params_str = decl.signature.substr(paren_open + 1, paren_close - paren_open - 1);
                        // Split by ", "
                        std::istringstream ps(params_str);
                        std::string param;
                        while (std::getline(ps, param, ',')) {
                            // Trim leading whitespace
                            size_t start = param.find_first_not_of(' ');
                            if (start != std::string::npos)
                                param = param.substr(start);
                            json pl = json{{"label", param}};
                            param_labels.push_back(pl);
                        }
                    }
                    
                    int active_param = param_labels.empty() ? 0 : std::min(comma_count, static_cast<int>(param_labels.size()) - 1);
                    
                    json sig_info = json{
                        {"label", decl.signature},
                        {"parameters", param_labels}
                    };
                    json sigs = json::array();
                    sigs.push_back(sig_info);
                    
                    return json{
                        {"signatures", sigs},
                        {"activeSignature", 0},
                        {"activeParameter", active_param}
                    };
                }
            }
        }
    } catch (...) {}
    
    return json(nullptr);
}

// ============================================================================
// AST Analysis Helpers
// ============================================================================

std::string Server::format_param(const std::shared_ptr<ParameterNode>& param) {
    std::string result;
    if (param->typeNode) {
        result = param->typeNode->toString();
    }
    result += ":" + param->paramName;
    return result;
}

std::string Server::format_func_signature(const std::shared_ptr<FuncDeclStmt>& func) {
    std::string sig;
    if (func->isPublic) sig += "pub ";
    if (func->isExtern) sig += "extern ";
    if (func->isAsync) sig += "async ";
    sig += "func:" + func->funcName + " = ";
    
    // Return type
    if (func->returnType) {
        sig += func->returnType->toString();
    } else {
        sig += "void";
    }
    
    sig += "(";
    for (size_t i = 0; i < func->parameters.size(); ++i) {
        if (i > 0) sig += ", ";
        auto param = std::static_pointer_cast<ParameterNode>(func->parameters[i]);
        sig += format_param(param);
    }
    sig += ")";
    
    return sig;
}

std::vector<Server::DeclInfo> Server::collect_declarations(const ASTNodePtr& ast) {
    std::vector<DeclInfo> decls;
    collect_decls_recursive(ast, decls);
    return decls;
}

void Server::collect_decls_recursive(const ASTNodePtr& node, std::vector<DeclInfo>& decls) {
    if (!node) return;
    
    using NT = ASTNode::NodeType;
    
    switch (node->type) {
        case NT::PROGRAM: {
            auto prog = std::static_pointer_cast<ProgramNode>(node);
            for (const auto& stmt : prog->declarations) {
                collect_decls_recursive(stmt, decls);
            }
            break;
        }
        
        case NT::FUNC_DECL: {
            auto func = std::static_pointer_cast<FuncDeclStmt>(node);
            DeclInfo info;
            info.name = func->funcName;
            info.kind = "function";
            info.signature = format_func_signature(func);
            info.line = func->line;
            info.column = func->column;
            decls.push_back(info);
            
            // Also index parameters as local declarations
            for (const auto& p : func->parameters) {
                auto param = std::static_pointer_cast<ParameterNode>(p);
                DeclInfo pinfo;
                pinfo.name = param->paramName;
                pinfo.kind = "parameter";
                pinfo.signature = format_param(param);
                pinfo.line = param->line;
                pinfo.column = param->column;
                decls.push_back(pinfo);
            }
            
            // Walk body for local declarations
            if (func->body) {
                collect_decls_recursive(func->body, decls);
            }
            break;
        }
        
        case NT::STRUCT_DECL: {
            auto s = std::static_pointer_cast<StructDeclStmt>(node);
            DeclInfo info;
            info.name = s->structName;
            info.kind = "struct";
            info.signature = "struct:" + s->structName + " = { ";
            for (size_t i = 0; i < s->fields.size(); ++i) {
                if (i > 0) info.signature += "; ";
                auto field = std::static_pointer_cast<VarDeclStmt>(s->fields[i]);
                info.signature += field->typeName + ":" + field->varName;
            }
            info.signature += " }";
            info.line = s->line;
            info.column = s->column;
            decls.push_back(info);
            break;
        }
        
        case NT::ENUM_DECL: {
            auto e = std::static_pointer_cast<EnumDeclStmt>(node);
            DeclInfo info;
            info.name = e->enumName;
            info.kind = "enum";
            info.signature = "enum:" + e->enumName + " = { ";
            bool first = true;
            for (const auto& [vname, vval] : e->variants) {
                if (!first) info.signature += ", ";
                info.signature += vname + " = " + std::to_string(vval);
                first = false;
            }
            info.signature += " }";
            info.line = e->line;
            info.column = e->column;
            decls.push_back(info);
            break;
        }
        
        case NT::TRAIT_DECL: {
            auto t = std::static_pointer_cast<TraitDeclStmt>(node);
            DeclInfo info;
            info.name = t->traitName;
            info.kind = "trait";
            info.signature = "trait:" + t->traitName + " = { ";
            for (size_t i = 0; i < t->methods.size(); ++i) {
                if (i > 0) info.signature += "; ";
                info.signature += "func:" + t->methods[i].name;
            }
            info.signature += " }";
            info.line = t->line;
            info.column = t->column;
            decls.push_back(info);
            break;
        }
        
        case NT::TYPE_DECL: {
            auto td = std::static_pointer_cast<TypeDeclStmt>(node);
            DeclInfo info;
            info.name = td->typeName;
            info.kind = "type";
            info.signature = "Type:" + td->typeName;
            info.line = td->line;
            info.column = td->column;
            decls.push_back(info);
            
            // Walk methods inside the type
            for (const auto& m : td->methods) {
                collect_decls_recursive(m, decls);
            }
            if (td->createFunc) collect_decls_recursive(td->createFunc, decls);
            if (td->destroyFunc) collect_decls_recursive(td->destroyFunc, decls);
            break;
        }
        
        case NT::IMPL_DECL: {
            auto imp = std::static_pointer_cast<ImplDeclStmt>(node);
            // Walk impl methods
            for (const auto& m : imp->methods) {
                collect_decls_recursive(m, decls);
            }
            break;
        }
        
        case NT::VAR_DECL: {
            auto v = std::static_pointer_cast<VarDeclStmt>(node);
            DeclInfo info;
            info.name = v->varName;
            info.kind = v->isConst ? "constant" : "variable";
            info.signature = v->typeName + ":" + v->varName;
            if (v->isConst) info.signature = "const " + info.signature;
            info.line = v->line;
            info.column = v->column;
            decls.push_back(info);
            break;
        }
        
        case NT::EXTERN: {
            auto ext = std::static_pointer_cast<ExternStmt>(node);
            for (const auto& d : ext->declarations) {
                collect_decls_recursive(d, decls);
            }
            break;
        }
        
        case NT::BLOCK: {
            auto block = std::static_pointer_cast<BlockStmt>(node);
            for (const auto& stmt : block->statements) {
                collect_decls_recursive(stmt, decls);
            }
            break;
        }
        
        default:
            break;
    }
}

void Server::send_response(const json& id, const json& result) {
    // Called by worker threads when request completes
    // This is the only place worker threads write to transport
    
    if (result.contains("error")) {
        // Task returned an error
        json error = Transport::makeError(id, 
                                          result["error"]["code"], 
                                          result["error"]["message"]);
        transport_.write(error);
    } else {
        // Success response
        json response = Transport::makeResponse(id, result);
        transport_.write(response);
    }
}

TaskType Server::classify_method(const std::string& method) const {
    if (method == "initialize") return TaskType::INITIALIZE;
    if (method == "shutdown") return TaskType::SHUTDOWN;
    if (method == "textDocument/didOpen") return TaskType::DID_OPEN;
    if (method == "textDocument/didChange") return TaskType::DID_CHANGE;
    if (method == "textDocument/didClose") return TaskType::DID_CLOSE;
    if (method == "textDocument/didSave") return TaskType::DID_SAVE;
    if (method == "textDocument/hover") return TaskType::HOVER;
    if (method == "textDocument/definition") return TaskType::DEFINITION;
    if (method == "textDocument/completion") return TaskType::COMPLETION;
    if (method == "textDocument/documentSymbol") return TaskType::DOCUMENT_SYMBOL;
    if (method == "textDocument/references") return TaskType::REFERENCES;
    if (method == "textDocument/signatureHelp") return TaskType::SIGNATURE_HELP;
    return TaskType::OTHER;
}

TaskPriority Server::get_priority(const std::string& method) const {
    // Critical: State-changing notifications
    if (method == "textDocument/didOpen" || 
        method == "textDocument/didChange") {
        return TaskPriority::CRITICAL;
    }
    
    // High: User-facing queries
    if (method == "textDocument/hover" || 
        method == "textDocument/completion" ||
        method == "textDocument/definition" ||
        method == "textDocument/signatureHelp") {
        return TaskPriority::HIGH;
    }
    
    // Normal: Other document events
    if (method == "textDocument/didSave" || 
        method == "textDocument/didClose") {
        return TaskPriority::NORMAL;
    }
    
    // Low: Background analysis
    if (method == "textDocument/documentSymbol" ||
        method == "textDocument/references") {
        return TaskPriority::LOW;
    }
    
    return TaskPriority::NORMAL;
}

} // namespace lsp
} // namespace aria
