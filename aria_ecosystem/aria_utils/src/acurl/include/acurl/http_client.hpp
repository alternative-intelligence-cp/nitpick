/**
 * acurl - Aria HTTP Client Library
 *
 * Implements the Hex-Stream transfer protocol for high-fidelity HTTP transfers.
 * - stdout (FD 1): User interface / progress
 * - stderr (FD 2): Error reporting
 * - stddbg (FD 3): Telemetry/protocol traces
 * - stddato (FD 5): Binary data output
 *
 * Features:
 * - TBB-safe arithmetic for protocol safety
 * - Zero-copy I/O with splice() on Linux
 * - Async event loop with libcurl multi interface
 * - Wild memory for transfer buffers
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_ACURL_HTTP_CLIENT_HPP
#define ARIA_ACURL_HTTP_CLIENT_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <optional>
#include <chrono>
#include <map>

namespace aria::acurl {

// =============================================================================
// TBB Types and Helpers
// =============================================================================

// TBB64 sentinel for sticky error propagation
constexpr int64_t TBB64_ERR = INT64_MIN;

// Check if a value is the TBB error sentinel
inline bool is_tbb_err(int64_t val) {
    return val == TBB64_ERR;
}

// Safe TBB addition - returns ERR on overflow
inline int64_t tbb_add(int64_t a, int64_t b) {
    if (is_tbb_err(a) || is_tbb_err(b)) return TBB64_ERR;
    // Check for overflow
    if (b > 0 && a > INT64_MAX - b) return TBB64_ERR;
    if (b < 0 && a < INT64_MIN - b) return TBB64_ERR;
    return a + b;
}

// =============================================================================
// Hex-Stream File Descriptors
// =============================================================================

constexpr int FD_STDIN   = 0;  // Control input (UTF-8 text)
constexpr int FD_STDOUT  = 1;  // User interface (UTF-8 text)
constexpr int FD_STDERR  = 2;  // Error reporting (UTF-8 text)
constexpr int FD_STDDBG  = 3;  // Telemetry/debug (structured)
constexpr int FD_STDDATI = 4;  // Binary data input
constexpr int FD_STDDATO = 5;  // Binary data output

// =============================================================================
// Error Codes
// =============================================================================

enum class ErrorCode {
    OK = 0,
    INVALID_URL,
    CONNECTION_FAILED,
    DNS_FAILED,
    SSL_ERROR,
    TIMEOUT,
    READ_ERROR,
    WRITE_ERROR,
    CANCELLED,
    OUT_OF_MEMORY,
    PROTOCOL_ERROR,
    OVERFLOW_ERROR,      // TBB overflow detected
    FD_SETUP_ERROR,      // Stream setup failed
    UNKNOWN_ERROR = -1
};

// Convert error code to string
const char* error_to_string(ErrorCode err);

// =============================================================================
// HTTP Types
// =============================================================================

enum class HttpMethod {
    GET,
    HEAD,
    POST,
    PUT,
    DELETE,
    PATCH,
    OPTIONS
};

// Convert method to string
const char* method_to_string(HttpMethod method);

// HTTP header pair
struct HttpHeader {
    std::string name;
    std::string value;
};

// HTTP request configuration
struct HttpRequest {
    HttpMethod method = HttpMethod::GET;
    std::string url;
    std::vector<HttpHeader> headers;
    std::string body;

    // Timeouts in seconds
    int32_t connect_timeout = 30;
    int32_t transfer_timeout = 0;  // 0 = no timeout

    // Options
    bool follow_redirects = true;
    int32_t max_redirects = 10;
    bool verify_ssl = true;
    std::string ca_bundle;

    // Authentication
    std::string username;
    std::string password;
    std::string bearer_token;

    // Custom user agent
    std::string user_agent = "acurl/0.1.0 (Aria)";
};

// HTTP response
struct HttpResponse {
    int32_t status_code = 0;
    std::string status_text;
    std::vector<HttpHeader> headers;
    int64_t content_length = -1;  // -1 = unknown
    std::string content_type;

    // Timing information (TBB-safe)
    int64_t dns_time_us = 0;
    int64_t connect_time_us = 0;
    int64_t ssl_time_us = 0;
    int64_t ttfb_us = 0;  // Time to first byte
    int64_t total_time_us = 0;

    // Get header by name (case-insensitive)
    std::optional<std::string> get_header(const std::string& name) const;
};

// =============================================================================
// Transfer State
// =============================================================================

struct TransferStats {
    // Byte counts (TBB-safe)
    int64_t bytes_total = 0;
    int64_t bytes_downloaded = 0;
    int64_t bytes_uploaded = 0;

    // Speed in bytes/sec
    double speed_download = 0.0;
    double speed_upload = 0.0;

    // Progress (0.0 - 100.0)
    double progress_percent = 0.0;

    // Timestamps
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point last_update;
};

// =============================================================================
// Callbacks
// =============================================================================

// Data callback - receives incoming data chunks
// Return: number of bytes handled, or -1 to abort
using DataCallback = std::function<int64_t(const uint8_t* data, size_t size, void* user_data)>;

// Progress callback - called periodically during transfer
// Return: true to continue, false to abort
using ProgressCallback = std::function<bool(const TransferStats& stats, void* user_data)>;

// Debug callback - receives protocol traces for stddbg
enum class DebugType {
    TEXT,       // Informational text
    HEADER_IN,  // Incoming headers
    HEADER_OUT, // Outgoing headers
    DATA_IN,    // Incoming data summary
    DATA_OUT,   // Outgoing data summary
    SSL_INFO    // TLS/SSL information
};

using DebugCallback = std::function<void(DebugType type, const std::string& data, void* user_data)>;

// =============================================================================
// HTTP Client
// =============================================================================

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Non-copyable but movable
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;

    // Configure callbacks
    void set_data_callback(DataCallback cb, void* user_data = nullptr);
    void set_progress_callback(ProgressCallback cb, void* user_data = nullptr);
    void set_debug_callback(DebugCallback cb, void* user_data = nullptr);

    // Configure Hex-Stream output
    // If stddato_fd >= 0, binary data is written directly to that FD
    void set_stddato_fd(int fd);

    // Enable zero-copy with splice() (Linux only)
    void set_zero_copy(bool enable);

    // Perform synchronous transfer
    std::pair<HttpResponse, ErrorCode> perform(const HttpRequest& request);

    // Get last transfer statistics
    const TransferStats& get_stats() const { return stats_; }

    // Cancel ongoing transfer
    void cancel();

    // Check if transfer was cancelled
    bool is_cancelled() const { return cancelled_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    TransferStats stats_;
    bool cancelled_ = false;

    // Hex-Stream configuration
    int stddato_fd_ = -1;
    bool zero_copy_ = true;

    // Callbacks
    DataCallback data_cb_;
    void* data_cb_user_ = nullptr;
    ProgressCallback progress_cb_;
    void* progress_cb_user_ = nullptr;
    DebugCallback debug_cb_;
    void* debug_cb_user_ = nullptr;
};

// =============================================================================
// Transfer Result (Hex-Stream Aware)
// =============================================================================

struct TransferResult {
    ErrorCode error = ErrorCode::OK;
    HttpResponse response;
    TransferStats stats;

    // If data was written to stddato (FD 5)
    int64_t bytes_to_stddato = 0;

    // Zero-copy stats
    int64_t bytes_spliced = 0;
    int64_t bytes_copied = 0;  // Fallback when splice unavailable
};

// =============================================================================
// Hex-Stream Transfer API
// =============================================================================

// Perform a transfer with full Hex-Stream support
// - Binary data -> stddato (FD 5)
// - UI/progress -> stdout (FD 1)
// - Errors -> stderr (FD 2)
// - Telemetry -> stddbg (FD 3)
TransferResult hex_transfer(const HttpRequest& request,
                            bool show_progress = true,
                            bool verbose = false);

// Simple GET to stddato
TransferResult hex_get(const std::string& url, bool verbose = false);

// POST with body, response to stddato
TransferResult hex_post(const std::string& url,
                        const std::string& body,
                        const std::string& content_type = "application/json",
                        bool verbose = false);

// =============================================================================
// Telemetry Writer (stddbg FD 3)
// =============================================================================

class TelemetryWriter {
public:
    TelemetryWriter(int fd = FD_STDDBG);
    ~TelemetryWriter();

    // Check if FD is valid
    bool is_open() const { return fd_ >= 0; }

    // Write NDJSON event
    void write_event(const std::string& type,
                     const std::map<std::string, std::string>& fields);

    // Convenience methods
    void log_request_start(const std::string& method, const std::string& url);
    void log_header_in(const std::string& header);
    void log_header_out(const std::string& header);
    void log_ssl_info(const std::string& info);
    void log_transfer_progress(int64_t bytes, int64_t total, double speed);
    void log_transfer_complete(int32_t status, int64_t bytes, int64_t time_us);
    void log_error(const std::string& message, ErrorCode code);

private:
    int fd_;
    void write_json(const std::string& json);
};

// =============================================================================
// Utility Functions
// =============================================================================

// Ensure Hex-Stream FDs are available (called by runtime)
// Opens /dev/null on FDs 3-5 if not already open
// Returns error if cannot set up streams
ErrorCode ensure_hex_streams();

// Check if running with systemd socket activation
// If so, remap FD 3 to avoid conflict with SD_LISTEN_FDS
bool handle_systemd_conflict();

// URL parsing utilities
struct ParsedUrl {
    std::string scheme;
    std::string host;
    uint16_t port = 0;
    std::string path;
    std::string query;
    std::string fragment;

    bool is_valid() const { return !scheme.empty() && !host.empty(); }
};

std::optional<ParsedUrl> parse_url(const std::string& url);

// Format bytes as human-readable string
std::string format_bytes(int64_t bytes);

// Format duration as human-readable string
std::string format_duration(int64_t microseconds);

// Format speed as human-readable string (e.g., "1.5 MB/s")
std::string format_speed(double bytes_per_sec);

// =============================================================================
// FFI (C API)
// =============================================================================

extern "C" {

// Error type for C API
typedef int32_t AriaError;

// Opaque handle to HTTP client (void* for C compatibility)
typedef void* AriaHttpClientHandle;

// Request structure for C API
typedef struct {
    const char* url;
    const char* method;  // "GET", "POST", etc.
    const char* body;
    size_t body_len;
    int32_t timeout;
    int32_t stddato_fd;  // -1 to disable
} AriaHttpRequest;

// Response structure for C API
typedef struct {
    int32_t status_code;
    int64_t content_length;
    int64_t bytes_transferred;
    int64_t time_us;
    AriaError error;
} AriaHttpResponse;

// Create client
AriaHttpClientHandle aria_acurl_new(void);

// Destroy client
void aria_acurl_free(AriaHttpClientHandle client);

// Perform transfer
AriaHttpResponse aria_acurl_perform(AriaHttpClientHandle client,
                                    const AriaHttpRequest* request);

// Simple GET to stddato (FD 5)
AriaHttpResponse aria_acurl_get(const char* url);

// Ensure Hex-Stream FDs are set up
AriaError aria_acurl_ensure_streams(void);

}  // extern "C"

}  // namespace aria::acurl

#endif // ARIA_ACURL_HTTP_CLIENT_HPP
