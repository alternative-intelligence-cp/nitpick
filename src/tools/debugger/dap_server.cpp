/**
 * Debug Adapter Protocol Server Implementation
 * Phase 7.4.3: DAP-to-LLDB bridge
 */

#include "tools/debugger/dap_server.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <unistd.h>

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
        response.seq = m_next_seq++;
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
    }
    
    std::cerr << "[DAP] Server exiting\n";
    return 0;
}

std::unique_ptr<DAPMessage> DAPServer::readMessage() {
    // Read Content-Length header
    std::string header;
    std::getline(std::cin, header);
    
    if (std::cin.eof()) {
        return nullptr;
    }
    
    // Parse Content-Length
    size_t colon = header.find(':');
    if (colon == std::string::npos) {
        std::cerr << "[DAP] Invalid header: " << header << "\n";
        return nullptr;
    }
    
    int content_length = std::stoi(header.substr(colon + 1));
    
    // Skip empty line
    std::string empty;
    std::getline(std::cin, empty);
    
    // Read content
    std::string content;
    content.resize(content_length);
    std::cin.read(&content[0], content_length);
    
    if (std::cin.gcount() != content_length) {
        std::cerr << "[DAP] Short read\n";
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
    nlohmann::json j;
    j["seq"] = msg.seq;
    
    if (msg.type == DAPMessageType::RESPONSE) {
        j["type"] = "response";
        j["request_seq"] = msg.seq - 1;  // Simplified
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
    response.seq = m_next_seq++;
    response.command = command;
    response.success = success;
    response.message = message;
    response.body = new nlohmann::json(body);
    
    writeMessage(response);
}

void DAPServer::sendEvent(const std::string& event, const nlohmann::json& body) {
    DAPMessage evt;
    evt.type = DAPMessageType::EVENT;
    evt.seq = m_next_seq++;
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
                
                switch (state) {
                case lldb::eStateStopped: {
                    // Thread stopped - send stopped event
                    lldb::SBThread thread = m_process.GetSelectedThread();
                    if (thread.IsValid()) {
                        nlohmann::json body;
                        body["reason"] = "breakpoint";  // Could be step, pause, etc.
                        body["threadId"] = thread.GetThreadID();
                        body["allThreadsStopped"] = true;
                        
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
    
    // Send initialized event
    sendEvent("initialized", nlohmann::json::object());
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
        response.message = "Failed to create target: " + std::string(error.GetCString());
        return;
    }
    
    // Get launch arguments
    std::vector<const char*> argv;
    argv.push_back(program.c_str());
    
    if (args.contains("args") && args["args"].is_array()) {
        for (const auto& arg : args["args"]) {
            argv.push_back(arg.get<std::string>().c_str());
        }
    }
    argv.push_back(nullptr);
    
    // Launch process
    // Note: LLDB 20 GetListener() returns by value, not reference
    lldb::SBListener listener = m_debugger.GetListener();
    m_process = m_target.Launch(
        listener,
        argv.data(),
        nullptr,  // envp
        nullptr,  // stdin
        nullptr,  // stdout
        nullptr,  // stderr
        nullptr,  // working directory
        0,        // launch flags
        false,    // stop at entry
        error
    );
    
    if (!m_process.IsValid()) {
        response.success = false;
        response.message = "Failed to launch: " + std::string(error.GetCString());
        return;
    }
    
    // Start event thread
    m_event_thread = std::make_unique<std::thread>(&DAPServer::eventThreadFunc, this);
    
    response.body = new nlohmann::json(nlohmann::json::object());
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
        response.message = "Failed to attach: " + std::string(error.GetCString());
        return;
    }
    
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
    
    if (!m_process.IsValid()) {
        response.success = false;
        response.message = "No process";
        return;
    }
    
    lldb::SBError error = m_process.Continue();
    
    if (error.Fail()) {
        response.success = false;
        response.message = error.GetCString();
        return;
    }
    
    nlohmann::json body;
    body["allThreadsContinued"] = true;
    response.body = new nlohmann::json(body);
}

void DAPServer::handleNext(const DAPMessage& request, DAPMessage& response) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::cerr << "[DAP] Next (step over)\n";
    
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
        response.message = error.GetCString();
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
    
    uint32_t num_frames = thread.GetNumFrames();
    for (uint32_t i = 0; i < num_frames; ++i) {
        lldb::SBFrame frame = thread.GetFrameAtIndex(i);
        
        if (frame.IsValid()) {
            lldb::SBLineEntry line_entry = frame.GetLineEntry();
            lldb::SBFileSpec file_spec = line_entry.GetFileSpec();
            
            nlohmann::json frame_json;
            frame_json["id"] = i;  // Frame ID
            frame_json["name"] = frame.GetFunctionName() ? frame.GetFunctionName() : "<unknown>";
            
            if (file_spec.IsValid()) {
                char path[1024];
                file_spec.GetPath(path, sizeof(path));
                
                frame_json["source"] = {
                    {"path", path},
                    {"name", file_spec.GetFilename()}
                };
                frame_json["line"] = line_entry.GetLine();
                frame_json["column"] = line_entry.GetColumn();
            } else {
                frame_json["line"] = 0;
                frame_json["column"] = 0;
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
    
    // Decode reference (frame_id * 1000 + scope)
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
    
    nlohmann::json variables = nlohmann::json::array();
    
    if (scope == 1) {
        // Locals
        lldb::SBValueList locals = frame.GetVariables(true, false, false, false);
        for (uint32_t i = 0; i < locals.GetSize(); ++i) {
            lldb::SBValue value = locals.GetValueAtIndex(i);
            
            if (value.IsValid()) {
                nlohmann::json var_json;
                var_json["name"] = value.GetName() ? value.GetName() : "<unnamed>";
                var_json["value"] = value.GetValue() ? value.GetValue() : "<no value>";
                var_json["type"] = value.GetTypeName() ? value.GetTypeName() : "<unknown>";
                var_json["variablesReference"] = 0;  // TODO: Handle children
                
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
                var_json["variablesReference"] = 0;
                
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

} // namespace debugger
} // namespace aria
