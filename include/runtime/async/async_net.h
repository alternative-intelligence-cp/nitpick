// Async networking primitives
// TCP, UDP, and HTTP operations

#ifndef ARIA_RUNTIME_ASYNC_NET_H
#define ARIA_RUNTIME_ASYNC_NET_H

#include "runtime/async/future.h"
#include "runtime/async/event_loop.h"
#include <string>
#include <vector>
#include <cstdint>
#include <netinet/in.h>

namespace npk {
namespace runtime {

/**
 * Socket address wrapper
 */
struct SocketAddr {
    std::string host;
    uint16_t port;
    
    SocketAddr(const std::string& h, uint16_t p) : host(h), port(p) {}
};

/**
 * Async TCP socket
 */
class AsyncTcpSocket {
private:
    int fd;
    EventLoop* event_loop;
    bool connected;
    
public:
    AsyncTcpSocket(EventLoop* loop);
    ~AsyncTcpSocket();
    
    /**
     * Connect to remote address
     * @return Future<bool> - true on success
     */
    Future* connect(const SocketAddr& addr);
    
    /**
     * Read data from socket
     * @param buffer Buffer to read into
     * @param size Buffer size
     * @return Future<int64_t> - bytes read (negative on error)
     */
    Future* read(void* buffer, size_t size);
    
    /**
     * Write data to socket
     * @param buffer Data to write
     * @param size Data size
     * @return Future<int64_t> - bytes written (negative on error)
     */
    Future* write(const void* buffer, size_t size);
    
    /**
     * Close socket
     */
    void close();
    
    /**
     * Get socket file descriptor
     */
    int get_fd() const { return fd; }
    
    /**
     * Check if connected
     */
    bool is_connected() const { return connected; }
    
private:
    // For AsyncTcpListener to set up accepted sockets
    friend class AsyncTcpListener;
    void set_fd(int new_fd) { fd = new_fd; }
    void set_connected(bool conn) { connected = conn; }
};

/**
 * Async TCP listener (server)
 */
class AsyncTcpListener {
private:
    int fd;
    EventLoop* event_loop;
    bool listening;
    
public:
    AsyncTcpListener(EventLoop* loop);
    ~AsyncTcpListener();
    
    /**
     * Bind to address and start listening
     * @param addr Address to bind to
     * @param backlog Listen backlog
     * @return true on success
     */
    bool bind_and_listen(const SocketAddr& addr, int backlog = 128);
    
    /**
     * Accept incoming connection
     * @return Future<AsyncTcpSocket*> - new client socket (nullptr on error)
     */
    Future* accept();
    
    /**
     * Stop listening and close
     */
    void close();
    
    /**
     * Get socket file descriptor
     */
    int get_fd() const { return fd; }
    
    /**
     * Check if listening
     */
    bool is_listening() const { return listening; }
};

/**
 * Async UDP socket
 */
class AsyncUdpSocket {
private:
    int fd;
    EventLoop* event_loop;
    bool bound;
    
public:
    AsyncUdpSocket(EventLoop* loop);
    ~AsyncUdpSocket();
    
    /**
     * Bind to local address
     * @param addr Address to bind to
     * @return true on success
     */
    bool bind(const SocketAddr& addr);
    
    /**
     * Send datagram
     * @param buffer Data to send
     * @param size Data size
     * @param addr Destination address
     * @return Future<int64_t> - bytes sent (negative on error)
     */
    Future* send_to(const void* buffer, size_t size, const SocketAddr& addr);
    
    /**
     * Receive datagram
     * @param buffer Buffer to receive into
     * @param size Buffer size
     * @param from Sender address (output)
     * @return Future<int64_t> - bytes received (negative on error)
     */
    Future* recv_from(void* buffer, size_t size, SocketAddr* from);
    
    /**
     * Close socket
     */
    void close();
    
    /**
     * Get socket file descriptor
     */
    int get_fd() const { return fd; }
    
    /**
     * Check if bound
     */
    bool is_bound() const { return bound; }
};

/**
 * HTTP request method
 */
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    PATCH,
    OPTIONS
};

/**
 * HTTP response
 */
struct HttpResponse {
    int status_code;
    std::string status_text;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    
    HttpResponse() : status_code(0) {}
    
    std::string get_body_string() const {
        return std::string(body.begin(), body.end());
    }
};

/**
 * Async HTTP client
 */
class AsyncHttpClient {
private:
    EventLoop* event_loop;
    
public:
    AsyncHttpClient(EventLoop* loop);
    ~AsyncHttpClient();
    
    /**
     * Send HTTP request
     * @param method HTTP method
     * @param url URL to request
     * @param headers Request headers
     * @param body Request body (for POST/PUT)
     * @return Future<HttpResponse*> - response (nullptr on error)
     */
    Future* request(
        HttpMethod method,
        const std::string& url,
        const std::map<std::string, std::string>& headers = {},
        const std::vector<uint8_t>& body = {}
    );
    
    /**
     * GET request
     */
    Future* get(const std::string& url, const std::map<std::string, std::string>& headers = {});
    
    /**
     * POST request
     */
    Future* post(
        const std::string& url,
        const std::vector<uint8_t>& body,
        const std::map<std::string, std::string>& headers = {}
    );
    
    /**
     * PUT request
     */
    Future* put(
        const std::string& url,
        const std::vector<uint8_t>& body,
        const std::map<std::string, std::string>& headers = {}
    );
    
    /**
     * DELETE request
     */
    Future* delete_req(const std::string& url, const std::map<std::string, std::string>& headers = {});
    
private:
    /**
     * Parse HTTP response
     */
    HttpResponse* parse_response(const std::vector<uint8_t>& data);
    
    /**
     * Build HTTP request
     */
    std::vector<uint8_t> build_request(
        HttpMethod method,
        const std::string& path,
        const std::string& host,
        const std::map<std::string, std::string>& headers,
        const std::vector<uint8_t>& body
    );
};

/**
 * Helper functions
 */

// Parse URL into host, port, path
bool parse_url(const std::string& url, std::string& host, uint16_t& port, std::string& path, bool& is_https);

// Resolve hostname to IP address
Future* resolve_host(const std::string& hostname, EventLoop* loop);

// Set socket to non-blocking mode
bool set_nonblocking(int fd);

// Set socket options
bool set_socket_option(int fd, int level, int optname, int value);

} // namespace runtime
} // namespace npk

#endif // ARIA_RUNTIME_ASYNC_NET_H
