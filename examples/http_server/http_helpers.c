/**
 * http_helpers.c — HTTP server helpers for EXP-5
 *
 * Exposes a simple int32-fd TCP server API plus AriaString-based request
 * parsing, so Aria code can handle HTTP routing without dealing with
 * sockaddr structs or POSIX socket internals.
 *
 * ABI notes:
 *   - AriaString layout matches include/runtime/strings.h exactly.
 *   - aria_http_send takes (int32, const AriaString*): Aria passes string
 *     parameters as AriaString* (Bug #13 — pointer, not by value).
 *   - aria_http_get_method / aria_http_get_path return AriaString*: Aria
 *     declares these as returning `string` (ptr to AriaString internally).
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

typedef struct {
    const char *data;
    int64_t     length;
} AriaString;

/* ── Global request state ──────────────────────────────────────────────── */
static char       g_method_buf[16];
static char       g_path_buf[256];
static char       g_recv_buf[8192];
static AriaString g_method_str;
static AriaString g_path_str;

/* ── Server lifecycle ──────────────────────────────────────────────────── */

int32_t aria_http_start(int32_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
    if (listen(fd, 5) < 0)                                    { close(fd); return -1; }
    return (int32_t)fd;
}

int32_t aria_http_accept(int32_t sfd) {
    return (int32_t)accept((int)sfd, NULL, NULL);
}

/* Read one HTTP request from client fd, extract method + path into globals. */
void aria_http_fill_request(int32_t cfd) {
    int n = (int)read((int)cfd, g_recv_buf, (int)sizeof(g_recv_buf) - 1);
    if (n < 0) n = 0;
    g_recv_buf[n] = '\0';

    const char *p = g_recv_buf;

    /* method */
    int mi = 0;
    while (*p && *p != ' ' && mi < 15) g_method_buf[mi++] = *p++;
    g_method_buf[mi] = '\0';
    if (*p == ' ') p++;

    /* path */
    int pi = 0;
    while (*p && *p != ' ' && *p != '\r' && *p != '\n' && pi < 255)
        g_path_buf[pi++] = *p++;
    g_path_buf[pi] = '\0';

    g_method_str.data = g_method_buf;  g_method_str.length = (int64_t)mi;
    g_path_str.data   = g_path_buf;    g_path_str.length   = (int64_t)pi;
}

/* Aria treats these as `string` return (= AriaString* internally). */
AriaString *aria_http_get_method(void) { return &g_method_str; }
AriaString *aria_http_get_path(void)   { return &g_path_str;   }

/* Send HTTP response.  Aria passes string params as AriaString* (Bug #13). */
void aria_http_send(int32_t cfd, const AriaString *resp) {
    if (resp && resp->data && resp->length > 0)
        (void)write((int)cfd, resp->data, (size_t)resp->length);
}

void aria_http_close(int32_t fd) {
    if (fd >= 0) close((int)fd);
}

/* ── Self-test client thread ───────────────────────────────────────────── */
static int32_t g_test_port;

static void *client_thread(void *arg) {
    (void)arg;
    usleep(50000);   /* 50 ms — ensure server is blocked in accept() */

    static const char *reqs[3] = {
        "GET / HTTP/1.0\r\nHost: localhost\r\n\r\n",
        "GET /ping HTTP/1.0\r\nHost: localhost\r\n\r\n",
        "GET /missing HTTP/1.0\r\nHost: localhost\r\n\r\n",
    };

    for (int i = 0; i < 3; i++) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port        = htons((uint16_t)g_test_port);

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) continue;
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            close(fd);
            continue;
        }

        const char *req = reqs[i];
        (void)write(fd, req, strlen(req));
        usleep(15000);   /* let server read before we close */
        close(fd);
        usleep(10000);   /* gap between requests */
    }
    return NULL;
}

void aria_http_start_test_client(int32_t port) {
    g_test_port = port;
    pthread_t tid;
    pthread_create(&tid, NULL, client_thread, NULL);
    pthread_detach(tid);
}
