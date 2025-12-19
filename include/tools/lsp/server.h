#ifndef ARIA_LSP_SERVER_H
#define ARIA_LSP_SERVER_H

#include "tools/lsp/transport.h"
#include "tools/lsp/vfs.h"
#include "tools/lsp/thread_pool.h"
#include "frontend/diagnostics.h"
#include "frontend/lexer/lexer.h"
#include "frontend/parser/parser.h"
#include <string>
#include <atomic>

namespace aria {
namespace lsp {

/**
 * LSP Server State Machine
 * 
 * Per LSP spec, server transitions through states:
 * - Uninitialized -> initialized request -> Initialized
 * - Initialized -> shutdown request -> ShuttingDown
 * - ShuttingDown -> exit notification -> Exited
 */
enum class ServerState {
    UNINITIALIZED,
    INITIALIZED,
    SHUTTING_DOWN,
    EXITED
};

/**
 * Server Capabilities
 * 
 * Declares what features this LSP server supports.
 * Start simple, add more as we implement features.
 */
struct ServerCapabilities {
    // Text document synchronization (Phase 7.3.3)
    int textDocumentSync = 1; // Full sync (TextDocumentSyncKind::Full)
    
    // Hover provider (Phase 7.3.5)
    bool hoverProvider = false;
    
    // Go to definition (Phase 7.3.5)
    bool definitionProvider = false;
    
    // Document symbols (outline view)
    bool documentSymbolProvider = false;
    
    // Workspace symbols (global search)
    bool workspaceSymbolProvider = false;
    
    // Code completion
    bool completionProvider = false;
    
    // Diagnostics
    bool diagnosticProvider = false;
    
    json to_json() const {
        return {
            {"textDocumentSync", textDocumentSync},
            {"hoverProvider", hoverProvider},
            {"definitionProvider", definitionProvider},
            {"documentSymbolProvider", documentSymbolProvider},
            {"workspaceSymbolProvider", workspaceSymbolProvider},
            {"completionProvider", completionProvider},
            {"diagnosticProvider", diagnosticProvider}
        };
    }
};

/**
 * Aria Language Server
 * 
 * Main LSP server implementation. Handles:
 * - Lifecycle (initialize, shutdown, exit)
 * - Document synchronization (didOpen, didChange, didClose)
 * - Language features (hover, definition, etc.)
 * 
 * Architecture (Phase 7.3.6 - Thread Pool):
 * - Main thread: I/O pump (read messages, write responses)
 * - Worker threads: Execute compilation/analysis tasks
 * - Work queue: Priority-based with debouncing
 * - Cancellation: Full $/cancelRequest support
 */
class Server {
public:
    /**
     * Create server with optional worker count
     * If worker_count == 0, uses CPU-based heuristic
     */
    explicit Server(size_t worker_count = 0);
    ~Server();
    
    /**
     * Main server loop - reads messages and dispatches
     * Returns when exit notification received
     */
    void run();
    
private:
    // Transport layer
    Transport transport_;
    
    // Server state
    std::atomic<ServerState> state_;
    
    // Declared capabilities
    ServerCapabilities capabilities_;
    
    // Virtual file system (shared between threads)
    VirtualFileSystem vfs_;
    
    // Thread pool for async task execution
    ThreadPool thread_pool_;
    
    // Request/notification handlers
    json handle_initialize(const json& params);
    void handle_initialized(const json& params);
    json handle_shutdown(const json& params);
    void handle_exit(const json& params);
    
    // Document synchronization handlers
    void handle_did_open(const json& params);
    void handle_did_change(const json& params);
    void handle_did_close(const json& params);
    void handle_did_save(const json& params);
    
    // Diagnostics
    void publish_diagnostics(const std::string& uri);
    void clear_diagnostics(const std::string& uri);
    json convert_diagnostic_to_lsp(const aria::Diagnostic& diag);
    
    // Navigation handlers (executed by worker threads)
    json handle_hover(const json& params);
    json handle_definition(const json& params);
    
    // Message dispatcher (I/O thread)
    void dispatch_message(const JsonRpcMessage& msg);
    void handle_request(const json& id, const std::string& method, const json& params);
    void handle_notification(const std::string& method, const json& params);
    
    // Result callback (called by worker threads)
    void send_response(const json& id, const json& result);
    
    // Error responses
    json error_response(int code, const std::string& message);
    
    // Task classification helpers
    TaskType classify_method(const std::string& method) const;
    TaskPriority get_priority(const std::string& method) const;
};

} // namespace lsp
} // namespace aria

#endif // ARIA_LSP_SERVER_H
