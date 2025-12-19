/**
 * DAP Server Unit Tests
 * Phase 7.4.3: Debug Adapter Protocol
 * 
 * Tests basic DAP message parsing and protocol handling.
 * Full integration tests require LLDB and sample programs.
 */

#include "test_helpers.h"

#ifdef LLDB_FOUND
#include "tools/debugger/dap_server.h"
#include <nlohmann/json.hpp>

using namespace aria::debugger;
using json = nlohmann::json;

// Test DAP message structure
TEST_CASE(dap_message_lifecycle) {
    DAPMessage msg;
    msg.type = DAPMessageType::REQUEST;
    msg.seq = 1;
    msg.command = "initialize";
    msg.body = new json({{"clientID", "vscode"}});
    
    ASSERT(msg.seq == 1, "Message sequence number");
    ASSERT(msg.command == "initialize", "Message command");
    ASSERT(msg.body != nullptr, "Message body not null");
    ASSERT((*msg.body)["clientID"] == "vscode", "Client ID field");
    
    // Destructor should clean up body
}

// Test breakpoint structure
TEST_CASE(dap_breakpoint_info) {
    Breakpoint bp;
    bp.id = 42;
    bp.source_path = "/path/to/file.aria";
    bp.line = 10;
    bp.verified = true;
    
    ASSERT(bp.id == 42, "Breakpoint ID");
    ASSERT(bp.source_path == "/path/to/file.aria", "Breakpoint source path");
    ASSERT(bp.line == 10, "Breakpoint line");
    ASSERT(bp.verified == true, "Breakpoint verified");
}

// Test stack frame structure
TEST_CASE(dap_stack_frame_info) {
    StackFrame frame;
    frame.id = 0;
    frame.name = "main";
    frame.source_path = "/path/to/main.aria";
    frame.line = 5;
    frame.column = 1;
    
    ASSERT(frame.id == 0, "Frame ID");
    ASSERT(frame.name == "main", "Frame name");
    ASSERT(frame.line == 5, "Frame line");
}

// Test variable structure
TEST_CASE(dap_variable_info) {
    Variable var;
    var.name = "x";
    var.value = "42";
    var.type = "tbb32";
    var.variablesReference = 0;
    
    ASSERT(var.name == "x", "Variable name");
    ASSERT(var.value == "42", "Variable value");
    ASSERT(var.type == "tbb32", "Variable type");
    ASSERT(var.variablesReference == 0, "Variables reference");
}

// Test JSON serialization of DAP capabilities
TEST_CASE(dap_capabilities_json) {
    json capabilities;
    capabilities["supportsConfigurationDoneRequest"] = true;
    capabilities["supportsEvaluateForHovers"] = true;
    capabilities["supportsStepBack"] = false;
    capabilities["supportTerminateDebuggee"] = true;
    
    ASSERT(capabilities["supportsConfigurationDoneRequest"] == true, "Configuration done support");
    ASSERT(capabilities["supportsEvaluateForHovers"] == true, "Evaluate for hovers support");
    ASSERT(capabilities["supportsStepBack"] == false, "Step back not supported");
    ASSERT(capabilities["supportTerminateDebuggee"] == true, "Terminate debuggee support");
    
    // Verify JSON dump works
    std::string json_str = capabilities.dump();
    ASSERT(json_str.find("supportsConfigurationDoneRequest") != std::string::npos, "JSON contains capability");
}

// Test DAP request parsing
TEST_CASE(dap_request_parsing) {
    std::string request_json = R"({
        "seq": 1,
        "type": "request",
        "command": "initialize",
        "arguments": {
            "clientID": "vscode",
            "adapterID": "aria"
        }
    })";
    
    json j = json::parse(request_json);
    
    ASSERT(j["seq"] == 1, "Request sequence");
    ASSERT(j["type"] == "request", "Request type");
    ASSERT(j["command"] == "initialize", "Request command");
    ASSERT(j["arguments"]["clientID"] == "vscode", "Client ID argument");
    ASSERT(j["arguments"]["adapterID"] == "aria", "Adapter ID argument");
}

// Test DAP response formatting
TEST_CASE(dap_response_formatting) {
    json response;
    response["seq"] = 2;
    response["type"] = "response";
    response["request_seq"] = 1;
    response["command"] = "initialize";
    response["success"] = true;
    response["body"] = {
        {"supportsConfigurationDoneRequest", true}
    };
    
    ASSERT(response["success"] == true, "Response success");
    ASSERT(response["body"]["supportsConfigurationDoneRequest"] == true, "Response body capability");
    
    std::string response_str = response.dump();
    ASSERT(response_str.find("\"success\":true") != std::string::npos, "JSON contains success field");
}

// Test DAP event formatting
TEST_CASE(dap_event_formatting) {
    json event;
    event["seq"] = 10;
    event["type"] = "event";
    event["event"] = "stopped";
    event["body"] = {
        {"reason", "breakpoint"},
        {"threadId", 123},
        {"allThreadsStopped", true}
    };
    
    ASSERT(event["type"] == "event", "Event type");
    ASSERT(event["event"] == "stopped", "Event name");
    ASSERT(event["body"]["reason"] == "breakpoint", "Event reason");
    ASSERT(event["body"]["threadId"] == 123, "Thread ID");
}

// Test breakpoint request parsing
TEST_CASE(dap_breakpoint_request) {
    json request;
    request["command"] = "setBreakpoints";
    request["arguments"] = {
        {"source", {{"path", "/path/to/file.aria"}}},
        {"breakpoints", json::array({
            {{"line", 10}},
            {{"line", 20}},
            {{"line", 30}}
        })}
    };
    
    ASSERT(request["arguments"]["source"]["path"] == "/path/to/file.aria", "Source path");
    ASSERT(request["arguments"]["breakpoints"].is_array(), "Breakpoints is array");
    ASSERT(request["arguments"]["breakpoints"].size() == 3, "Three breakpoints");
    ASSERT(request["arguments"]["breakpoints"][0]["line"] == 10, "First breakpoint line");
    ASSERT(request["arguments"]["breakpoints"][2]["line"] == 30, "Third breakpoint line");
}

// Test stack trace response formatting
TEST_CASE(dap_stack_trace_response) {
    json response;
    response["success"] = true;
    response["body"] = {
        {"stackFrames", json::array({
            {
                {"id", 0},
                {"name", "main"},
                {"source", {{"path", "/path/to/main.aria"}}},
                {"line", 15},
                {"column", 5}
            },
            {
                {"id", 1},
                {"name", "foo"},
                {"source", {{"path", "/path/to/utils.aria"}}},
                {"line", 42},
                {"column", 10}
            }
        })},
        {"totalFrames", 2}
    };
    
    ASSERT(response["body"]["stackFrames"].size() == 2, "Two stack frames");
    ASSERT(response["body"]["stackFrames"][0]["name"] == "main", "First frame is main");
    ASSERT(response["body"]["stackFrames"][1]["line"] == 42, "Second frame line 42");
    ASSERT(response["body"]["totalFrames"] == 2, "Total frames count");
}

#else
// Stub test when LLDB not available
TEST_CASE(dap_lldb_not_available) {
    ASSERT(true, "DAP server requires LLDB");
}
#endif
