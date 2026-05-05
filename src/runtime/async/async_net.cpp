// Async networking implementation

#include "runtime/async/async_net.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

namespace npk {
namespace runtime {

// Helper to set non-blocking mode
bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool set_socket_option(int fd, int level, int optname, int value) {
    return setsockopt(fd, level, optname, &value, sizeof(value)) == 0;
}

// ============================================================================
// AsyncTcpSocket
// ============================================================================

AsyncTcpSocket::AsyncTcpSocket(EventLoop* loop)
    : fd(-1), event_loop(loop), connected(false) {
    
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    
    set_nonblocking(fd);
    set_socket_option(fd, IPPROTO_TCP, TCP_NODELAY, 1);
}

AsyncTcpSocket::~AsyncTcpSocket() {
    close();
}

Future* AsyncTcpSocket::connect(const SocketAddr& addr) {
    Future* future = new Future(sizeof(bool));
    
    // Resolve address
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(addr.port);
    
    if (inet_pton(AF_INET, addr.host.c_str(), &server_addr.sin_addr) <= 0) {
        // Try DNS lookup
        struct hostent* host = gethostbyname(addr.host.c_str());
        if (!host) {
            bool error = false;
            future->setValue(&error, sizeof(bool));
            return future;
        }
        std::memcpy(&server_addr.sin_addr, host->h_addr, host->h_length);
    }
    
    // Start non-blocking connect
    int result = ::connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (result == 0) {
        // Connected immediately (unlikely but possible)
        connected = true;
        bool success = true;
        future->setValue(&success, sizeof(bool));
        return future;
    }
    
    if (errno != EINPROGRESS) {
        // Connection failed
        bool error = false;
        future->setValue(&error, sizeof(bool));
        return future;
    }
    
    // Connection in progress - wait for writable
    event_loop->add_io_event(fd, EventType::CONNECT, [this, future]() {
        // Check connection result
        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
        
        bool success = (error == 0);
        connected = success;
        
        future->setValue(&success, sizeof(bool));
        
        // Remove event handler
        event_loop->remove_io_event(fd);
    });
    
    return future;
}

Future* AsyncTcpSocket::read(void* buffer, size_t size) {
    Future* future = new Future(sizeof(int64_t));
    
    if (!connected) {
        int64_t error = -1;
        future->setValue(&error, sizeof(int64_t));
        return future;
    }
    
    // Try immediate read
    ssize_t bytes = ::read(fd, buffer, size);
    
    if (bytes > 0) {
        // Data available immediately
        int64_t result = bytes;
        future->setValue(&result, sizeof(int64_t));
        return future;
    }
    
    if (bytes == 0 || (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        // EOF or error
        int64_t error = bytes;
        future->setValue(&error, sizeof(int64_t));
        return future;
    }
    
    // Would block - wait for readable
    event_loop->add_io_event(fd, EventType::READ, [this, buffer, size, future]() {
        ssize_t bytes = ::read(fd, buffer, size);
        int64_t result = bytes;
        future->setValue(&result, sizeof(int64_t));
        
        // Remove event handler
        event_loop->remove_io_event(fd);
    });
    
    return future;
}

Future* AsyncTcpSocket::write(const void* buffer, size_t size) {
    Future* future = new Future(sizeof(int64_t));
    
    if (!connected) {
        int64_t error = -1;
        future->setValue(&error, sizeof(int64_t));
        return future;
    }
    
    // Try immediate write
    ssize_t bytes = ::write(fd, buffer, size);
    
    if (bytes > 0) {
        // Wrote some data immediately
        int64_t result = bytes;
        future->setValue(&result, sizeof(int64_t));
        return future;
    }
    
    if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        // Error
        int64_t error = bytes;
        future->setValue(&error, sizeof(int64_t));
        return future;
    }
    
    // Would block - wait for writable
    event_loop->add_io_event(fd, EventType::WRITE, [this, buffer, size, future]() {
        ssize_t bytes = ::write(fd, buffer, size);
        int64_t result = bytes;
        future->setValue(&result, sizeof(int64_t));
        
        // Remove event handler
        event_loop->remove_io_event(fd);
    });
    
    return future;
}

void AsyncTcpSocket::close() {
    if (fd >= 0) {
        event_loop->remove_io_event(fd);
        ::close(fd);
        fd = -1;
        connected = false;
    }
}

// ============================================================================
// AsyncTcpListener
// ============================================================================

AsyncTcpListener::AsyncTcpListener(EventLoop* loop)
    : fd(-1), event_loop(loop), listening(false) {
    
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("Failed to create listener socket");
    }
    
    set_nonblocking(fd);
    set_socket_option(fd, SOL_SOCKET, SO_REUSEADDR, 1);
}

AsyncTcpListener::~AsyncTcpListener() {
    close();
}

bool AsyncTcpListener::bind_and_listen(const SocketAddr& addr, int backlog) {
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(addr.port);
    
    if (addr.host.empty() || addr.host == "0.0.0.0") {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, addr.host.c_str(), &server_addr.sin_addr) <= 0) {
            return false;
        }
    }
    
    if (::bind(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        return false;
    }
    
    if (::listen(fd, backlog) < 0) {
        return false;
    }
    
    listening = true;
    return true;
}

Future* AsyncTcpListener::accept() {
    Future* future = new Future(sizeof(AsyncTcpSocket*));
    
    if (!listening) {
        AsyncTcpSocket* null_socket = nullptr;
        future->setValue(&null_socket, sizeof(AsyncTcpSocket*));
        return future;
    }
    
    // Try immediate accept
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = ::accept(fd, (struct sockaddr*)&client_addr, &addr_len);
    
    if (client_fd >= 0) {
        // Connection available immediately
        set_nonblocking(client_fd);
        
        AsyncTcpSocket* socket = new AsyncTcpSocket(event_loop);
        socket->set_fd(client_fd);
        socket->set_connected(true);
        
        future->setValue(&socket, sizeof(AsyncTcpSocket*));
        return future;
    }
    
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
        // Error
        AsyncTcpSocket* null_socket = nullptr;
        future->setValue(&null_socket, sizeof(AsyncTcpSocket*));
        return future;
    }
    
    // Would block - wait for readable
    event_loop->add_io_event(fd, EventType::ACCEPT, [this, future]() {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = ::accept(fd, (struct sockaddr*)&client_addr, &addr_len);
        
        AsyncTcpSocket* socket = nullptr;
        
        if (client_fd >= 0) {
            set_nonblocking(client_fd);
            socket = new AsyncTcpSocket(event_loop);
            socket->set_fd(client_fd);
            socket->set_connected(true);
        }
        
        future->setValue(&socket, sizeof(AsyncTcpSocket*));
        
        // Remove event handler
        event_loop->remove_io_event(fd);
    });
    
    return future;
}

void AsyncTcpListener::close() {
    if (fd >= 0) {
        event_loop->remove_io_event(fd);
        ::close(fd);
        fd = -1;
        listening = false;
    }
}

// ============================================================================
// AsyncUdpSocket
// ============================================================================

AsyncUdpSocket::AsyncUdpSocket(EventLoop* loop)
    : fd(-1), event_loop(loop), bound(false) {
    
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        throw std::runtime_error("Failed to create UDP socket");
    }
    
    set_nonblocking(fd);
}

AsyncUdpSocket::~AsyncUdpSocket() {
    close();
}

bool AsyncUdpSocket::bind(const SocketAddr& addr) {
    struct sockaddr_in local_addr;
    std::memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(addr.port);
    
    if (addr.host.empty() || addr.host == "0.0.0.0") {
        local_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, addr.host.c_str(), &local_addr.sin_addr) <= 0) {
            return false;
        }
    }
    
    if (::bind(fd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        return false;
    }
    
    bound = true;
    return true;
}

Future* AsyncUdpSocket::send_to(const void* buffer, size_t size, const SocketAddr& addr) {
    Future* future = new Future(sizeof(int64_t));
    
    struct sockaddr_in dest_addr;
    std::memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(addr.port);
    
    if (inet_pton(AF_INET, addr.host.c_str(), &dest_addr.sin_addr) <= 0) {
        int64_t error = -1;
        future->setValue(&error, sizeof(int64_t));
        return future;
    }
    
    ssize_t bytes = sendto(fd, buffer, size, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    int64_t result = bytes;
    future->setValue(&result, sizeof(int64_t));
    
    return future;
}

Future* AsyncUdpSocket::recv_from(void* buffer, size_t size, SocketAddr* from) {
    Future* future = new Future(sizeof(int64_t));
    
    if (!bound) {
        int64_t error = -1;
        future->setValue(&error, sizeof(int64_t));
        return future;
    }
    
    // Try immediate receive
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);
    ssize_t bytes = recvfrom(fd, buffer, size, 0, (struct sockaddr*)&sender_addr, &addr_len);
    
    if (bytes > 0) {
        // Data available immediately
        if (from) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sender_addr.sin_addr, ip, INET_ADDRSTRLEN);
            from->host = ip;
            from->port = ntohs(sender_addr.sin_port);
        }
        
        int64_t result = bytes;
        future->setValue(&result, sizeof(int64_t));
        return future;
    }
    
    if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        // Error
        int64_t error = bytes;
        future->setValue(&error, sizeof(int64_t));
        return future;
    }
    
    // Would block - wait for readable
    event_loop->add_io_event(fd, EventType::READ, [this, buffer, size, from, future]() {
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);
        ssize_t bytes = recvfrom(fd, buffer, size, 0, (struct sockaddr*)&sender_addr, &addr_len);
        
        if (bytes > 0 && from) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sender_addr.sin_addr, ip, INET_ADDRSTRLEN);
            from->host = ip;
            from->port = ntohs(sender_addr.sin_port);
        }
        
        int64_t result = bytes;
        future->setValue(&result, sizeof(int64_t));
        
        // Remove event handler
        event_loop->remove_io_event(fd);
    });
    
    return future;
}

void AsyncUdpSocket::close() {
    if (fd >= 0) {
        event_loop->remove_io_event(fd);
        ::close(fd);
        fd = -1;
        bound = false;
    }
}

// ============================================================================
// AsyncHttpClient (simplified implementation)
// ============================================================================

AsyncHttpClient::AsyncHttpClient(EventLoop* loop)
    : event_loop(loop) {
}

AsyncHttpClient::~AsyncHttpClient() {
}

Future* AsyncHttpClient::get(const std::string& url, const std::map<std::string, std::string>& headers) {
    return request(HttpMethod::GET, url, headers);
}

Future* AsyncHttpClient::post(
    const std::string& url,
    const std::vector<uint8_t>& body,
    const std::map<std::string, std::string>& headers) {
    return request(HttpMethod::POST, url, headers, body);
}

Future* AsyncHttpClient::request(
    HttpMethod method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::vector<uint8_t>& body) {
    
    Future* future = new Future(sizeof(HttpResponse*));
    
    // Parse URL
    std::string host, path;
    uint16_t port;
    bool is_https;
    
    if (!parse_url(url, host, port, path, is_https)) {
        HttpResponse* null_response = nullptr;
        future->setValue(&null_response, sizeof(HttpResponse*));
        return future;
    }
    
    // Create socket and connect
    AsyncTcpSocket* socket = new AsyncTcpSocket(event_loop);
    Future* connect_future = socket->connect(SocketAddr(host, port));
    
    // Chain: connect -> send request -> receive response
    event_loop->post_task([socket, connect_future, future, method, path, host, headers, body, this]() {
        // Wait for connection
        bool connected = false;
        if (connect_future->isReady()) {
            connected = *(bool*)connect_future->getValue();
        }
        delete connect_future;
        
        if (!connected) {
            HttpResponse* null_response = nullptr;
            future->setValue(&null_response, sizeof(HttpResponse*));
            delete socket;
            return;
        }
        
        // Build request
        auto request_data = build_request(method, path, host, headers, body);
        
        // Send request
        Future* write_future = socket->write(request_data.data(), request_data.size());
        
        // Wait for write to complete
        while (!write_future->isReady()) {
            write_future->poll();
        }
        delete write_future;
        
        // Read HTTP response
        HttpResponse* response = new HttpResponse();
        
        // Read response data in chunks
        std::vector<uint8_t> raw_response;
        const size_t read_buf_size = 4096;
        uint8_t read_buf[4096];
        
        while (true) {
            Future* read_future = socket->read(read_buf, read_buf_size);
            while (!read_future->isReady()) {
                read_future->poll();
            }
            
            int64_t bytes_read = *(int64_t*)read_future->getValue();
            delete read_future;
            
            if (bytes_read <= 0) {
                break;  // Connection closed or error
            }
            
            raw_response.insert(raw_response.end(), read_buf, read_buf + bytes_read);
            
            // Check if we have complete headers (look for \r\n\r\n)
            // and if Content-Length body is fully received
            std::string resp_str(raw_response.begin(), raw_response.end());
            size_t header_end = resp_str.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                // Parse Content-Length to see if body is complete
                size_t body_start = header_end + 4;
                std::string header_section = resp_str.substr(0, header_end);
                
                // Check for Content-Length
                size_t cl_pos = header_section.find("Content-Length: ");
                if (cl_pos == std::string::npos) {
                    cl_pos = header_section.find("content-length: ");
                }
                
                if (cl_pos != std::string::npos) {
                    size_t cl_end = header_section.find("\r\n", cl_pos);
                    std::string cl_val = header_section.substr(cl_pos + 16, cl_end - cl_pos - 16);
                    size_t content_length = std::stoull(cl_val);
                    
                    if (raw_response.size() >= body_start + content_length) {
                        break;  // Full response received
                    }
                } else {
                    // No Content-Length — for HTTP/1.1, read until connection close
                    // For simplicity, if we got headers and some data, check for
                    // Transfer-Encoding: chunked or just break on small reads
                    if (static_cast<size_t>(bytes_read) < read_buf_size) {
                        break;  // Partial read suggests end of data
                    }
                }
            }
        }
        
        // Parse the HTTP response
        std::string resp_str(raw_response.begin(), raw_response.end());
        size_t header_end = resp_str.find("\r\n\r\n");
        
        if (header_end != std::string::npos) {
            // Parse status line: "HTTP/1.1 200 OK\r\n"
            size_t first_line_end = resp_str.find("\r\n");
            if (first_line_end != std::string::npos) {
                std::string status_line = resp_str.substr(0, first_line_end);
                size_t sp1 = status_line.find(' ');
                if (sp1 != std::string::npos) {
                    size_t sp2 = status_line.find(' ', sp1 + 1);
                    if (sp2 != std::string::npos) {
                        response->status_code = std::stoi(status_line.substr(sp1 + 1, sp2 - sp1 - 1));
                        response->status_text = status_line.substr(sp2 + 1);
                    }
                }
            }
            
            // Parse headers
            size_t pos = first_line_end + 2;
            while (pos < header_end) {
                size_t line_end = resp_str.find("\r\n", pos);
                if (line_end == std::string::npos || line_end > header_end) break;
                
                std::string header_line = resp_str.substr(pos, line_end - pos);
                size_t colon = header_line.find(": ");
                if (colon != std::string::npos) {
                    response->headers[header_line.substr(0, colon)] = header_line.substr(colon + 2);
                }
                pos = line_end + 2;
            }
            
            // Extract body
            size_t body_start = header_end + 4;
            if (body_start < raw_response.size()) {
                response->body.assign(raw_response.begin() + body_start, raw_response.end());
            }
        } else if (!raw_response.empty()) {
            // Couldn't parse headers — store raw data as body
            response->status_code = 0;
            response->status_text = "Malformed Response";
            response->body = raw_response;
        } else {
            response->status_code = 0;
            response->status_text = "Empty Response";
        }
        
        future->setValue(&response, sizeof(HttpResponse*));
        
        delete socket;
    });
    
    return future;
}

std::vector<uint8_t> AsyncHttpClient::build_request(
    HttpMethod method,
    const std::string& path,
    const std::string& host,
    const std::map<std::string, std::string>& headers,
    const std::vector<uint8_t>& body) {
    
    std::string request;
    
    // Request line
    switch (method) {
        case HttpMethod::GET:    request += "GET "; break;
        case HttpMethod::POST:   request += "POST "; break;
        case HttpMethod::PUT:    request += "PUT "; break;
        case HttpMethod::DELETE: request += "DELETE "; break;
        case HttpMethod::HEAD:   request += "HEAD "; break;
        case HttpMethod::PATCH:  request += "PATCH "; break;
        case HttpMethod::OPTIONS:request += "OPTIONS "; break;
    }
    
    request += path + " HTTP/1.1\r\n";
    
    // Host header
    request += "Host: " + host + "\r\n";
    
    // Custom headers
    for (const auto& pair : headers) {
        request += pair.first + ": " + pair.second + "\r\n";
    }
    
    // Content-Length
    if (!body.empty()) {
        request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    
    // End headers
    request += "\r\n";
    
    // Add body
    std::vector<uint8_t> result(request.begin(), request.end());
    result.insert(result.end(), body.begin(), body.end());
    
    return result;
}

bool parse_url(const std::string& url, std::string& host, uint16_t& port, std::string& path, bool& is_https) {
    // Simple URL parser: http[s]://host[:port]/path
    size_t pos = 0;
    
    // Protocol
    if (url.find("https://") == 0) {
        is_https = true;
        pos = 8;
        port = 443;
    } else if (url.find("http://") == 0) {
        is_https = false;
        pos = 7;
        port = 80;
    } else {
        return false;
    }
    
    // Host and port
    size_t path_start = url.find('/', pos);
    if (path_start == std::string::npos) {
        path_start = url.length();
        path = "/";
    } else {
        path = url.substr(path_start);
    }
    
    std::string host_port = url.substr(pos, path_start - pos);
    
    size_t colon = host_port.find(':');
    if (colon != std::string::npos) {
        host = host_port.substr(0, colon);
        port = std::stoi(host_port.substr(colon + 1));
    } else {
        host = host_port;
    }
    
    return true;
}

} // namespace runtime
} // namespace npk
