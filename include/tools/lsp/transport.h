#ifndef ARIA_LSP_TRANSPORT_H
#define ARIA_LSP_TRANSPORT_H

#include <string>
#include <optional>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>

namespace aria {
namespace lsp {

using json = nlohmann::json;

/**
 * JSON-RPC 2.0 Message Types
 * 
 * As per research_034_lsp.txt Section 2.1.2:
 * - Request: Has id, requires response
 * - Notification: No id, fire-and-forget
 * - Response: Has id, result or error
 */
enum class MessageType {
    REQUEST,
    NOTIFICATION,
    RESPONSE
};

/**
 * Parsed JSON-RPC message with type information
 */
struct JsonRpcMessage {
    MessageType type;
    json content;
    
    // For requests/responses
    std::optional<json> id;
    
    // For requests/notifications
    std::optional<std::string> method;
    
    // For responses
    bool is_error = false;
};

/**
 * LSP Transport Layer
 * 
 * Implements the LSP header-based framing over stdin/stdout.
 * 
 * Critical Implementation Details (from research_034):
 * - Messages have Header Part + Content Part separated by \r\n\r\n
 * - Content-Length header specifies exact byte count
 * - MUST read exact byte count (not line-based or whitespace-delimited)
 * - Content-Type defaults to application/vscode-jsonrpc; charset=utf-8
 * 
 * Thread Safety:
 * - read() is NOT thread-safe (stdin is shared)
 * - write() uses internal mutex for thread-safe stdout access
 */
class Transport {
public:
    Transport();
    ~Transport();
    
    /**
     * Read one complete LSP message from stdin.
     * 
     * Returns std::nullopt on EOF or fatal error.
     * Throws std::runtime_error on parse errors (caught by main loop).
     * 
     * Implementation Strategy (research_034 Section 2.1.1):
     * 1. Read headers line-by-line until \r\n\r\n
     * 2. Extract Content-Length value
     * 3. Allocate exact-size buffer
     * 4. Read exact number of bytes (blocking)
     * 5. Parse JSON payload
     */
    std::optional<JsonRpcMessage> read();
    
    /**
     * Write a JSON-RPC message to stdout.
     * 
     * Thread-safe: Multiple worker threads can call concurrently.
     * Automatically adds LSP headers (Content-Length, Content-Type).
     */
    void write(const json& message);
    
    /**
     * Helper to construct JSON-RPC response
     */
    static json makeResponse(const json& id, const json& result);
    
    /**
     * Helper to construct JSON-RPC error response
     */
    static json makeError(const json& id, int code, const std::string& message);
    
    /**
     * Helper to construct JSON-RPC notification
     */
    static json makeNotification(const std::string& method, const json& params);

private:
    /**
     * Read headers until \r\n\r\n delimiter.
     * Returns Content-Length value.
     */
    int read_headers();
    
    /**
     * Read exact number of bytes from stdin.
     * Blocking call that fills the buffer completely.
     */
    std::string read_content(int content_length);
    
    /**
     * Parse JSON-RPC message and determine type
     */
    JsonRpcMessage parse_message(const std::string& content);
    
    // Mutex for thread-safe stdout writes
    std::mutex write_mutex_;
};

} // namespace lsp
} // namespace aria

#endif // ARIA_LSP_TRANSPORT_H
