/*
 * aria_socket_shim.c — TCP/UDP socket abstraction for Aria
 * Wraps POSIX socket API for basic networking.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>

/* ═══════════════════ constants ═══════════════════ */

#define MAX_SOCKETS 64
#define RECV_BUF_SIZE 65536

/* ═══════════════════ state ═══════════════════ */

static char g_err[512] = "";
static char g_recv_buf[RECV_BUF_SIZE];
static int64_t g_recv_len = 0;
static const char *g_last_result = "";

static void set_err(const char *prefix) {
    snprintf(g_err, sizeof(g_err), "%s: %s", prefix, strerror(errno));
}

const char *aria_socket_error(void) { return g_err; }

/* ═══════════════════ TCP client ═══════════════════ */

int32_t aria_socket_tcp_connect(const char *host, int32_t port) {
    g_err[0] = '\0';

    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int rv = getaddrinfo(host, port_str, &hints, &res);
    if (rv != 0) {
        snprintf(g_err, sizeof(g_err), "tcp_connect: %s", gai_strerror(rv));
        return -1;
    }

    int fd = -1;
    for (p = res; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) { set_err("tcp_connect"); return -1; }
    return (int32_t)fd;
}

/* ═══════════════════ TCP server ═══════════════════ */

int32_t aria_socket_tcp_listen(const char *bind_addr, int32_t port, int32_t backlog) {
    g_err[0] = '\0';

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { set_err("tcp_listen(socket)"); return -1; }

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)port);
    addr.sin_addr.s_addr = inet_addr(bind_addr);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        set_err("tcp_listen(bind)");
        close(fd);
        return -1;
    }

    if (listen(fd, backlog) < 0) {
        set_err("tcp_listen(listen)");
        close(fd);
        return -1;
    }

    return (int32_t)fd;
}

int32_t aria_socket_tcp_accept(int32_t listen_fd) {
    g_err[0] = '\0';
    struct sockaddr_in client;
    socklen_t clen = sizeof(client);
    int fd = accept(listen_fd, (struct sockaddr *)&client, &clen);
    if (fd < 0) { set_err("tcp_accept"); return -1; }
    return (int32_t)fd;
}

/* ═══════════════════ UDP ═══════════════════ */

int32_t aria_socket_udp_create(void) {
    g_err[0] = '\0';
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { set_err("udp_create"); return -1; }
    return (int32_t)fd;
}

int32_t aria_socket_udp_bind(int32_t fd, const char *bind_addr, int32_t port) {
    g_err[0] = '\0';
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)port);
    addr.sin_addr.s_addr = inet_addr(bind_addr);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        set_err("udp_bind");
        return 0;
    }
    return 1;
}

int32_t aria_socket_udp_sendto(int32_t fd, const char *data, int32_t len,
                               const char *host, int32_t port) {
    g_err[0] = '\0';
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family      = AF_INET;
    dest.sin_port        = htons((uint16_t)port);
    dest.sin_addr.s_addr = inet_addr(host);

    ssize_t n = sendto(fd, data, (size_t)len, 0,
                       (struct sockaddr *)&dest, sizeof(dest));
    if (n < 0) { set_err("udp_sendto"); return -1; }
    return (int32_t)n;
}

const char *aria_socket_udp_recvfrom(int32_t fd) {
    g_err[0] = '\0';
    struct sockaddr_in src;
    socklen_t slen = sizeof(src);
    ssize_t n = recvfrom(fd, g_recv_buf, RECV_BUF_SIZE - 1, 0,
                         (struct sockaddr *)&src, &slen);
    if (n < 0) { set_err("udp_recvfrom"); g_recv_buf[0] = '\0'; g_recv_len = 0; g_last_result = g_recv_buf; return g_last_result; }
    g_recv_buf[n] = '\0';
    g_recv_len = (int64_t)n;
    g_last_result = g_recv_buf;
    return g_last_result;
}

/* ═══════════════════ send / recv ═══════════════════ */

int32_t aria_socket_send(int32_t fd, const char *data, int32_t len) {
    g_err[0] = '\0';
    ssize_t n = send(fd, data, (size_t)len, 0);
    if (n < 0) { set_err("send"); return -1; }
    return (int32_t)n;
}

int32_t aria_socket_send_str(int32_t fd, const char *data) {
    return aria_socket_send(fd, data, (int32_t)strlen(data));
}

const char *aria_socket_recv(int32_t fd, int32_t max_len) {
    g_err[0] = '\0';
    int32_t cap = max_len;
    if (cap > RECV_BUF_SIZE - 1) cap = RECV_BUF_SIZE - 1;
    ssize_t n = recv(fd, g_recv_buf, (size_t)cap, 0);
    if (n < 0) { set_err("recv"); g_recv_buf[0] = '\0'; g_recv_len = 0; g_last_result = g_recv_buf; return g_last_result; }
    g_recv_buf[n] = '\0';
    g_recv_len = (int64_t)n;
    g_last_result = g_recv_buf;
    return g_last_result;
}

const char *aria_socket_last_result(void) { return g_last_result; }

int64_t aria_socket_recv_length(void) { return g_recv_len; }

/* ═══════════════════ poll / non-blocking ═══════════════════ */

int32_t aria_socket_set_nonblocking(int32_t fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) { set_err("set_nonblocking"); return 0; }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) { set_err("set_nonblocking"); return 0; }
    return 1;
}

int32_t aria_socket_poll_read(int32_t fd, int32_t timeout_ms) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int rv = poll(&pfd, 1, timeout_ms);
    if (rv < 0) { set_err("poll_read"); return -1; }
    if (rv == 0) return 0; /* timeout */
    return 1; /* readable */
}

/* ═══════════════════ close ═══════════════════ */

void aria_socket_close(int32_t fd) {
    if (fd >= 0) close(fd);
}

/* ═══════════════════ utility ═══════════════════ */

int32_t aria_socket_set_timeout(int32_t fd, int32_t seconds) {
    struct timeval tv;
    tv.tv_sec  = seconds;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        set_err("set_timeout(recv)");
        return 0;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        set_err("set_timeout(send)");
        return 0;
    }
    return 1;
}

/* ═══════════════════ test helpers (C-side assertions) ═══════════════════ */

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_socket_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual); }
}

void aria_socket_assert_true(int32_t val, const char *msg) {
    if (val) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s\n", msg); }
}

void aria_socket_assert_udp_recv_eq(int32_t fd, const char *expected, const char *msg) {
    const char *result = aria_socket_udp_recvfrom(fd);
    if (strcmp(result, expected) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, result); }
}

void aria_socket_assert_tcp_recv_eq(int32_t fd, int32_t max_len, const char *expected, const char *msg) {
    const char *result = aria_socket_recv(fd, max_len);
    if (strcmp(result, expected) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, result); }
}

void aria_socket_test_summary(void) {
    printf("\n=== aria-socket test summary ===\n");
    printf("passed=%d failed=%d total=%d\n", g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
