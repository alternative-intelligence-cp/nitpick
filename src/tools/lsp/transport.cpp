#include "tools/lsp/transport.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstring>

namespace aria {
namespace lsp {

Transport::Transport() {
    // Disable stdio buffering for immediate I/O
    // Critical for stdin/stdout communication with editor
    std::ios::sync_with_stdio(false);
}

Transport::~Transport() = default;

std::optional<JsonRpcMessage> Transport::read() {
    try {
        // Step 1: Read headers to get Content-Length
        int content_length = read_headers();
        if (content_length <= 0) {
            return std::nullopt; // EOF or invalid
        }
        
        // Step 2: Read exact byte count of JSON payload
        std::string content = read_content(content_length);
        
        // Step 3: Parse JSON and determine message type
        JsonRpcMessage msg = parse_message(content);
        
        return msg;
        
    } catch (const std::exception& e) {
        // Log to stderr (VS Code captures this in Output panel)
        std::cerr << "Transport::read() error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

int Transport::read_headers() {
    int content_length = -1;
    std::string line;
    
    // Read headers line by line until we hit the \r\n\r\n delimiter
    // Headers format:
    //   Content-Length: 123\r\n
    //   Content-Type: application/vscode-jsonrpc; charset=utf-8\r\n
    //   \r\n
    while (std::getline(std::cin, line)) {
        // Remove \r if present (cross-platform compatibility)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Empty line signals end of headers
        if (line.empty()) {
            break;
        }
        
        // Parse "Content-Length: NNN"
        const std::string prefix = "Content-Length: ";
        if (line.compare(0, prefix.size(), prefix) == 0) {
            try {
                content_length = std::stoi(line.substr(prefix.size()));
            } catch (...) {
                throw std::runtime_error("Invalid Content-Length header");
            }
        }
        
        // We ignore Content-Type (default is fine)
    }
    
    if (std::cin.eof()) {
        return -1; // Client closed connection
    }
    
    if (content_length <= 0) {
        throw std::runtime_error("Missing or invalid Content-Length header");
    }
    
    return content_length;
}

std::string Transport::read_content(int content_length) {
    // CRITICAL: Must read EXACT byte count (research_034 Section 2.1.1)
    // Cannot use std::cin >> or getline() - they stop at whitespace/newlines
    // Must use read() to get raw bytes
    
    std::string buffer;
    buffer.resize(content_length);
    
    std::cin.read(&buffer[0], content_length);
    
    if (std::cin.gcount() != content_length) {
        throw std::runtime_error("Failed to read complete message content");
    }
    
    return buffer;
}

JsonRpcMessage Transport::parse_message(const std::string& content) {
    JsonRpcMessage msg;
    
    try {
        msg.content = json::parse(content);
    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }
    
    // Determine message type based on JSON-RPC 2.0 spec (research_034 Table 1)
    
    // Check for 'id' field
    if (msg.content.contains("id")) {
        msg.id = msg.content["id"];
        
        // Has 'method' -> Request
        if (msg.content.contains("method")) {
            msg.type = MessageType::REQUEST;
            msg.method = msg.content["method"].get<std::string>();
        }
        // Has 'result' or 'error' -> Response
        else if (msg.content.contains("result") || msg.content.contains("error")) {
            msg.type = MessageType::RESPONSE;
            msg.is_error = msg.content.contains("error");
        }
        else {
            throw std::runtime_error("Message with id but no method/result/error");
        }
    }
    // No 'id' + has 'method' -> Notification
    else if (msg.content.contains("method")) {
        msg.type = MessageType::NOTIFICATION;
        msg.method = msg.content["method"].get<std::string>();
    }
    else {
        throw std::runtime_error("Invalid JSON-RPC message structure");
    }
    
    return msg;
}

void Transport::write(const json& message) {
    // Thread-safe write to stdout (research_034 Section 3.2.1)
    std::lock_guard<std::mutex> lock(write_mutex_);
    
    // Serialize JSON to string
    std::string content = message.dump();
    
    // Write LSP headers
    std::cout << "Content-Length: " << content.size() << "\r\n";
    std::cout << "Content-Type: application/vscode-jsonrpc; charset=utf-8\r\n";
    std::cout << "\r\n";
    
    // Write JSON payload
    std::cout << content;
    std::cout.flush(); // Critical: ensure immediate delivery
}

json Transport::makeResponse(const json& id, const json& result) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    };
}

json Transport::makeError(const json& id, int code, const std::string& message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message}
        }}
    };
}

json Transport::makeNotification(const std::string& method, const json& params) {
    return {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params}
    };
}

} // namespace lsp
} // namespace aria
