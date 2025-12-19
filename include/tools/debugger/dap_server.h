/**
 * Debug Adapter Protocol (DAP) Server for Aria
 * Phase 7.4.3: Protocol bridge between editors and LLDB
 * 
 * Implements the Debug Adapter Protocol (DAP) specification:
 * https://microsoft.github.io/debug-adapter-protocol/
 * 
 * Maps DAP requests to LLDB API calls, enabling VS Code and other
 * editors to debug Aria programs through a standardized interface.
 * 
 * Reference: docs/gemini/responses/request_036_debugger.txt (Phase 3)
 */

#ifndef ARIA_DEBUGGER_DAP_SERVER_H
#define ARIA_DEBUGGER_DAP_SERVER_H

#include <lldb/API/LLDB.h>
#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

// Note: Fully qualify nlohmann::json to avoid conflicts with LLDB's internal json types

namespace aria {
namespace debugger {

/**
 * DAP Message Types
 */
enum class DAPMessageType {
    REQUEST,
    RESPONSE,
    EVENT
};

/**
 * DAP Request/Response/Event structure
 */
struct DAPMessage {
    DAPMessageType type;
    int seq;                    // Sequence number
    std::string command;        // For requests
    bool success;               // For responses
    std::string message;        // For errors
    nlohmann::json* body;       // Message body (owned by caller)
    std::string event;          // For events
    
    DAPMessage() : type(DAPMessageType::REQUEST), seq(0), success(true), body(nullptr) {}
    ~DAPMessage();
};

/**
 * Breakpoint information
 */
struct Breakpoint {
    int id;
    std::string source_path;
    int line;
    bool verified;
    lldb::SBBreakpoint lldb_breakpoint;
};

/**
 * Stack frame information
 */
struct StackFrame {
    int id;
    std::string name;
    std::string source_path;
    int line;
    int column;
};

/**
 * Variable information
 */
struct Variable {
    std::string name;
    std::string value;
    std::string type;
    int variablesReference;  // 0 if no children
    std::vector<Variable> children;
};

/**
 * DAP Server
 * 
 * Implements the Debug Adapter Protocol server that communicates with
 * editors (VS Code, etc.) and controls LLDB debugger backend.
 * 
 * Threading Model:
 * - Main thread handles DAP protocol communication
 * - Event thread monitors LLDB events and sends DAP events
 * - LLDB operations are synchronized via mutex
 */
class DAPServer {
public:
    /**
     * Create DAP server
     * @param in_fd Input file descriptor (stdin)
     * @param out_fd Output file descriptor (stdout)
     */
    DAPServer(int in_fd = 0, int out_fd = 1);
    
    ~DAPServer();

    /**
     * Start the server event loop
     * Processes DAP requests until shutdown
     * @return Exit code
     */
    int run();

    /**
     * Send DAP event to client
     * @param event Event name
     * @param body Event body (JSON)
     */
    void sendEvent(const std::string& event, const nlohmann::json& body);

private:
    // File descriptors for communication
    int m_in_fd;
    int m_out_fd;
    
    // LLDB components
    lldb::SBDebugger m_debugger;
    lldb::SBTarget m_target;
    lldb::SBProcess m_process;
    lldb::SBListener m_listener;
    
    // DAP state
    int m_next_seq;
    std::map<int, Breakpoint> m_breakpoints;
    int m_next_breakpoint_id;
    bool m_initialized;
    bool m_shutdown;
    
    // Threading
    std::unique_ptr<std::thread> m_event_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    
    // Request handlers
    using RequestHandler = std::function<void(const DAPMessage&, DAPMessage&)>;
    std::map<std::string, RequestHandler> m_handlers;

    /**
     * Initialize request handlers
     */
    void initializeHandlers();

    /**
     * Read DAP message from input
     * @return Message or nullptr on EOF
     */
    std::unique_ptr<DAPMessage> readMessage();

    /**
     * Write DAP message to output
     * @param msg Message to send
     */
    void writeMessage(const DAPMessage& msg);

    /**
     * Send response to a request
     * @param request_seq Sequence number of request
     * @param command Command name
     * @param success Success flag
     * @param body Response body
     * @param message Error message (if !success)
     */
    void sendResponse(int request_seq, const std::string& command,
                     bool success, const nlohmann::json& body,
                     const std::string& message = "");

    /**
     * Event monitoring thread
     * Listens for LLDB events and converts to DAP events
     */
    void eventThreadFunc();

    // ========================================================================
    // DAP Request Handlers
    // ========================================================================

    void handleInitialize(const DAPMessage& request, DAPMessage& response);
    void handleLaunch(const DAPMessage& request, DAPMessage& response);
    void handleAttach(const DAPMessage& request, DAPMessage& response);
    void handleConfigurationDone(const DAPMessage& request, DAPMessage& response);
    void handleDisconnect(const DAPMessage& request, DAPMessage& response);
    
    void handleSetBreakpoints(const DAPMessage& request, DAPMessage& response);
    void handleSetExceptionBreakpoints(const DAPMessage& request, DAPMessage& response);
    
    void handleContinue(const DAPMessage& request, DAPMessage& response);
    void handleNext(const DAPMessage& request, DAPMessage& response);
    void handleStepIn(const DAPMessage& request, DAPMessage& response);
    void handleStepOut(const DAPMessage& request, DAPMessage& response);
    void handlePause(const DAPMessage& request, DAPMessage& response);
    
    void handleThreads(const DAPMessage& request, DAPMessage& response);
    void handleStackTrace(const DAPMessage& request, DAPMessage& response);
    void handleScopes(const DAPMessage& request, DAPMessage& response);
    void handleVariables(const DAPMessage& request, DAPMessage& response);
    void handleEvaluate(const DAPMessage& request, DAPMessage& response);

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /**
     * Convert LLDB stack frames to DAP format
     * @param thread LLDB thread
     * @return Vector of DAP stack frames
     */
    std::vector<StackFrame> getStackFrames(lldb::SBThread thread);

    /**
     * Convert LLDB variable to DAP format
     * @param value LLDB value
     * @return DAP variable
     */
    Variable convertVariable(lldb::SBValue value);

    /**
     * Get variables for a frame
     * @param frame_id Frame identifier
     * @return Vector of variables
     */
    std::vector<Variable> getFrameVariables(int frame_id);

    /**
     * Parse path from source reference
     * @param source DAP source object
     * @return File path
     */
    std::string getSourcePath(const nlohmann::json& source);
};

} // namespace debugger
} // namespace aria

#endif // ARIA_DEBUGGER_DAP_SERVER_H
