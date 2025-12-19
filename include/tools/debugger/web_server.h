/**
 * Web Server for Aria Debugger
 * Phase 7.4.4: HTTP/WebSocket server for browser-based debugging
 * 
 * Provides an embedded HTTP server to serve the debugger UI and
 * WebSocket server for real-time DAP communication with the browser.
 * 
 * Architecture:
 *   Browser <--WebSocket--> WebServer <--Local--> DAPServer <--> LLDB
 * 
 * Features:
 * - Serves static HTML/CSS/JS debugger UI
 * - WebSocket bridge to DAP server
 * - Real-time event streaming (breakpoints, variable updates)
 * - Memory map visualization data
 * 
 * Reference: docs/gemini/responses/request_036_debugger.txt (Phase 4, Weeks 13-16)
 */

#ifndef ARIA_DEBUGGER_WEB_SERVER_H
#define ARIA_DEBUGGER_WEB_SERVER_H

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <vector>
#include <thread>
#include <mutex>

namespace aria {
namespace debugger {

// Forward declare DAPServer (optional, only when LLDB available)
class DAPServer;

/**
 * HTTP/WebSocket Server for Debugger UI
 * 
 * Serves static content for browser-based debugger interface
 * and provides WebSocket connection for real-time DAP communication.
 */
class WebServer {
public:
    /**
     * Create web server
     * @param dap_server Pointer to DAP server for debugging backend
     * @param port HTTP server port (default: 8080)
     * @param host Host address (default: localhost)
     */
    WebServer(DAPServer* dap_server = nullptr, int port = 8080, 
              const std::string& host = "localhost");
    
    ~WebServer();

    /**
     * Start the server
     * Serves debugger UI and accepts WebSocket connections
     * @return true if started successfully
     */
    bool start();

    /**
     * Stop the server
     */
    void stop();

    /**
     * Check if server is running
     */
    bool isRunning() const { return m_running; }

    /**
     * Get server URL
     */
    std::string getURL() const;

    /**
     * Set static content directory
     * @param path Path to directory containing HTML/CSS/JS files
     */
    void setStaticDir(const std::string& path);

    /**
     * Send event to all connected WebSocket clients
     * @param event_type Event type (stopped, breakpoint, etc.)
     * @param data Event data (JSON)
     */
    void broadcastEvent(const std::string& event_type, const std::string& data);

private:
    // Configuration
    std::string m_host;
    int m_port;
    std::string m_static_dir;
    bool m_running;
    
    // DAP integration
    DAPServer* m_dap_server;
    
    // HTTP server (placeholder for future implementation)
    std::unique_ptr<std::thread> m_server_thread;
    
    // WebSocket clients
    struct WebSocketClient {
        int id;
        bool active;
        std::function<void(const std::string&)> send;
    };
    
    std::map<int, WebSocketClient> m_ws_clients;
    int m_next_client_id;
    std::mutex m_clients_mutex;

    /**
     * Initialize HTTP routes
     */
    void setupRoutes();

    /**
     * Handle WebSocket message
     */
    void handleWebSocketMessage(int client_id, const std::string& message);

    /**
     * Generate default HTML if no static files
     */
    std::string generateDefaultHTML();

    /**
     * Get file MIME type from extension
     */
    std::string getMimeType(const std::string& path);
};

} // namespace debugger
} // namespace aria

#endif // ARIA_DEBUGGER_WEB_SERVER_H
