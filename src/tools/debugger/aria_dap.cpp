/**
 * DAP Server Entry Point
 * Phase 7.4.3: Debug Adapter Protocol Server
 * 
 * Standalone executable that implements DAP protocol for debugging Aria programs.
 * Communicates via stdin/stdout using JSON-RPC messages.
 * 
 * Usage:
 *   aria-dap
 * 
 * Typically launched by VS Code or other DAP-compatible editors.
 */

#include "tools/debugger/dap_server.h"
#include <iostream>
#include <csignal>

using namespace aria::debugger;

// Global server instance for signal handling
DAPServer* g_server = nullptr;

void signalHandler(int signum) {
    std::cerr << "[DAP] Signal " << signum << " received, shutting down...\n";
    if (g_server) {
        // Graceful shutdown
        exit(0);
    }
}

int main(int argc, char** argv) {
    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Disable output buffering for immediate logging
    std::cerr.setf(std::ios::unitbuf);
    
    std::cerr << "[DAP] Aria Debug Adapter Protocol Server\n";
    std::cerr << "[DAP] LLDB version: " << lldb::SBDebugger::GetVersionString() << "\n";
    std::cerr << "[DAP] Listening on stdin/stdout...\n";
    
    try {
        // Create and run server
        DAPServer server(0, 1);  // stdin, stdout
        g_server = &server;
        
        int result = server.run();
        
        std::cerr << "[DAP] Server exited with code " << result << "\n";
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "[DAP] Fatal error: " << e.what() << "\n";
        return 1;
    }
}
