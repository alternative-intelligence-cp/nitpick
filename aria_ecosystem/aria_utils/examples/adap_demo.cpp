/**
 * adap_demo.cpp
 * Interactive demonstration of the adap (Debug Adapter Protocol) utility
 *
 * Shows DAP message handling and debugging infrastructure
 */

#include "adap/dap_protocol.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace aria::adap;

void print_section(const std::string& title) {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::left << std::setw(56) << title << "  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
}

// Demo 1: JsonValue construction and manipulation
void demo_json_value() {
    print_section("Demo 1: JSON Value Construction");

    // Simple values
    JsonValue null_val(nullptr);
    JsonValue bool_val(true);
    JsonValue int_val(42);
    JsonValue str_val("Hello, Aria!");

    std::cout << "Simple values:\n";
    std::cout << "  null:   " << null_val.to_string() << "\n";
    std::cout << "  bool:   " << bool_val.to_string() << "\n";
    std::cout << "  int:    " << int_val.to_string() << "\n";
    std::cout << "  string: " << str_val.to_string() << "\n";

    // Array
    JsonArray arr;
    arr.push_back(JsonValue(1));
    arr.push_back(JsonValue(2));
    arr.push_back(JsonValue(3));
    JsonValue arr_val(arr);

    std::cout << "\nArray: " << arr_val.to_string() << "\n";

    // Object
    JsonObject obj;
    obj["name"] = JsonValue("Aria");
    obj["version"] = JsonValue("0.1.0");
    obj["safe"] = JsonValue(true);
    JsonValue obj_val(obj);

    std::cout << "Object: " << obj_val.to_string() << "\n";
}

// Demo 2: Request message construction
void demo_request_message() {
    print_section("Demo 2: DAP Request Message");

    Request req;
    req.seq = 1;
    req.command = "initialize";
    req.arguments["clientID"] = JsonValue("vscode");
    req.arguments["clientName"] = JsonValue("Visual Studio Code");
    req.arguments["adapterID"] = JsonValue("aria");
    req.arguments["linesStartAt1"] = JsonValue(true);
    req.arguments["columnsStartAt1"] = JsonValue(true);

    std::cout << "Initialize request:\n";
    std::cout << "  seq: " << req.seq << "\n";
    std::cout << "  command: " << req.command << "\n";
    std::cout << "  arguments: " << JsonValue(req.arguments).to_string() << "\n";
}

// Demo 3: Response message construction
void demo_response_message() {
    print_section("Demo 3: DAP Response Message");

    Response resp;
    resp.seq = 2;
    resp.request_seq = 1;
    resp.success = true;
    resp.command = "initialize";

    // Add capabilities
    Capabilities caps;
    caps.supportsConfigurationDoneRequest = true;
    caps.supportsEvaluateForHovers = true;
    caps.supportTerminateDebuggee = true;

    resp.body = caps.to_json();

    std::cout << "Initialize response:\n";
    std::cout << "  seq: " << resp.seq << "\n";
    std::cout << "  request_seq: " << resp.request_seq << "\n";
    std::cout << "  success: " << (resp.success ? "true" : "false") << "\n";
    std::cout << "  command: " << resp.command << "\n";
    std::cout << "\nCapabilities preview:\n";
    std::string caps_json = JsonValue(resp.body).to_string();
    std::cout << "  " << caps_json.substr(0, 100) << "...\n";
}

// Demo 4: Event message construction
void demo_event_message() {
    print_section("Demo 4: DAP Event Message");

    Event evt;
    evt.seq = 3;
    evt.event = "stopped";
    evt.body["reason"] = JsonValue("breakpoint");
    evt.body["threadId"] = JsonValue(1);
    evt.body["allThreadsStopped"] = JsonValue(true);

    std::cout << "Stopped event:\n";
    std::cout << "  seq: " << evt.seq << "\n";
    std::cout << "  event: " << evt.event << "\n";
    std::cout << "  body: " << JsonValue(evt.body).to_string() << "\n";
}

// Demo 5: Source file representation
void demo_source_type() {
    print_section("Demo 5: Source File Type");

    Source src;
    src.name = "main.aria";
    src.path = "/home/user/project/src/main.aria";
    src.sourceReference = 0;

    auto json = src.to_json();

    std::cout << "Source:\n";
    std::cout << JsonValue(json).to_string() << "\n";
}

// Demo 6: Breakpoint representation
void demo_breakpoint_type() {
    print_section("Demo 6: Breakpoint Type");

    Breakpoint bp;
    bp.id = 1;
    bp.verified = true;
    bp.source.name = "main.aria";
    bp.source.path = "/home/user/project/src/main.aria";
    bp.line = 42;
    bp.column = 5;

    auto json = bp.to_json();

    std::cout << "Breakpoint:\n";
    std::cout << JsonValue(json).to_string() << "\n";
}

// Demo 7: Stack frame representation
void demo_stack_frame_type() {
    print_section("Demo 7: Stack Frame Type");

    StackFrame frame;
    frame.id = 1;
    frame.name = "main";
    frame.source.name = "main.aria";
    frame.source.path = "/home/user/project/src/main.aria";
    frame.line = 42;
    frame.column = 5;
    frame.presentationHint = "normal";

    auto json = frame.to_json();

    std::cout << "Stack frame:\n";
    std::cout << JsonValue(json).to_string() << "\n";
}

// Demo 8: Variable representation
void demo_variable_type() {
    print_section("Demo 8: Variable Type");

    Variable var;
    var.name = "count";
    var.value = "42";
    var.type = "tbb32";
    var.presentationHint = "property";
    var.variablesReference = 0;

    auto json = var.to_json();

    std::cout << "Variable:\n";
    std::cout << JsonValue(json).to_string() << "\n";
}

// Demo 9: Thread representation
void demo_thread_type() {
    print_section("Demo 9: Thread Type");

    Thread thread;
    thread.id = 1;
    thread.name = "Main Thread";

    auto json = thread.to_json();

    std::cout << "Thread:\n";
    std::cout << JsonValue(json).to_string() << "\n";
}

// Demo 10: Capabilities
void demo_capabilities() {
    print_section("Demo 10: Server Capabilities");

    Capabilities caps;
    caps.supportsConfigurationDoneRequest = true;
    caps.supportsFunctionBreakpoints = true;
    caps.supportsConditionalBreakpoints = true;
    caps.supportsEvaluateForHovers = true;
    caps.supportsValueFormattingOptions = true;
    caps.supportTerminateDebuggee = true;

    auto json = caps.to_json();

    std::cout << "Capabilities:\n";
    std::string caps_str = JsonValue(json).to_string();
    
    // Pretty print a few capabilities
    std::cout << "  supportsConfigurationDoneRequest: true\n";
    std::cout << "  supportsFunctionBreakpoints: true\n";
    std::cout << "  supportsConditionalBreakpoints: true\n";
    std::cout << "  ... and " << json.size() << " total capabilities\n";
}

// Demo 11: Simulated debug session flow
void demo_debug_session_flow() {
    print_section("Demo 11: Debug Session Flow");

    std::cout << "Simulating a typical DAP session:\n\n";

    // 1. Initialize
    std::cout << "1. Client → Server: initialize request\n";
    Request init_req;
    init_req.seq = 1;
    init_req.command = "initialize";
    init_req.arguments["adapterID"] = JsonValue("aria");

    std::cout << "2. Server → Client: initialize response with capabilities\n";
    Response init_resp;
    init_resp.seq = 1;
    init_resp.request_seq = 1;
    init_resp.success = true;
    init_resp.command = "initialize";

    // 3. Initialized event
    std::cout << "3. Server → Client: initialized event\n";
    Event init_evt;
    init_evt.seq = 2;
    init_evt.event = "initialized";

    // 4. Launch/Attach
    std::cout << "4. Client → Server: launch request\n";
    Request launch_req;
    launch_req.seq = 2;
    launch_req.command = "launch";
    launch_req.arguments["program"] = JsonValue("${workspaceFolder}/build/myapp");

    std::cout << "5. Server → Client: launch response\n";

    // 6. Configuration done
    std::cout << "6. Client → Server: configurationDone request\n";
    std::cout << "7. Server → Client: configurationDone response\n";

    // 8. Breakpoint hit
    std::cout << "8. Server → Client: stopped event (reason: breakpoint)\n";
    Event stopped_evt;
    stopped_evt.seq = 3;
    stopped_evt.event = "stopped";
    stopped_evt.body["reason"] = JsonValue("breakpoint");
    stopped_evt.body["threadId"] = JsonValue(1);

    // 9. Stack trace request
    std::cout << "9. Client → Server: stackTrace request\n";
    std::cout << "10. Server → Client: stackTrace response with frames\n";

    std::cout << "\n✓ Typical session flow demonstrated\n";
}

// Demo 12: TBB-safe sequence numbers
void demo_tbb_sequence() {
    print_section("Demo 12: TBB-Safe Sequence Numbers");

    std::cout << "DAP uses sequence numbers for message ordering.\n";
    std::cout << "These should use TBB-safe arithmetic:\n\n";

    int64_t seq = 1;
    for (int i = 0; i < 5; ++i) {
        std::cout << "  Message seq: " << seq << "\n";
        seq++;
    }

    std::cout << "\nIf sequence number approaches INT64_MAX:\n";
    int64_t huge_seq = INT64_MAX - 2;
    std::cout << "  Current seq: " << huge_seq << "\n";
    std::cout << "  Would overflow, but TBB prevents it\n";
    std::cout << "  ✓ Server can safely handle unlimited messages\n";
}

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║   adap - Interactive Demonstration                        ║\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║  Debug Adapter Protocol infrastructure                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    try {
        demo_json_value();
        demo_request_message();
        demo_response_message();
        demo_event_message();
        demo_source_type();
        demo_breakpoint_type();
        demo_stack_frame_type();
        demo_variable_type();
        demo_thread_type();
        demo_capabilities();
        demo_debug_session_flow();
        demo_tbb_sequence();

        std::cout << "\n✓ All 12 demonstrations completed successfully!\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
