/**
 * Web Server Implementation
 * Phase 7.4.4: Browser-based debugger UI
 */

#include "tools/debugger/web_server.h"
#ifdef LLDB_FOUND
#include "tools/debugger/dap_server.h"
#endif
#include <iostream>
#include <fstream>
#include <sstream>

// We'll use a simple embedded HTTP server approach
// For production, this would integrate cpp-httplib or similar

namespace aria {
namespace debugger {

WebServer::WebServer(DAPServer* dap_server, int port, const std::string& host)
    : m_host(host),
      m_port(port),
      m_running(false),
      m_dap_server(dap_server),
      m_next_client_id(1)
{
    // Set default static directory (relative to executable)
    m_static_dir = "./tools/debugger/ui";
}

WebServer::~WebServer() {
    stop();
}

bool WebServer::start() {
    if (m_running) {
        std::cerr << "[WebServer] Already running\n";
        return false;
    }

    std::cerr << "[WebServer] Starting on " << m_host << ":" << m_port << "\n";
    std::cerr << "[WebServer] Static files: " << m_static_dir << "\n";
    
    // For now, just print the URL where the UI would be
    // Full HTTP server implementation would require cpp-httplib or similar library
    std::cerr << "[WebServer] UI would be available at: " << getURL() << "\n";
    std::cerr << "[WebServer] Note: Full HTTP server requires cpp-httplib library\n";
    std::cerr << "[WebServer] For now, use VS Code DAP integration instead\n";
    
    m_running = true;
    return true;
}

void WebServer::stop() {
    if (!m_running) {
        return;
    }

    std::cerr << "[WebServer] Stopping\n";
    
    // Close all WebSocket connections
    {
        std::lock_guard<std::mutex> lock(m_clients_mutex);
        m_ws_clients.clear();
    }
    
    // Stop HTTP server
    if (m_server_thread && m_server_thread->joinable()) {
        m_server_thread->join();
    }
    
    m_running = false;
}

std::string WebServer::getURL() const {
    std::stringstream ss;
    ss << "http://" << m_host << ":" << m_port;
    return ss.str();
}

void WebServer::setStaticDir(const std::string& path) {
    m_static_dir = path;
}

void WebServer::broadcastEvent(const std::string& event_type, const std::string& data) {
    std::lock_guard<std::mutex> lock(m_clients_mutex);
    
    std::cerr << "[WebServer] Broadcasting event: " << event_type << "\n";
    
    // Build JSON event message
    std::stringstream ss;
    ss << "{"
       << "\"type\":\"event\","
       << "\"event\":\"" << event_type << "\","
       << "\"body\":" << data
       << "}";
    
    std::string message = ss.str();
    
    // Send to all active WebSocket clients
    for (auto& pair : m_ws_clients) {
        if (pair.second.active && pair.second.send) {
            pair.second.send(message);
        }
    }
}

void WebServer::setupRoutes() {
    // Routes would be set up here if we had cpp-httplib
    // Example:
    // m_http_server->Get("/", [this](const auto& req, auto& res) {
    //     serveStaticFile(req, res);
    // });
}

void WebServer::handleWebSocketMessage(int client_id, const std::string& message) {
    // Forward DAP message to backend
#ifdef LLDB_FOUND
    if (m_dap_server) {
        // Parse and forward to DAP server
        std::cerr << "[WebServer] Forwarding message to DAP server\n";
    }
#else
    std::cerr << "[WebServer] DAP server not available (LLDB not found)\n";
#endif
}

std::string WebServer::generateDefaultHTML() {
    return R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Aria Debugger</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 0;
            background: #1e1e1e;
            color: #d4d4d4;
        }
        .header {
            background: #2d2d30;
            padding: 20px;
            border-bottom: 1px solid #3e3e42;
        }
        .header h1 {
            margin: 0;
            color: #569cd6;
        }
        .content {
            padding: 40px;
            max-width: 800px;
            margin: 0 auto;
        }
        .info-box {
            background: #252526;
            border: 1px solid #3e3e42;
            border-radius: 4px;
            padding: 20px;
            margin: 20px 0;
        }
        .info-box h2 {
            margin-top: 0;
            color: #4ec9b0;
        }
        .code {
            background: #1e1e1e;
            border: 1px solid #3e3e42;
            padding: 10px;
            border-radius: 4px;
            font-family: 'Courier New', monospace;
            overflow-x: auto;
        }
        .button {
            background: #0e639c;
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 4px;
            cursor: pointer;
            font-size: 14px;
        }
        .button:hover {
            background: #1177bb;
        }
        ul {
            line-height: 1.8;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>🔍 Aria Debugger</h1>
        <p>Browser-based debugging interface</p>
    </div>
    
    <div class="content">
        <div class="info-box">
            <h2>⚠️ Web UI Under Development</h2>
            <p>The web-based debugger UI is currently under development. This page shows the placeholder interface.</p>
            <p><strong>Current Status:</strong> Phase 7.4.4 - Web Server Infrastructure</p>
        </div>
        
        <div class="info-box">
            <h2>✅ Available Now: VS Code Integration</h2>
            <p>You can debug Aria programs right now using VS Code with the Debug Adapter Protocol (DAP) server:</p>
            <ol>
                <li>Install the Aria VS Code extension</li>
                <li>Create a <code>launch.json</code> configuration</li>
                <li>Press F5 to start debugging</li>
            </ol>
            
            <h3>Example launch.json:</h3>
            <div class="code">
{
  "version": "0.2.0",
  "configurations": [{
    "type": "aria",
    "request": "launch",
    "name": "Debug Aria Program",
    "program": "${workspaceFolder}/build/my_program",
    "args": []
  }]
}
            </div>
        </div>
        
        <div class="info-box">
            <h2>🚀 Coming Soon: Full Web UI</h2>
            <p>The complete web-based debugger will include:</p>
            <ul>
                <li>📝 Source code viewer with syntax highlighting</li>
                <li>🔴 Interactive breakpoint management</li>
                <li>🌳 Variable inspector with tree view</li>
                <li>📊 Memory map visualization (GC, Wild, WildX regions)</li>
                <li>⚡ Real-time debugging via WebSocket</li>
                <li>🎨 TBB type visualizations (gauges for symmetric ranges)</li>
                <li>🔍 Async/await call chain debugging</li>
            </ul>
        </div>
        
        <div class="info-box">
            <h2>🛠️ For Developers</h2>
            <p>To enable the full web UI, install cpp-httplib:</p>
            <div class="code">
# Ubuntu/Debian
sudo apt install libhttplib-dev

# Build with web UI support
cmake -DENABLE_WEB_UI=ON -S . -B build
cmake --build build --target aria-dap
            </div>
        </div>
    </div>
</body>
</html>)";
}

std::string WebServer::getMimeType(const std::string& path) {
    // Helper to check if string ends with suffix (C++17 compatible)
    auto endsWith = [](const std::string& str, const std::string& suffix) {
        if (suffix.length() > str.length()) return false;
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    };
    
    if (endsWith(path, ".html")) return "text/html";
    if (endsWith(path, ".css")) return "text/css";
    if (endsWith(path, ".js")) return "application/javascript";
    if (endsWith(path, ".json")) return "application/json";
    if (endsWith(path, ".png")) return "image/png";
    if (endsWith(path, ".jpg") || endsWith(path, ".jpeg")) return "image/jpeg";
    if (endsWith(path, ".svg")) return "image/svg+xml";
    return "text/plain";
}

} // namespace debugger
} // namespace aria
