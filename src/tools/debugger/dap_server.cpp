/**
 * Debug Adapter Protocol Server Implementation
 * Phase 7.4.3: DAP-to-LLDB bridge
 */

#include "tools/debugger/dap_server.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <thread>
#include <chrono>

// Note: Fully qualify nlohmann::nlohmann::json to avoid conflicts with LLDB's internal JSON types

namespace aria {
namespace debugger {

// ============================================================================
// DAPMessage
// ============================================================================

DAPMessage::~DAPMessage() {
    if (body) {
        delete body;
        body = nullptr;
    }
}

// ============================================================================
// DAPServer
// ============================================================================

DAPServer::DAPServer(int in_fd, int out_fd)
    : m_in_fd(in_fd),
      m_out_fd(out_fd),
      m_next_seq(1),
      m_next_breakpoint_id(1),
      m_initialized(false),
      m_shutdown(false)
{
    // Ensure LLDB can find lldb-server (may not be on PATH)
    if (!getenv("LLDB_DEBUGSERVER_PATH")) {
        // Try common LLVM installation paths
        const char* paths[] = {
            "/usr/lib/llvm-20/bin/lldb-server",
            "/usr/lib/llvm-19/bin/lldb-server",
            "/usr/lib/llvm-18/bin/lldb-server",
            "/usr/local/lib/llvm/bin/lldb-server",
            nullptr
        };
        for (const char** p = paths; *p; ++p) {
            if (access(*p, X_OK) == 0) {
                setenv("LLDB_DEBUGSERVER_PATH", *p, 0);
                break;
            }
        }
    }
    
    // Initialize LLDB
    lldb::SBDebugger::Initialize();
    m_debugger = lldb::SBDebugger::Create();
    
    // Create listener for events
    m_listener = m_debugger.GetListener();
    
    // Initialize request handlers
    initializeHandlers();
}

DAPServer::~DAPServer() {
    m_shutdown = true;
    m_cv.notify_all();
    
    if (m_event_thread && m_event_thread->joinable()) {
        m_event_thread->join();
    }
    
    // Cleanup LLDB
    if (m_process.IsValid()) {
        m_process.Destroy();
    }
    if (m_target.IsValid()) {
        m_debugger.DeleteTarget(m_target);
    }
    lldb::SBDebugger::Destroy(m_debugger);
    lldb::SBDebugger::Terminate();
}

void DAPServer::initializeHandlers() {
    m_handlers["initialize"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleInitialize(req, res);
    };
    m_handlers["launch"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleLaunch(req, res);
    };
    m_handlers["attach"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleAttach(req, res);
    };
    m_handlers["configurationDone"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleConfigurationDone(req, res);
    };
    m_handlers["disconnect"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleDisconnect(req, res);
    };
    
    m_handlers["setBreakpoints"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleSetBreakpoints(req, res);
    };
    m_handlers["setExceptionBreakpoints"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleSetExceptionBreakpoints(req, res);
    };
    
    m_handlers["continue"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleContinue(req, res);
    };
    m_handlers["next"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleNext(req, res);
    };
    m_handlers["stepIn"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleStepIn(req, res);
    };
    m_handlers["stepOut"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleStepOut(req, res);
    };
    m_handlers["pause"] = [this](const DAPMessage& req, DAPMessage& res) {
        handlePause(req, res);
    };
    
    m_handlers["threads"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleThreads(req, res);
    };
    m_handlers["stackTrace"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleStackTrace(req, res);
    };
    m_handlers["scopes"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleScopes(req, res);
    };
    m_handlers["variables"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleVariables(req, res);
    };
    m_handlers["evaluate"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleEvaluate(req, res);
    };
    
    // Terminate request
    m_handlers["terminate"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleDisconnect(req, res);
    };
    
    // Custom Aria extensions (Phase 7.4.6)
    m_handlers["ariaMemoryMap"] = [this](const DAPMessage& req, DAPMessage& res) {
        handleAriaMemoryMap(req, res);
    };
}

int DAPServer::run() {
    std::cerr << "[DAP] Server starting...\n";
    
    // Main protocol loop
    while (!m_shutdown) {
        auto request = readMessage();
        if (!request) {
            // EOF or error
            break;
        }
        
        // Process request
        DAPMessage response;
        response.type = DAPMessageType::RESPONSE;
        {
            std::lock_guard<std::mutex> lock(m_write_mutex);
            response.seq = m_next_seq++;
        }
        response.request_seq = request->seq;
        response.command = request->command;
        response.success = true;
        
        // Find and call handler
        auto it = m_handlers.find(request->command);
        if (it != m_handlers.end()) {
            try {
                it->second(*request, response);
            } catch (const std::exception& e) {
                response.success = false;
                response.message = std::string("Handler error: ") + e.what();
            }
        } else {
            response.success = false;
            response.message = "Unknown command: " + request->command;
        }
        
        writeMessage(response);
        
        // Send initialized event after initialize response (DAP spec requirement)
        if (request->command == "initialize" && response.success) {
            sendEvent("initialized", nlohmann::json::object());
        }
    }
    
    std::cerr << "[DAP] Server exiting\n";
    return 0;
}

std::unique_ptr<DAPMessage> DAPServer::readMessage() {
    // Read headers (DAP uses HTTP-style Content-Length headers with \r\n)
    int content_length = -1;
    std::string header;
    while (std::getline(std::cin, header)) {
        // Strip trailing \r (DAP uses \r\n line endings)
        if (!header.empty() && header.back() == '\r') {
            header.pop_back();
        }
        
        // Empty line marks end of headers
        if (header.empty()) {
            break;
        }
        
        // Parse Content-Length
        size_t colon = header.find(':');
        if (colon != std::string::npos) {
            std::string name = header.substr(0, colon);
            if (name == "Content-Length") {
                content_length = std::stoi(header.substr(colon + 1));
            }
        }
    }
    
    if (std::cin.eof() || content_length < 0) {
        return nullptr;
    }
    
    // Read content body
    std::string content;
    content.resize(content_length);
    std::cin.read(&content[0], content_length);
    
    if (std::cin.gcount() != content_length) {
        std::cerr << "[DAP] Short read: expected " << content_length << ", got " << std::cin.gcount() << "\n";
        return nullptr;
    }
    
    // Parse JSON
    try {
        nlohmann::json j = nlohmann::json::parse(content);
        
        auto msg = std::make_unique<DAPMessage>();
        msg->seq = j["seq"];
        msg->type = DAPMessageType::REQUEST;
        msg->command = j["command"];
        
        if (j.contains("arguments")) {
            msg->body = new nlohmann::json(j["arguments"]);
        }
        
        return msg;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "[DAP] JSON parse error: " << e.what() << "\n";
        return nullptr;
    }
}

void DAPServer::writeMessage(const DAPMessage& msg) {
    std::lock_guard<std::mutex> lock(m_write_mutex);
    nlohmann::json j;
    j["seq"] = msg.seq;
    
    if (msg.type == DAPMessageType::RESPONSE) {
        j["type"] = "response";
        j["request_seq"] = msg.request_seq;
        j["command"] = msg.command;
        j["success"] = msg.success;
        
        if (!msg.success) {
            j["message"] = msg.message;
        }
        
        if (msg.body) {
            j["body"] = *msg.body;
        }
    } else if (msg.type == DAPMessageType::EVENT) {
        j["type"] = "event";
        j["event"] = msg.event;
        
        if (msg.body) {
            j["body"] = *msg.body;
        }
    }
    
    std::string content = j.dump();
    std::string header = "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n";
    
    std::cout << header << content << std::flush;
}

void DAPServer::sendResponse(int request_seq, const std::string& command,
                             bool success, const nlohmann::json& body,
                             const std::string& message) {
    DAPMessage response;
    response.type = DAPMessageType::RESPONSE;
    {
        std::lock_guard<std::mutex> lock(m_write_mutex);
        response.seq = m_next_seq++;
    }
    response.request_seq = request_seq;
    response.command = command;
    response.success = success;
    response.message = message;
    response.body = new nlohmann::json(body);
    
    writeMessage(response);
}

void DAPServer::sendEvent(const std::string& event, const nlohmann::json& body) {
    DAPMessage evt;
    evt.type = DAPMessageType::EVENT;
    {
        std::lock_guard<std::mutex> lock(m_write_mutex);
        evt.seq = m_next_seq++;
    }
    evt.event = event;
    evt.body = new nlohmann::json(body);
    
    writeMessage(evt);
}

void DAPServer::eventThreadFunc() {
    std::cerr << "[DAP] Event thread started\n";
    
    while (!m_shutdown) {
        lldb::SBEvent event;
        if (m_listener.WaitForEvent(1, event)) {
            if (!event.IsValid()) continue;
            
            // Process event
            uint32_t event_type = event.GetType();
            
            if (lldb::SBProcess::EventIsProcessEvent(event)) {
                lldb::StateType state = lldb::SBProcess::GetStateFromEvent(event);
                std::cerr << "[DAP] Process event: state=" << state << "\n";
                
                switch (state) {
                case lldb::eStateStopped: {
                    // Thread stopped - determine stop reason
                    lldb::SBThread thread = m_process.GetSelectedThread();
                    if (thread.IsValid()) {
                        nlohmann::json body;
                        body["threadId"] = static_cast<int64_t>(thread.GetThreadID());
                        body["allThreadsStopped"] = true;
                        
                        // Determine stop reason
                        lldb::StopReason reason = thread.GetStopReason();
                        switch (reason) {
                        case lldb::eStopReasonBreakpoint:
                            body["reason"] = "breakpoint";
                            break;
                        case lldb::eStopReasonWatchpoint:
                            body["reason"] = "data breakpoint";
                            break;
                        case lldb::eStopReasonSignal:
                            body["reason"] = "pause";
                            break;
                        case lldb::eStopReasonPlanComplete:
                            body["reason"] = "step";
                            break;
                        case lldb::eStopReasonException: {
                            body["reason"] = "exception";
                            char desc[256];
                            size_t len = thread.GetStopDescription(desc, sizeof(desc));
                            if (len > 0) {
                                body["text"] = std::string(desc, len);
                            }
                            break;
                        }
                        default:
                            body["reason"] = "pause";
                            break;
                        }
                        
                        sendEvent("stopped", body);
                    }
                    break;
                }
                case lldb::eStateExited: {
                    nlohmann::json body;
                    body["exitCode"] = m_process.GetExitStatus();
                    sendEvent("exited", body);
                    
                    sendEvent("terminated", nlohmann::json::object());
                    break;
                }
                case lldb::eStateCrashed: {
                    sendEvent("terminated", nlohmann::json::object());
                    break;
                }
                default:
                    break;
                }
            } else if (lldb::SBBreakpoint::EventIsBreakpointEvent(event)) {
                // Breakpoint added/removed/changed
                uint32_t bp_event_type = lldb::SBBreakpoint::GetBreakpointEventTypeFromEvent(event);
                
                if (bp_event_type & lldb::eBreakpointEventTypeLocationsAdded) {
                    // Breakpoint verified
                    lldb::SBBreakpoint bp = lldb::SBBreakpoint::GetBreakpointFromEvent(event);
                    
                    nlohmann::json body;
                    body["reason"] = "changed";
                    body["breakpoint"] = {
                        {"id", static_cast<int>(bp.GetID())},
                        {"verified", bp.GetNumLocations() > 0}
                    };
                    
                    sendEvent("breakpoint", body);
                }
            }
        }
    }
    
    std::cerr << "[DAP] Event thread exiting\n";
}

// ============================================================================
// Request Handlers
// ============================================================================

void DAPServer::handleInitialize(const DAPMessage& request, DAPMessage& response) {
    std::cerr << "[DAP] Initialize\n";
    
    nlohmann::json capabilities;
    capabilities["supportsConfigurationDoneRequest"] = true;
    capabilities["supportsEvaluateForHovers"] = true;
    capabilities["supportsStepBack"] = false;
    capabilities["supportsSetVariable"] = false;
    capabilities["supportsRestartFrame"] = false;
    capabilities["supportsGotoTargetsRequest"] = false;
    capabilities["supportsStepInTargetsRequest"] = false;
    capabilities["supportsCompletionsRequest"] = false;
    capabilities["completionTriggerCharacters"] = nlohmann::json::array();
    capabilities["supportsModulesRequest"] = false;
    capabilities["additionalModuleColumns"] = nlohmann::json::array();
    capabilities["supportedChecksumAlgorithms"] = nlohmann::json::array();
    capabilities["supportsRestartRequest"] = false;
    capabilities["supportsExceptionOptions"] = false;
    capabilities["supportsValueFormattingOptions"] = true;
    capabilities["supportsExceptionInfoRequest"] = false;
    capabilities["supportTerminateDebuggee"] = true;
    capabilities["supportsDelayedStackTraceLoading"] = false;
    capabilities["supportsLoadedSourcesRequest"] = false;
    capabilities["supportsLogPoints"] = false;
    capabilities["supportsTerminateThreadsRequest"] = false;
    capabilities["supportsSetExpression"] = false;
    capabilities["supportsTerminateRequest"] = true;
    capabilities["supportsDataBreakpoints"] = false;
    capabilities["supportsReadMemoryRequest"] = false;
    capabilities["supportsDisassembleRequest"] = false;
    capabilities["supportsCancelRequest"] = false;
    capabilities["supportsBreakpointLocationsRequest"] = false;
    capabilities["supportsClipboardContext"] = false;
    capabilities["supportsSteppingGranularity"] = false;
    capabilities["supportsInstructionBreakpoints"] = false;
    capabilities["supportsExceptionFilterOptions"] = false;
    
    response.body = new nlohmann::json(capabilities);
    m_initialized = true;
    
    // Note: initialized event is sent by the run() loop AFTER the response
}

void DAPServer::handleLaunch(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!request.body) {
        response.success = false;
        response.message = "Missing launch arguments";
        return;
    }
    
    const nlohmann::json& args = *request.body;
    std::string program = args["program"];
    
    std::cerr << "[DAP] Launch: " << program << "\n";
    
    // Create target
    lldb::SBError error;
    m_target = m_debugger.CreateTarget(program.c_str(), nullptr, nullptr, false, error);
    
    if (!m_target.IsValid()) {
        response.success = false;
        response.message = "Failed to create target: " + std::string(error.GetCString() ? error.GetCString() : "unknown error");
        return;
    }
    
    // Get launch arguments (keep string storage alive for argv pointers)
    std::vector<std::string> arg_strings;
    arg_strings.push_back(program);
    
    if (args.contains("args") && args["args"].is_array()) {
        for (const auto& arg : args["args"]) {
            arg_strings.push_back(arg.get<std::string>());
        }
    }
    
    std::vector<const char*> argv;
    for (const auto& s : arg_strings) {
        argv.push_back(s.c_str());
    }
    argv.push_back(nullptr);
    
    // Launch process
    // Working directory
    const char* cwd = nullptr;
    std::string cwd_str;
    if (args.contains("cwd") && args["cwd"].is_string()) {
        cwd_str = args["cwd"].get<std::string>();
        cwd = cwd_str.c_str();
    }
    
    // Stop at entry point
    bool stop_at_entry = args.value("stopOnEntry", false);
    
    // Use synchronous mode for launch so process is fully started before we continue
    m_debugger.SetAsync(false);
    
    lldb::SBLaunchInfo launch_info(argv.data());
    launch_info.SetWorkingDirectory(cwd ? cwd : ".");
    // Redirect process I/O away from DAP stdin/stdout
    launch_info.AddOpenFileAction(STDIN_FILENO, "/dev/null", true, false);
    launch_info.AddOpenFileAction(STDOUT_FILENO, "/dev/null", false, true);
    launch_info.AddOpenFileAction(STDERR_FILENO, "/dev/null", false, true);
    if (stop_at_entry) {
        launch_info.SetLaunchFlags(launch_info.GetLaunchFlags() | lldb::eLaunchFlagStopAtEntry);
    }
    
    m_process = m_target.Launch(launch_info, error);
    
    // Restore async mode for interactive debugging
    m_debugger.SetAsync(true);
    
    if (!m_process.IsValid()) {
        response.success = false;
        response.message = "Failed to launch: " + std::string(error.GetCString() ? error.GetCString() : "unknown error");
        return;
    }
    
    // Initialize async debugger (Phase 7.4.5)
    m_async_debugger = std::make_unique<AsyncDebugger>(m_target, m_process);
    
    // Initialize memory visualizer (Phase 7.4.6)
    m_memory_visualizer = std::make_unique<MemoryVisualizer>(m_target, m_process);
    
    // Explicitly subscribe listener to process state change events
    m_process.GetBroadcaster().AddListener(
        m_listener,
        lldb::SBProcess::eBroadcastBitStateChanged |
        lldb::SBProcess::eBroadcastBitSTDOUT |
        lldb::SBProcess::eBroadcastBitSTDERR
    );
    
    // Start event thread
    m_event_thread = std::make_unique<std::thread>(&DAPServer::eventThreadFunc, this);
    
    response.body = new nlohmann::json(nlohmann::json::object());
    
    // If stopOnEntry, send stopped event after responding
    // (LLDB returns process in stopped state — no event is generated for initial stop)
    std::cerr << "[DAP] Process state after launch: " << m_process.GetState() 
              << " stopOnEntry=" << stop_at_entry << "\n";
    if (stop_at_entry && m_process.GetState() == lldb::eStateStopped) {
        lldb::SBThread thread = m_process.GetSelectedThread();
        std::cerr << "[DAP] Thread valid: " << thread.IsValid() 
                  << " tid=" << (thread.IsValid() ? thread.GetThreadID() : 0) << "\n";
        if (thread.IsValid()) {
            nlohmann::json stop_body;
            stop_body["reason"] = "entry";
            stop_body["threadId"] = static_cast<int64_t>(thread.GetThreadID());
            stop_body["allThreadsStopped"] = true;
            // Defer the event so the launch response is sent first
            std::thread([this, stop_body]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                sendEvent("stopped", stop_body);
            }).detach();
        }
    }
}

void DAPServer::handleAttach(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!request.body) {
        response.success = false;
        response.message = "Missing attach arguments";
        return;
    }
    
    const nlohmann::json& args = *request.body;
    
    if (!args.contains("pid")) {
        response.success = false;
        response.message = "Missing 'pid' argument";
        return;
    }
    
    lldb::pid_t pid = args["pid"];
    
    std::cerr << "[DAP] Attach to PID: " << pid << "\n";
    
    // Create target
    lldb::SBError error;
    m_target = m_debugger.CreateTarget("", nullptr, nullptr, false, error);
    
    if (!m_target.IsValid()) {
        response.success = false;
        response.message = "Failed to create target";
        return;
    }
    
    // Attach to process
    lldb::SBAttachInfo attach_info(pid);
    m_process = m_target.Attach(attach_info, error);
    
    if (!m_process.IsValid()) {
        response.success = false;
        response.message = "Failed to attach: " + std::string(error.GetCString() ? error.GetCString() : "unknown error");
        return;
    }
    
    // Initialize async debugger (Phase 7.4.5)
    m_async_debugger = std::make_unique<AsyncDebugger>(m_target, m_process);
    
    // Initialize memory visualizer (Phase 7.4.6)
    m_memory_visualizer = std::make_unique<MemoryVisualizer>(m_target, m_process);
    
    // Explicitly subscribe listener to process state change events
    m_process.GetBroadcaster().AddListener(
        m_listener,
        lldb::SBProcess::eBroadcastBitStateChanged |
        lldb::SBProcess::eBroadcastBitSTDOUT |
        lldb::SBProcess::eBroadcastBitSTDERR
    );
    
    // Start event thread
    m_event_thread = std::make_unique<std::thread>(&DAPServer::eventThreadFunc, this);
    
    response.body = new nlohmann::json(nlohmann::json::object());
}

void DAPServer::handleConfigurationDone(const DAPMessage& request, DAPMessage& response) {
    std::cerr << "[DAP] Configuration done\n";
    response.body = new nlohmann::json(nlohmann::json::object());
    
    // Resume if stopped at entry
    if (m_process.IsValid() && m_process.GetState() == lldb::eStateStopped) {
        m_process.Continue();
    }
}

void DAPServer::handleDisconnect(const DAPMessage& request, DAPMessage& response) {
    std::cerr << "[DAP] Disconnect\n";
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_process.IsValid()) {
        m_process.Destroy();
    }
    
    m_shutdown = true;
    response.body = new nlohmann::json(nlohmann::json::object());
}

void DAPServer::handleSetBreakpoints(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!request.body) {
        response.success = false;
        response.message = "Missing breakpoint arguments";
        return;
    }
    
    const nlohmann::json& args = *request.body;
    std::string source_path = getSourcePath(args["source"]);
    
    std::cerr << "[DAP] Set breakpoints in: " << source_path << "\n";
    
    // Clear existing breakpoints for this file
    for (auto it = m_breakpoints.begin(); it != m_breakpoints.end();) {
        if (it->second.source_path == source_path) {
            if (it->second.lldb_breakpoint.IsValid()) {
                m_target.BreakpointDelete(it->second.lldb_breakpoint.GetID());
            }
            it = m_breakpoints.erase(it);
        } else {
            ++it;
        }
    }
    
    // Set new breakpoints
    nlohmann::json breakpoints = nlohmann::json::array();
    
    if (args.contains("breakpoints") && args["breakpoints"].is_array()) {
        for (const auto& bp_req : args["breakpoints"]) {
            int line = bp_req["line"];
            
            // Create LLDB breakpoint
            lldb::SBBreakpoint lldb_bp = m_target.BreakpointCreateByLocation(
                source_path.c_str(), line);
            
            Breakpoint bp;
            bp.id = m_next_breakpoint_id++;
            bp.source_path = source_path;
            bp.line = line;
            bp.verified = lldb_bp.IsValid() && lldb_bp.GetNumLocations() > 0;
            bp.lldb_breakpoint = lldb_bp;
            
            m_breakpoints[bp.id] = bp;
            
            nlohmann::json bp_json;
            bp_json["id"] = bp.id;
            bp_json["verified"] = bp.verified;
            bp_json["line"] = bp.line;
            
            breakpoints.push_back(bp_json);
            
            std::cerr << "[DAP]   Breakpoint " << bp.id << " at line " << line
                     << " (verified: " << bp.verified << ")\n";
        }
    }
    
    nlohmann::json body;
    body["breakpoints"] = breakpoints;
    response.body = new nlohmann::json(body);
}

void DAPServer::handleSetExceptionBreakpoints(const DAPMessage& request, DAPMessage& response) {
    // Not implemented yet
    response.body = new nlohmann::json(nlohmann::json::object());
}

void DAPServer::handleContinue(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::cerr << "[DAP] Continue\n";
    
    // Clear variable references — they become invalid when execution resumes
    m_var_refs.clear();
    m_next_var_ref = 100000;
    
    if (!m_process.IsValid()) {
        response.success = false;
        response.message = "No process";
        return;
    }
    
    lldb::SBError error = m_process.Continue();
    
    if (error.Fail()) {
        response.success = false;
        response.message = error.GetCString() ? error.GetCString() : "unknown error";
        return;
    }
    
    nlohmann::json body;
    body["allThreadsContinued"] = true;
    response.body = new nlohmann::json(body);
}

void DAPServer::handleNext(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::cerr << "[DAP] Next (step over)\n";
    m_var_refs.clear();
    m_next_var_ref = 100000;
    
    if (!request.body || !(*request.body).contains("threadId")) {
        response.success = false;
        response.message = "Missing threadId";
        return;
    }
    
    lldb::tid_t thread_id = (*request.body)["threadId"];
    lldb::SBThread thread = m_process.GetThreadByID(thread_id);
    
    if (!thread.IsValid()) {
        response.success = false;
        response.message = "Invalid thread";
        return;
    }
    
    thread.StepOver();
    response.body = new nlohmann::json(nlohmann::json::object());
}

void DAPServer::handleStepIn(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::cerr << "[DAP] Step in\n";
    m_var_refs.clear();
    m_next_var_ref = 100000;
    
    if (!request.body || !(*request.body).contains("threadId")) {
        response.success = false;
        response.message = "Missing threadId";
        return;
    }
    
    lldb::tid_t thread_id = (*request.body)["threadId"];
    lldb::SBThread thread = m_process.GetThreadByID(thread_id);
    
    if (!thread.IsValid()) {
        response.success = false;
        response.message = "Invalid thread";
        return;
    }
    
    thread.StepInto();
    response.body = new nlohmann::json(nlohmann::json::object());
}

void DAPServer::handleStepOut(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::cerr << "[DAP] Step out\n";
    m_var_refs.clear();
    m_next_var_ref = 100000;
    
    if (!request.body || !(*request.body).contains("threadId")) {
        response.success = false;
        response.message = "Missing threadId";
        return;
    }
    
    lldb::tid_t thread_id = (*request.body)["threadId"];
    lldb::SBThread thread = m_process.GetThreadByID(thread_id);
    
    if (!thread.IsValid()) {
        response.success = false;
        response.message = "Invalid thread";
        return;
    }
    
    thread.StepOut();
    response.body = new nlohmann::json(nlohmann::json::object());
}

void DAPServer::handlePause(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::cerr << "[DAP] Pause\n";
    
    if (!m_process.IsValid()) {
        response.success = false;
        response.message = "No process";
        return;
    }
    
    lldb::SBError error = m_process.Stop();
    
    if (error.Fail()) {
        response.success = false;
        response.message = error.GetCString() ? error.GetCString() : "unknown error";
        return;
    }
    
    response.body = new nlohmann::json(nlohmann::json::object());
}

void DAPServer::handleThreads(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_process.IsValid()) {
        response.success = false;
        response.message = "No process";
        return;
    }
    
    nlohmann::json threads = nlohmann::json::array();
    
    uint32_t num_threads = m_process.GetNumThreads();
    for (uint32_t i = 0; i < num_threads; ++i) {
        lldb::SBThread thread = m_process.GetThreadAtIndex(i);
        
        if (thread.IsValid()) {
            nlohmann::json thread_json;
            thread_json["id"] = thread.GetThreadID();
            thread_json["name"] = thread.GetName() ? thread.GetName() : "Thread";
            
            threads.push_back(thread_json);
        }
    }
    
    nlohmann::json body;
    body["threads"] = threads;
    response.body = new nlohmann::json(body);
}

void DAPServer::handleStackTrace(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!request.body || !(*request.body).contains("threadId")) {
        response.success = false;
        response.message = "Missing threadId";
        return;
    }
    
    lldb::tid_t thread_id = (*request.body)["threadId"];
    lldb::SBThread thread = m_process.GetThreadByID(thread_id);
    
    if (!thread.IsValid()) {
        response.success = false;
        response.message = "Invalid thread";
        return;
    }
    
    nlohmann::json stack_frames = nlohmann::json::array();
    
    // Get async call stack if async debugger is available (Phase 7.4.5)
    std::vector<AsyncFrameInfo> async_frames;
    if (m_async_debugger) {
        async_frames = m_async_debugger->getAsyncCallStack(thread);
    }
    
    uint32_t num_frames = thread.GetNumFrames();
    for (uint32_t i = 0; i < num_frames; ++i) {
        lldb::SBFrame frame = thread.GetFrameAtIndex(i);
        
        if (frame.IsValid()) {
            lldb::SBLineEntry line_entry = frame.GetLineEntry();
            lldb::SBFileSpec file_spec = line_entry.GetFileSpec();
            
            nlohmann::json frame_json;
            frame_json["id"] = i;  // Frame ID
            frame_json["name"] = frame.GetFunctionName() ? frame.GetFunctionName() : "<unknown>";
            
            if (file_spec.IsValid() && file_spec.GetFilename()) {
                char path[1024];
                file_spec.GetPath(path, sizeof(path));
                
                frame_json["source"] = {
                    {"path", std::string(path)},
                    {"name", file_spec.GetFilename() ? std::string(file_spec.GetFilename()) : std::string(path)}
                };
                frame_json["line"] = line_entry.GetLine();
                frame_json["column"] = line_entry.GetColumn();
            } else {
                frame_json["line"] = 0;
                frame_json["column"] = 0;
            }
            
            // Check if this is an async frame (Phase 7.4.5)
            if (m_async_debugger && m_async_debugger->isAsyncFrame(frame)) {
                // Mark as async frame
                frame_json["presentationHint"] = "label";
                
                // Find corresponding async frame info
                for (const auto& async_frame : async_frames) {
                    if (async_frame.function_name == (frame.GetFunctionName() ? frame.GetFunctionName() : "")) {
                        // Add async-specific metadata
                        std::string status = async_frame.is_suspended ? " [suspended]" : " [running]";
                        frame_json["name"] = frame_json["name"].get<std::string>() + status;
                        
                        // Add Future state if available
                        if (async_frame.future_state == "READY") {
                            frame_json["name"] = frame_json["name"].get<std::string>() + " ✓";
                        } else if (async_frame.future_state == "PENDING") {
                            frame_json["name"] = frame_json["name"].get<std::string>() + " ⏳";
                        } else if (async_frame.future_state == "ERROR") {
                            frame_json["name"] = frame_json["name"].get<std::string>() + " ✗";
                        }
                        
                        // Add awaiter info if suspended
                        if (async_frame.is_suspended && !async_frame.suspend_reason.empty()) {
                            frame_json["name"] = frame_json["name"].get<std::string>() + " - " + async_frame.suspend_reason;
                        }
                        break;
                    }
                }
            }
            
            stack_frames.push_back(frame_json);
        }
    }
    
    nlohmann::json body;
    body["stackFrames"] = stack_frames;
    body["totalFrames"] = num_frames;
    response.body = new nlohmann::json(body);
}

void DAPServer::handleScopes(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!request.body || !(*request.body).contains("frameId")) {
        response.success = false;
        response.message = "Missing frameId";
        return;
    }
    
    int frame_id = (*request.body)["frameId"];
    
    // Get thread and frame
    lldb::SBThread thread = m_process.GetSelectedThread();
    if (!thread.IsValid()) {
        response.success = false;
        response.message = "No selected thread";
        return;
    }
    
    lldb::SBFrame frame = thread.GetFrameAtIndex(frame_id);
    if (!frame.IsValid()) {
        response.success = false;
        response.message = "Invalid frame";
        return;
    }
    
    nlohmann::json scopes = nlohmann::json::array();
    
    // Locals scope
    nlohmann::json locals_scope;
    locals_scope["name"] = "Locals";
    locals_scope["variablesReference"] = frame_id * 1000 + 1;  // Encoded reference
    locals_scope["expensive"] = false;
    scopes.push_back(locals_scope);
    
    // Arguments scope
    nlohmann::json args_scope;
    args_scope["name"] = "Arguments";
    args_scope["variablesReference"] = frame_id * 1000 + 2;
    args_scope["expensive"] = false;
    scopes.push_back(args_scope);
    
    nlohmann::json body;
    body["scopes"] = scopes;
    response.body = new nlohmann::json(body);
}

void DAPServer::handleVariables(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!request.body || !(*request.body).contains("variablesReference")) {
        response.success = false;
        response.message = "Missing variablesReference";
        return;
    }
    
    int var_ref = (*request.body)["variablesReference"];
    
    nlohmann::json variables = nlohmann::json::array();
    
    // Child variable lookup — references >= 100000 are tracked expandable values
    if (var_ref >= 100000) {
        auto it = m_var_refs.find(var_ref);
        if (it == m_var_refs.end()) {
            response.success = false;
            response.message = "Unknown variable reference";
            return;
        }
        
        lldb::SBValue parent = it->second;
        uint32_t num_children = parent.GetNumChildren();
        for (uint32_t i = 0; i < num_children; ++i) {
            lldb::SBValue child = parent.GetChildAtIndex(i);
            if (!child.IsValid()) continue;
            
            nlohmann::json var_json;
            var_json["name"] = child.GetName() ? child.GetName() : "<unnamed>";
            var_json["value"] = child.GetValue() ? child.GetValue() : 
                               (child.GetSummary() ? child.GetSummary() : "<no value>");
            var_json["type"] = child.GetTypeName() ? child.GetTypeName() : "<unknown>";
            
            // Recursively allow expansion of nested composites
            if (child.GetNumChildren() > 0) {
                int child_ref = m_next_var_ref++;
                m_var_refs[child_ref] = child;
                var_json["variablesReference"] = child_ref;
            } else {
                var_json["variablesReference"] = 0;
            }
            
            variables.push_back(var_json);
        }
        
        nlohmann::json body;
        body["variables"] = variables;
        response.body = new nlohmann::json(body);
        return;
    }
    
    // Decode scope reference (frame_id * 1000 + scope)
    int frame_id = var_ref / 1000;
    int scope = var_ref % 1000;
    
    lldb::SBThread thread = m_process.GetSelectedThread();
    if (!thread.IsValid()) {
        response.success = false;
        response.message = "No selected thread";
        return;
    }
    
    lldb::SBFrame frame = thread.GetFrameAtIndex(frame_id);
    if (!frame.IsValid()) {
        response.success = false;
        response.message = "Invalid frame";
        return;
    }
    
    if (scope == 1) {
        // Locals — get all local variables and arguments
        lldb::SBValueList locals = frame.GetVariables(true, true, false, true);
        for (uint32_t i = 0; i < locals.GetSize(); ++i) {
            lldb::SBValue value = locals.GetValueAtIndex(i);
            std::cerr << "[DAP]   local[" << i << "] valid=" << value.IsValid()
                      << " name=" << (value.GetName() ? value.GetName() : "null")
                      << " value=" << (value.GetValue() ? value.GetValue() : "null") << "\n";
            
            if (value.IsValid()) {
                nlohmann::json var_json;
                var_json["name"] = value.GetName() ? value.GetName() : "<unnamed>";
                var_json["value"] = value.GetValue() ? value.GetValue() : "<no value>";
                var_json["type"] = value.GetTypeName() ? value.GetTypeName() : "<unknown>";
                
                if (value.GetNumChildren() > 0) {
                    int ref = m_next_var_ref++;
                    m_var_refs[ref] = value;
                    var_json["variablesReference"] = ref;
                } else {
                    var_json["variablesReference"] = 0;
                }
                
                variables.push_back(var_json);
            }
        }
    } else if (scope == 2) {
        // Arguments
        lldb::SBValueList args = frame.GetVariables(false, true, false, false);
        for (uint32_t i = 0; i < args.GetSize(); ++i) {
            lldb::SBValue value = args.GetValueAtIndex(i);
            
            if (value.IsValid()) {
                nlohmann::json var_json;
                var_json["name"] = value.GetName() ? value.GetName() : "<unnamed>";
                var_json["value"] = value.GetValue() ? value.GetValue() : "<no value>";
                var_json["type"] = value.GetTypeName() ? value.GetTypeName() : "<unknown>";
                
                if (value.GetNumChildren() > 0) {
                    int ref = m_next_var_ref++;
                    m_var_refs[ref] = value;
                    var_json["variablesReference"] = ref;
                } else {
                    var_json["variablesReference"] = 0;
                }
                
                variables.push_back(var_json);
            }
        }
    }
    
    nlohmann::json body;
    body["variables"] = variables;
    response.body = new nlohmann::json(body);
}

void DAPServer::handleEvaluate(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!request.body || !(*request.body).contains("expression")) {
        response.success = false;
        response.message = "Missing expression";
        return;
    }
    
    std::string expression = (*request.body)["expression"];
    int frame_id = (*request.body).value("frameId", 0);
    
    lldb::SBThread thread = m_process.GetSelectedThread();
    if (!thread.IsValid()) {
        response.success = false;
        response.message = "No selected thread";
        return;
    }
    
    lldb::SBFrame frame = thread.GetFrameAtIndex(frame_id);
    if (!frame.IsValid()) {
        response.success = false;
        response.message = "Invalid frame";
        return;
    }
    
    // Evaluate expression
    lldb::SBValue result = frame.EvaluateExpression(expression.c_str());
    
    if (!result.IsValid() || result.GetError().Fail()) {
        response.success = false;
        response.message = result.GetError().GetCString();
        return;
    }
    
    nlohmann::json body;
    body["result"] = result.GetValue() ? result.GetValue() : "<no value>";
    body["type"] = result.GetTypeName() ? result.GetTypeName() : "<unknown>";
    body["variablesReference"] = 0;
    
    response.body = new nlohmann::json(body);
}

// ============================================================================
// Helper Methods
// ============================================================================

std::string DAPServer::getSourcePath(const nlohmann::json& source) {
    if (source.contains("path")) {
        return source["path"];
    }
    return "";
}

// ============================================================================
// Custom Aria DAP Extensions (Phase 7.4.6)
// ============================================================================

void DAPServer::handleAriaMemoryMap(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_memory_visualizer) {
        response.success = false;
        response.message = "Memory visualizer not initialized";
        return;
    }
    
    if (!m_process.IsValid() || m_process.GetState() != lldb::eStateStopped) {
        response.success = false;
        response.message = "Process must be stopped to scan memory";
        return;
    }
    
    // Get filter type from request body
    std::string filter_type = "all";
    if (request.body && (*request.body).contains("type")) {
        filter_type = (*request.body)["type"];
    }
    
    std::cerr << "[DAP] ariaMemoryMap request (filter: " << filter_type << ")\n";
    
    try {
        // Scan memory
        MemoryMap memory_map = m_memory_visualizer->scanMemory();
        
        // Build response JSON
        nlohmann::json body;
        
        // Add regions
        nlohmann::json regions_json = nlohmann::json::array();
        for (const auto& region : memory_map.regions) {
            // Apply filter
            if (filter_type != "all") {
                if (filter_type == "gc" && 
                    region.type != MemoryRegionType::GC_NURSERY && 
                    region.type != MemoryRegionType::GC_OLD_GEN) {
                    continue;
                }
                if (filter_type == "wild" && region.type != MemoryRegionType::WILD) {
                    continue;
                }
                if (filter_type == "wildx" && region.type != MemoryRegionType::WILDX) {
                    continue;
                }
            }
            
            nlohmann::json region_json;
            region_json["start"] = region.start_address;
            region_json["end"] = region.end_address;
            region_json["size"] = region.size;
            region_json["name"] = region.name;
            region_json["is_readable"] = region.is_readable;
            region_json["is_writable"] = region.is_writable;
            region_json["is_executable"] = region.is_executable;
            region_json["object_count"] = region.object_count;
            region_json["fragmentation"] = region.fragmentation_ratio;
            
            // Convert region type to string
            switch (region.type) {
                case MemoryRegionType::GC_NURSERY:
                    region_json["type"] = "gc_nursery";
                    break;
                case MemoryRegionType::GC_OLD_GEN:
                    region_json["type"] = "gc_old_gen";
                    break;
                case MemoryRegionType::WILD:
                    region_json["type"] = "wild";
                    break;
                case MemoryRegionType::WILDX:
                    region_json["type"] = "wildx";
                    break;
                case MemoryRegionType::FREE:
                    region_json["type"] = "free";
                    break;
                default:
                    region_json["type"] = "unknown";
                    break;
            }
            
            regions_json.push_back(region_json);
        }
        body["regions"] = regions_json;
        
        // Add blocks for visualization
        nlohmann::json blocks_json = nlohmann::json::array();
        for (const auto& block : memory_map.blocks) {
            nlohmann::json block_json;
            block_json["address"] = block.address;
            block_json["size"] = block.size;
            block_json["is_allocated"] = block.is_allocated;
            block_json["tooltip"] = block.tooltip;
            
            // Convert block type to string
            switch (block.type) {
                case MemoryRegionType::GC_NURSERY:
                    block_json["type"] = "gc_nursery";
                    break;
                case MemoryRegionType::GC_OLD_GEN:
                    block_json["type"] = "gc_old_gen";
                    break;
                case MemoryRegionType::WILD:
                    block_json["type"] = "wild";
                    break;
                case MemoryRegionType::WILDX:
                    block_json["type"] = "wildx";
                    break;
                default:
                    block_json["type"] = "unknown";
                    break;
            }
            
            blocks_json.push_back(block_json);
        }
        body["blocks"] = blocks_json;
        
        // Add GC statistics
        nlohmann::json stats_json;
        stats_json["nursery_size"] = memory_map.gc_stats.nursery_size;
        stats_json["nursery_used"] = memory_map.gc_stats.nursery_used;
        stats_json["old_gen_size"] = memory_map.gc_stats.old_gen_size;
        stats_json["old_gen_used"] = memory_map.gc_stats.old_gen_used;
        stats_json["total_allocations"] = memory_map.gc_stats.total_allocations;
        stats_json["total_collections"] = memory_map.gc_stats.total_collections;
        stats_json["nursery_collections"] = memory_map.gc_stats.nursery_collections;
        stats_json["old_gen_collections"] = memory_map.gc_stats.old_gen_collections;
        stats_json["fragmentation_percent"] = memory_map.gc_stats.fragmentation_percent;
        stats_json["live_objects"] = memory_map.gc_stats.live_objects;
        stats_json["total_bytes_allocated"] = memory_map.gc_stats.total_bytes_allocated;
        stats_json["total_bytes_freed"] = memory_map.gc_stats.total_bytes_freed;
        body["statistics"] = stats_json;
        
        // Add metadata
        body["heap_start"] = memory_map.heap_start;
        body["heap_end"] = memory_map.heap_end;
        body["total_heap_size"] = memory_map.total_heap_size;
        body["timestamp"] = memory_map.timestamp;
        
        response.body = new nlohmann::json(body);
        
        std::cerr << "[DAP] Memory map generated: " << regions_json.size() 
                  << " regions, " << blocks_json.size() << " blocks\n";
        
    } catch (const std::exception& e) {
        response.success = false;
        response.message = std::string("Memory scan failed: ") + e.what();
        std::cerr << "[DAP] Memory scan error: " << e.what() << "\n";
    }
}

} // namespace debugger
} // namespace aria
