/**
 * adap - Aria Debug Adapter Protocol Library
 *
 * Provides a reusable framework for building Debug Adapter Protocol servers.
 * Implements the DAP specification: https://microsoft.github.io/debug-adapter-protocol/
 *
 * Features:
 * - JSON-RPC message parsing and serialization
 * - Request/Response/Event handling
 * - Hex-Stream compatible (stddbg for protocol traces)
 * - TBB-safe counters
 * - Extensible handler registration
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_ADAP_DAP_PROTOCOL_HPP
#define ARIA_ADAP_DAP_PROTOCOL_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <optional>
#include <variant>

namespace npk::adap {

// =============================================================================
// TBB Types
// =============================================================================

constexpr int64_t TBB64_ERR = INT64_MIN;

inline bool is_tbb_err(int64_t val) {
    return val == TBB64_ERR;
}

// =============================================================================
// Hex-Stream File Descriptors
// =============================================================================

constexpr int FD_STDIN   = 0;
constexpr int FD_STDOUT  = 1;
constexpr int FD_STDERR  = 2;
constexpr int FD_STDDBG  = 3;

// =============================================================================
// DAP Message Types
// =============================================================================

enum class MessageType {
    REQUEST,
    RESPONSE,
    EVENT
};

// =============================================================================
// JSON Value (simplified)
// =============================================================================

class JsonValue;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

class JsonValue {
public:
    using Variant = std::variant<
        std::nullptr_t,
        bool,
        int64_t,
        double,
        std::string,
        JsonArray,
        JsonObject
    >;

    JsonValue() : data_(nullptr) {}
    JsonValue(std::nullptr_t) : data_(nullptr) {}
    JsonValue(bool v) : data_(v) {}
    JsonValue(int v) : data_(static_cast<int64_t>(v)) {}
    JsonValue(int64_t v) : data_(v) {}
    JsonValue(double v) : data_(v) {}
    JsonValue(const char* v) : data_(std::string(v)) {}
    JsonValue(const std::string& v) : data_(v) {}
    JsonValue(std::string&& v) : data_(std::move(v)) {}
    JsonValue(const JsonArray& v) : data_(v) {}
    JsonValue(JsonArray&& v) : data_(std::move(v)) {}
    JsonValue(const JsonObject& v) : data_(v) {}
    JsonValue(JsonObject&& v) : data_(std::move(v)) {}

    // Type checks
    bool is_null() const { return std::holds_alternative<std::nullptr_t>(data_); }
    bool is_bool() const { return std::holds_alternative<bool>(data_); }
    bool is_int() const { return std::holds_alternative<int64_t>(data_); }
    bool is_double() const { return std::holds_alternative<double>(data_); }
    bool is_string() const { return std::holds_alternative<std::string>(data_); }
    bool is_array() const { return std::holds_alternative<JsonArray>(data_); }
    bool is_object() const { return std::holds_alternative<JsonObject>(data_); }

    // Accessors
    bool as_bool() const { return std::get<bool>(data_); }
    int64_t as_int() const { return std::get<int64_t>(data_); }
    double as_double() const { return std::get<double>(data_); }
    const std::string& as_string() const { return std::get<std::string>(data_); }
    const JsonArray& as_array() const { return std::get<JsonArray>(data_); }
    const JsonObject& as_object() const { return std::get<JsonObject>(data_); }

    JsonArray& as_array() { return std::get<JsonArray>(data_); }
    JsonObject& as_object() { return std::get<JsonObject>(data_); }

    // Object access
    JsonValue& operator[](const std::string& key);
    const JsonValue& operator[](const std::string& key) const;
    bool has(const std::string& key) const;

    // Array access
    JsonValue& operator[](size_t index);
    const JsonValue& operator[](size_t index) const;
    size_t size() const;

    // Get with default
    template<typename T>
    T get(const std::string& key, const T& default_val) const;

    // Serialization
    std::string to_string() const;
    static std::optional<JsonValue> parse(const std::string& json);

private:
    Variant data_;
};

// =============================================================================
// DAP Messages
// =============================================================================

struct ProtocolMessage {
    int seq = 0;
    MessageType type = MessageType::REQUEST;
};

struct Request : ProtocolMessage {
    std::string command;
    JsonObject arguments;

    Request() { type = MessageType::REQUEST; }
};

struct Response : ProtocolMessage {
    int request_seq = 0;
    bool success = true;
    std::string command;
    std::string message;  // Error message if !success
    JsonObject body;

    Response() { type = MessageType::RESPONSE; }
};

struct Event : ProtocolMessage {
    std::string event;
    JsonObject body;

    Event() { type = MessageType::EVENT; }
};

// =============================================================================
// DAP Capabilities
// =============================================================================

struct Capabilities {
    bool supportsConfigurationDoneRequest = true;
    bool supportsFunctionBreakpoints = false;
    bool supportsConditionalBreakpoints = true;
    bool supportsHitConditionalBreakpoints = true;
    bool supportsEvaluateForHovers = true;
    bool supportsStepBack = false;
    bool supportsSetVariable = true;
    bool supportsRestartFrame = false;
    bool supportsGotoTargetsRequest = false;
    bool supportsStepInTargetsRequest = false;
    bool supportsCompletionsRequest = false;
    bool supportsModulesRequest = false;
    bool supportsExceptionOptions = false;
    bool supportsValueFormattingOptions = true;
    bool supportsExceptionInfoRequest = false;
    bool supportTerminateDebuggee = true;
    bool supportsDelayedStackTraceLoading = false;
    bool supportsLoadedSourcesRequest = false;
    bool supportsLogPoints = true;
    bool supportsTerminateThreadsRequest = false;
    bool supportsSetExpression = false;
    bool supportsTerminateRequest = true;
    bool supportsDataBreakpoints = false;
    bool supportsReadMemoryRequest = false;
    bool supportsDisassembleRequest = false;
    bool supportsCancelRequest = false;
    bool supportsBreakpointLocationsRequest = false;
    bool supportsClipboardContext = false;
    bool supportsSteppingGranularity = false;
    bool supportsInstructionBreakpoints = false;
    bool supportsExceptionFilterOptions = false;

    JsonObject to_json() const;
};

// =============================================================================
// Common DAP Types
// =============================================================================

struct Source {
    std::string name;
    std::string path;
    int sourceReference = 0;

    JsonObject to_json() const;
    static Source from_json(const JsonObject& obj);
};

struct SourceBreakpoint {
    int line = 0;
    int column = 0;
    std::string condition;       // Expression for conditional breakpoint
    std::string hitCondition;    // Expression for hit count condition (e.g. ">= 5")
    std::string logMessage;      // Log message template (logpoint); {expr} placeholders

    JsonObject to_json() const;
    static SourceBreakpoint from_json(const JsonObject& obj);
};

struct ExceptionBreakpointsFilter {
    std::string filter;           // Internal ID (e.g. "all", "uncaught")
    std::string label;            // Human-readable label
    std::string description;
    bool default_value = false;
    bool supportsCondition = false;
    std::string conditionDescription;

    JsonObject to_json() const;
};

struct Breakpoint {
    int id = 0;
    bool verified = false;
    std::string message;
    Source source;
    int line = 0;
    int column = 0;
    int endLine = 0;
    int endColumn = 0;

    JsonObject to_json() const;
};

struct StackFrame {
    int id = 0;
    std::string name;
    Source source;
    int line = 0;
    int column = 0;
    int endLine = 0;
    int endColumn = 0;
    std::string moduleId;
    std::string presentationHint;  // "normal", "label", "subtle"

    JsonObject to_json() const;
};

struct Scope {
    std::string name;
    std::string presentationHint;  // "arguments", "locals", "registers"
    int variablesReference = 0;
    int namedVariables = 0;
    int indexedVariables = 0;
    bool expensive = false;

    JsonObject to_json() const;
};

struct Variable {
    std::string name;
    std::string value;
    std::string type;
    std::string presentationHint;
    std::string evaluateName;
    int variablesReference = 0;
    int namedVariables = 0;
    int indexedVariables = 0;

    JsonObject to_json() const;
};

struct Thread {
    int id = 0;
    std::string name;

    JsonObject to_json() const;
};

// =============================================================================
// DAP Server
// =============================================================================

class DAPServer {
public:
    using RequestHandler = std::function<void(const Request&, Response&)>;

    /**
     * Create DAP server
     * @param in_fd Input file descriptor (default: stdin)
     * @param out_fd Output file descriptor (default: stdout)
     */
    DAPServer(int in_fd = FD_STDIN, int out_fd = FD_STDOUT);
    ~DAPServer();

    // Non-copyable
    DAPServer(const DAPServer&) = delete;
    DAPServer& operator=(const DAPServer&) = delete;

    /**
     * Register a request handler
     * @param command DAP command name
     * @param handler Handler function
     */
    void register_handler(const std::string& command, RequestHandler handler);

    /**
     * Set server capabilities
     * @param caps Capabilities to advertise
     */
    void set_capabilities(const Capabilities& caps);

    /**
     * Run the server event loop
     * Processes requests until disconnect or EOF
     * @return Exit code (0 = success)
     */
    int run();

    /**
     * Send an event to the client
     * @param event Event name
     * @param body Event body
     */
    void send_event(const std::string& event, const JsonObject& body = {});

    /**
     * Send a response to a request
     * @param request_seq Sequence number of the request
     * @param command Command name
     * @param success Success flag
     * @param body Response body
     * @param message Error message (if !success)
     */
    void send_response(int request_seq, const std::string& command,
                       bool success, const JsonObject& body = {},
                       const std::string& message = "");

    /**
     * Request shutdown (will exit run() loop)
     */
    void shutdown();

    /**
     * Check if shutdown was requested
     */
    bool is_shutdown() const { return shutdown_; }

    /**
     * Get next sequence number
     */
    int next_seq() { return ++seq_; }

    /**
     * Enable protocol tracing to stddbg (FD 3)
     */
    void enable_tracing(bool enable) { tracing_ = enable; }

private:
    int in_fd_;
    int out_fd_;
    int seq_ = 0;
    bool shutdown_ = false;
    bool initialized_ = false;
    bool tracing_ = false;
    Capabilities capabilities_;
    std::map<std::string, RequestHandler> handlers_;

    // Read a complete DAP message from input
    std::optional<Request> read_request();

    // Write a message to output
    void write_message(const std::string& json);

    // Trace message to stddbg
    void trace(const std::string& direction, const std::string& json);

    // Default handlers
    void handle_initialize(const Request& req, Response& resp);
    void handle_disconnect(const Request& req, Response& resp);
};

// =============================================================================
// Protocol Parsing
// =============================================================================

// Parse Content-Length header and return body
std::optional<std::string> read_dap_message(int fd);

// Write message with Content-Length header
void write_dap_message(int fd, const std::string& json);

// =============================================================================
// FFI (C API)
// =============================================================================

extern "C" {

typedef void* AriaDAPServerHandle;
typedef void (*AriaDAPRequestHandler)(const char* command,
                                       const char* arguments_json,
                                       char** response_json,
                                       void* user_data);

// Create server
AriaDAPServerHandle aria_dap_new(int in_fd, int out_fd);

// Free server
void aria_dap_free(AriaDAPServerHandle server);

// Register handler
void aria_dap_register_handler(AriaDAPServerHandle server,
                                const char* command,
                                AriaDAPRequestHandler handler,
                                void* user_data);

// Run server
int aria_dap_run(AriaDAPServerHandle server);

// Send event
void aria_dap_send_event(AriaDAPServerHandle server,
                          const char* event,
                          const char* body_json);

// Request shutdown
void aria_dap_shutdown(AriaDAPServerHandle server);

// Free response string (allocated by handler)
void aria_dap_free_string(char* str);

}  // extern "C"

}  // namespace npk::adap

#endif // ARIA_ADAP_DAP_PROTOCOL_HPP
