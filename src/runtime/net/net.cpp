/**
 * Network I/O runtime for Aria stdlib.
 *
 * Provides TCP client and server socket operations.
 *
 * API:
 *  Client:
 *    net_connect(host, port)             → handle  TCP connect
 *    net_send(h, data)                   → int64   send string, returns bytes sent
 *    net_recv(h, max_bytes)              → string  receive data
 *    net_close(h)                        → void    close socket
 *
 *  Server:
 *    net_listen(port, backlog)           → handle  bind + listen
 *    net_accept(h)                       → handle  accept connection
 *    net_server_close(h)                 → void    close server socket
 *
 *  Info:
 *    net_peer_addr(h)                    → string  remote IP address
 *    net_peer_port(h)                    → int32   remote port
 *    net_is_connected(h)                 → int32   1 if connected
 *
 * ABI: string params as const char*, string returns as AriaString*.
 */

#include "runtime/strings.h"
#include "runtime/gc.h"
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>

// ═══════════════════════════════════════════════════════════════════════
// Internal structures
// ═══════════════════════════════════════════════════════════════════════

struct NetSocket {
    int fd;
    struct sockaddr_in peer_addr;
    int connected;
};

struct NetServer {
    int fd;
    int port;
};

static AriaString* make_aria_string(const char* data, int64_t length) {
    char* buf = (char*)npk_gc_alloc(length + 1, 0);
    if (!buf) std::abort();
    if (length > 0) memcpy(buf, data, length);
    buf[length] = '\0';
    AriaString* s = (AriaString*)npk_gc_alloc(sizeof(AriaString), 0);
    if (!s) std::abort();
    s->data = buf;
    s->length = length;
    return s;
}

// ═══════════════════════════════════════════════════════════════════════
// Public C API
// ═══════════════════════════════════════════════════════════════════════

extern "C" {

// ── Client ───────────────────────────────────────────────────────────

void* net_connect(const char* host, int32_t port) {
    if (!host) return nullptr;

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int gai_err = getaddrinfo(host, port_str, &hints, &res);
    if (gai_err != 0) return nullptr;

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return nullptr; }

    int ret = connect(fd, res->ai_addr, res->ai_addrlen);
    if (ret < 0) { close(fd); freeaddrinfo(res); return nullptr; }

    NetSocket* s = (NetSocket*)calloc(1, sizeof(NetSocket));
    if (!s) { close(fd); freeaddrinfo(res); std::abort(); }
    s->fd = fd;
    s->connected = 1;
    if (res->ai_family == AF_INET)
        memcpy(&s->peer_addr, res->ai_addr, sizeof(struct sockaddr_in));

    freeaddrinfo(res);
    return (void*)s;
}

int64_t net_send(void* h, const char* data) {
    if (!h || !data) return -1;
    NetSocket* s = (NetSocket*)h;
    if (!s->connected) return -1;
    int64_t len = (int64_t)strlen(data);
    ssize_t sent = send(s->fd, data, len, MSG_NOSIGNAL);
    return (int64_t)sent;
}

AriaString* net_recv(void* h, int64_t max_bytes) {
    if (!h) return make_aria_string("", 0);
    NetSocket* s = (NetSocket*)h;
    if (!s->connected || max_bytes <= 0) return make_aria_string("", 0);
    if (max_bytes > 1048576) max_bytes = 1048576; // 1MB cap

    char* buf = (char*)malloc(max_bytes + 1);
    if (!buf) return make_aria_string("", 0);

    ssize_t n = recv(s->fd, buf, max_bytes, 0);
    if (n <= 0) {
        free(buf);
        if (n == 0) s->connected = 0;
        return make_aria_string("", 0);
    }

    AriaString* result = make_aria_string(buf, (int64_t)n);
    free(buf);
    return result;
}

void net_close(void* h) {
    if (!h) return;
    NetSocket* s = (NetSocket*)h;
    if (s->fd >= 0) close(s->fd);
    s->connected = 0;
    s->fd = -1;
    free(s);
}

// ── Server ───────────────────────────────────────────────────────────

void* net_listen(int32_t port, int32_t backlog) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return nullptr;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return nullptr;
    }

    if (backlog <= 0) backlog = 16;
    if (listen(fd, backlog) < 0) {
        close(fd);
        return nullptr;
    }

    NetServer* srv = (NetServer*)calloc(1, sizeof(NetServer));
    if (!srv) { close(fd); std::abort(); }
    srv->fd = fd;
    srv->port = port;
    return (void*)srv;
}

void* net_accept(void* h) {
    if (!h) return nullptr;
    NetServer* srv = (NetServer*)h;

    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int client_fd = accept(srv->fd, (struct sockaddr*)&client_addr, &addrlen);
    if (client_fd < 0) return nullptr;

    NetSocket* s = (NetSocket*)calloc(1, sizeof(NetSocket));
    if (!s) { close(client_fd); std::abort(); }
    s->fd = client_fd;
    s->connected = 1;
    memcpy(&s->peer_addr, &client_addr, sizeof(client_addr));
    return (void*)s;
}

void net_server_close(void* h) {
    if (!h) return;
    NetServer* srv = (NetServer*)h;
    if (srv->fd >= 0) close(srv->fd);
    srv->fd = -1;
    free(srv);
}

// ── Info ─────────────────────────────────────────────────────────────

AriaString* net_peer_addr(void* h) {
    if (!h) return make_aria_string("", 0);
    NetSocket* s = (NetSocket*)h;
    char buf[INET_ADDRSTRLEN];
    const char* result = inet_ntop(AF_INET, &s->peer_addr.sin_addr, buf, sizeof(buf));
    if (!result) return make_aria_string("", 0);
    return make_aria_string(result, (int64_t)strlen(result));
}

int32_t net_peer_port(void* h) {
    if (!h) return 0;
    NetSocket* s = (NetSocket*)h;
    return (int32_t)ntohs(s->peer_addr.sin_port);
}

int32_t net_is_connected(void* h) {
    if (!h) return 0;
    return ((NetSocket*)h)->connected;
}

int32_t net_valid(void* h) {
    return h ? 1 : 0;
}

// ── Utility ──────────────────────────────────────────────────────────

// Non-blocking recv with timeout (milliseconds)
AriaString* net_recv_timeout(void* h, int64_t max_bytes, int32_t timeout_ms) {
    if (!h) return make_aria_string("", 0);
    NetSocket* s = (NetSocket*)h;
    if (!s->connected || max_bytes <= 0) return make_aria_string("", 0);
    if (max_bytes > 1048576) max_bytes = 1048576;

    struct pollfd pfd;
    pfd.fd = s->fd;
    pfd.events = POLLIN;
    int ret = poll(&pfd, 1, timeout_ms);
    if (ret <= 0) return make_aria_string("", 0);

    return net_recv(h, max_bytes);
}

// Set socket non-blocking
void net_set_nonblocking(void* h) {
    if (!h) return;
    NetSocket* s = (NetSocket*)h;
    int flags = fcntl(s->fd, F_GETFL, 0);
    fcntl(s->fd, F_SETFL, flags | O_NONBLOCK);
}

} // extern "C"
