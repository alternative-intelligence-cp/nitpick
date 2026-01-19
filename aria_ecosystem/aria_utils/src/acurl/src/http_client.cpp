/**
 * http_client.cpp
 * Implementation of the acurl HTTP client library
 *
 * Uses libcurl for HTTP backend with Hex-Stream routing
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "acurl/http_client.hpp"

#include <curl/curl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <errno.h>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <iomanip>

#ifdef __linux__
#include <sys/sendfile.h>
#endif

namespace aria::acurl {

// =============================================================================
// Error Handling
// =============================================================================

const char* error_to_string(ErrorCode err) {
    switch (err) {
        case ErrorCode::OK: return "OK";
        case ErrorCode::INVALID_URL: return "Invalid URL";
        case ErrorCode::CONNECTION_FAILED: return "Connection failed";
        case ErrorCode::DNS_FAILED: return "DNS resolution failed";
        case ErrorCode::SSL_ERROR: return "SSL/TLS error";
        case ErrorCode::TIMEOUT: return "Operation timed out";
        case ErrorCode::READ_ERROR: return "Read error";
        case ErrorCode::WRITE_ERROR: return "Write error";
        case ErrorCode::CANCELLED: return "Operation cancelled";
        case ErrorCode::OUT_OF_MEMORY: return "Out of memory";
        case ErrorCode::PROTOCOL_ERROR: return "Protocol error";
        case ErrorCode::OVERFLOW_ERROR: return "TBB overflow detected";
        case ErrorCode::FD_SETUP_ERROR: return "Stream setup failed";
        case ErrorCode::UNKNOWN_ERROR: return "Unknown error";
    }
    return "Unknown error";
}

const char* method_to_string(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::PATCH: return "PATCH";
        case HttpMethod::OPTIONS: return "OPTIONS";
    }
    return "GET";
}

// Convert curl error to our error code
static ErrorCode curl_to_error(CURLcode code) {
    switch (code) {
        case CURLE_OK: return ErrorCode::OK;
        case CURLE_URL_MALFORMAT: return ErrorCode::INVALID_URL;
        case CURLE_COULDNT_CONNECT: return ErrorCode::CONNECTION_FAILED;
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_RESOLVE_PROXY: return ErrorCode::DNS_FAILED;
        case CURLE_SSL_CONNECT_ERROR:
        case CURLE_SSL_CERTPROBLEM:
        case CURLE_SSL_CIPHER:
        case CURLE_SSL_CACERT: return ErrorCode::SSL_ERROR;
        case CURLE_OPERATION_TIMEDOUT: return ErrorCode::TIMEOUT;
        case CURLE_READ_ERROR: return ErrorCode::READ_ERROR;
        case CURLE_WRITE_ERROR: return ErrorCode::WRITE_ERROR;
        case CURLE_ABORTED_BY_CALLBACK: return ErrorCode::CANCELLED;
        case CURLE_OUT_OF_MEMORY: return ErrorCode::OUT_OF_MEMORY;
        default: return ErrorCode::UNKNOWN_ERROR;
    }
}

// =============================================================================
// URL Parsing
// =============================================================================

std::optional<ParsedUrl> parse_url(const std::string& url) {
    CURLU* h = curl_url();
    if (!h) return std::nullopt;

    if (curl_url_set(h, CURLUPART_URL, url.c_str(), 0) != CURLUE_OK) {
        curl_url_cleanup(h);
        return std::nullopt;
    }

    ParsedUrl result;
    char* part = nullptr;

    if (curl_url_get(h, CURLUPART_SCHEME, &part, 0) == CURLUE_OK && part) {
        result.scheme = part;
        curl_free(part);
    }

    if (curl_url_get(h, CURLUPART_HOST, &part, 0) == CURLUE_OK && part) {
        result.host = part;
        curl_free(part);
    }

    if (curl_url_get(h, CURLUPART_PORT, &part, 0) == CURLUE_OK && part) {
        result.port = static_cast<uint16_t>(std::stoi(part));
        curl_free(part);
    }

    if (curl_url_get(h, CURLUPART_PATH, &part, 0) == CURLUE_OK && part) {
        result.path = part;
        curl_free(part);
    }

    if (curl_url_get(h, CURLUPART_QUERY, &part, 0) == CURLUE_OK && part) {
        result.query = part;
        curl_free(part);
    }

    if (curl_url_get(h, CURLUPART_FRAGMENT, &part, 0) == CURLUE_OK && part) {
        result.fragment = part;
        curl_free(part);
    }

    curl_url_cleanup(h);
    return result;
}

// =============================================================================
// Formatting Utilities
// =============================================================================

std::string format_bytes(int64_t bytes) {
    if (is_tbb_err(bytes)) return "ERR";
    if (bytes < 0) return "N/A";

    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    int unit = 0;

    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }

    std::ostringstream oss;
    if (unit == 0) {
        oss << static_cast<int64_t>(size) << " " << units[unit];
    } else {
        oss << std::fixed << std::setprecision(1) << size << " " << units[unit];
    }
    return oss.str();
}

std::string format_duration(int64_t microseconds) {
    if (is_tbb_err(microseconds)) return "ERR";
    if (microseconds < 0) return "N/A";

    if (microseconds < 1000) {
        return std::to_string(microseconds) + " µs";
    } else if (microseconds < 1000000) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << (microseconds / 1000.0) << " ms";
        return oss.str();
    } else {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << (microseconds / 1000000.0) << " s";
        return oss.str();
    }
}

std::string format_speed(double bytes_per_sec) {
    if (bytes_per_sec < 0) return "N/A";

    double bits = bytes_per_sec * 8.0;
    const char* units[] = {"b/s", "Kb/s", "Mb/s", "Gb/s"};
    int unit = 0;

    while (bits >= 1000.0 && unit < 3) {
        bits /= 1000.0;
        unit++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << bits << " " << units[unit];
    return oss.str();
}

// =============================================================================
// Hex-Stream Setup
// =============================================================================

ErrorCode ensure_hex_streams() {
    // Check and open FDs 3, 4, 5 if not available
    for (int fd = FD_STDDBG; fd <= FD_STDDATO; fd++) {
        if (fcntl(fd, F_GETFD) == -1) {
            // FD not open, open /dev/null
            int null_fd = open("/dev/null", (fd == FD_STDDATI) ? O_RDONLY : O_WRONLY);
            if (null_fd < 0) {
                return ErrorCode::FD_SETUP_ERROR;
            }
            if (null_fd != fd) {
                if (dup2(null_fd, fd) < 0) {
                    close(null_fd);
                    return ErrorCode::FD_SETUP_ERROR;
                }
                close(null_fd);
            }
        }
    }
    return ErrorCode::OK;
}

bool handle_systemd_conflict() {
    // Check if FD 3 is a socket (systemd socket activation)
    const char* listen_fds = getenv("LISTEN_FDS");
    if (!listen_fds) return false;

    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    if (getsockname(FD_STDDBG, (struct sockaddr*)&addr, &len) == 0) {
        // FD 3 is a socket, remap it
        int new_fd = dup(FD_STDDBG);
        if (new_fd >= 0) {
            // Store the socket on a higher FD
            fcntl(new_fd, F_SETFD, FD_CLOEXEC);

            // Replace FD 3 with /dev/null for stddbg
            int null_fd = open("/dev/null", O_WRONLY);
            if (null_fd >= 0) {
                dup2(null_fd, FD_STDDBG);
                close(null_fd);
            }

            // Log warning
            const char* msg = "Warning: FD 3 remapped due to systemd socket activation\n";
            ssize_t written = write(FD_STDERR, msg, strlen(msg));
            (void)written;
            return true;
        }
    }
    return false;
}

// =============================================================================
// HttpResponse
// =============================================================================

std::optional<std::string> HttpResponse::get_header(const std::string& name) const {
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

    for (const auto& h : headers) {
        std::string lower_h = h.name;
        std::transform(lower_h.begin(), lower_h.end(), lower_h.begin(), ::tolower);
        if (lower_h == lower_name) {
            return h.value;
        }
    }
    return std::nullopt;
}

// =============================================================================
// TelemetryWriter
// =============================================================================

TelemetryWriter::TelemetryWriter(int fd) : fd_(fd) {
    // Verify FD is writable
    if (fd >= 0 && fcntl(fd, F_GETFD) == -1) {
        fd_ = -1;
    }
}

TelemetryWriter::~TelemetryWriter() {
    // Don't close - we don't own the FD
}

void TelemetryWriter::write_json(const std::string& json) {
    if (fd_ < 0) return;
    std::string line = json + "\n";
    ssize_t written = write(fd_, line.c_str(), line.size());
    (void)written;
}

static std::string escape_json(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    o << c;
                }
        }
    }
    return o.str();
}

static int64_t get_timestamp_us() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

void TelemetryWriter::write_event(const std::string& type,
                                  const std::map<std::string, std::string>& fields) {
    std::ostringstream json;
    json << "{\"type\":\"" << escape_json(type) << "\"";
    json << ",\"ts\":" << get_timestamp_us();
    for (const auto& [k, v] : fields) {
        json << ",\"" << escape_json(k) << "\":\"" << escape_json(v) << "\"";
    }
    json << "}";
    write_json(json.str());
}

void TelemetryWriter::log_request_start(const std::string& method, const std::string& url) {
    write_event("request_start", {{"method", method}, {"url", url}});
}

void TelemetryWriter::log_header_in(const std::string& header) {
    write_event("header_in", {{"data", header}});
}

void TelemetryWriter::log_header_out(const std::string& header) {
    write_event("header_out", {{"data", header}});
}

void TelemetryWriter::log_ssl_info(const std::string& info) {
    write_event("ssl_info", {{"data", info}});
}

void TelemetryWriter::log_transfer_progress(int64_t bytes, int64_t total, double speed) {
    std::ostringstream json;
    json << "{\"type\":\"progress\"";
    json << ",\"ts\":" << get_timestamp_us();
    json << ",\"bytes\":" << bytes;
    json << ",\"total\":" << total;
    json << ",\"speed\":" << std::fixed << std::setprecision(1) << speed;
    json << "}";
    write_json(json.str());
}

void TelemetryWriter::log_transfer_complete(int32_t status, int64_t bytes, int64_t time_us) {
    std::ostringstream json;
    json << "{\"type\":\"complete\"";
    json << ",\"ts\":" << get_timestamp_us();
    json << ",\"status\":" << status;
    json << ",\"bytes\":" << bytes;
    json << ",\"time_us\":" << time_us;
    json << "}";
    write_json(json.str());
}

void TelemetryWriter::log_error(const std::string& message, ErrorCode code) {
    write_event("error", {
        {"message", message},
        {"code", std::to_string(static_cast<int>(code))}
    });
}

// =============================================================================
// Curl Callback Data (at namespace scope for static callback access)
// =============================================================================

struct CurlCallbackData {
    CURL* curl = nullptr;
    struct curl_slist* headers = nullptr;

    // For stddato writing
    int stddato_fd = -1;
    bool use_splice = true;
    int64_t bytes_written = 0;
    int64_t bytes_spliced = 0;

    // For response headers
    std::vector<HttpHeader>* response_headers = nullptr;

    // Telemetry
    TelemetryWriter* telemetry = nullptr;

    // Callback storage (accessible from static callbacks)
    DataCallback data_cb;
    void* data_cb_user = nullptr;
    ProgressCallback progress_cb;
    void* progress_cb_user = nullptr;
    DebugCallback debug_cb;
    void* debug_cb_user = nullptr;

    // Stats (accessible from callbacks)
    TransferStats* stats = nullptr;
    bool* cancelled = nullptr;

    CurlCallbackData() {
        curl = curl_easy_init();
    }

    ~CurlCallbackData() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
        if (headers) {
            curl_slist_free_all(headers);
        }
    }
};

// =============================================================================
// HttpClient Implementation
// =============================================================================

struct HttpClient::Impl {
    std::unique_ptr<CurlCallbackData> cb_data;

    Impl() : cb_data(std::make_unique<CurlCallbackData>()) {}
};

HttpClient::HttpClient() : impl_(std::make_unique<Impl>()) {
    impl_->cb_data->stats = &stats_;
    impl_->cb_data->cancelled = &cancelled_;
}

HttpClient::~HttpClient() = default;

HttpClient::HttpClient(HttpClient&& other) noexcept = default;
HttpClient& HttpClient::operator=(HttpClient&& other) noexcept = default;

void HttpClient::set_data_callback(DataCallback cb, void* user_data) {
    data_cb_ = cb;
    data_cb_user_ = user_data;
    impl_->cb_data->data_cb = cb;
    impl_->cb_data->data_cb_user = user_data;
}

void HttpClient::set_progress_callback(ProgressCallback cb, void* user_data) {
    progress_cb_ = cb;
    progress_cb_user_ = user_data;
    impl_->cb_data->progress_cb = cb;
    impl_->cb_data->progress_cb_user = user_data;
}

void HttpClient::set_debug_callback(DebugCallback cb, void* user_data) {
    debug_cb_ = cb;
    debug_cb_user_ = user_data;
    impl_->cb_data->debug_cb = cb;
    impl_->cb_data->debug_cb_user = user_data;
}

void HttpClient::set_stddato_fd(int fd) {
    stddato_fd_ = fd;
    impl_->cb_data->stddato_fd = fd;
}

void HttpClient::set_zero_copy(bool enable) {
    zero_copy_ = enable;
    impl_->cb_data->use_splice = enable;
}

void HttpClient::cancel() {
    cancelled_ = true;
}

// Write callback for libcurl
static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    CurlCallbackData* cb = static_cast<CurlCallbackData*>(userdata);
    size_t total = size * nmemb;

    if (cb->cancelled && *cb->cancelled) {
        return 0;  // Abort transfer
    }

    // Write to stddato if configured
    if (cb->stddato_fd >= 0) {
        size_t written = 0;
        while (written < total) {
            ssize_t ret = write(cb->stddato_fd,
                               ptr + written,
                               total - written);
            if (ret < 0) {
                if (errno == EINTR) continue;
                return 0;  // Error
            }
            written += ret;
        }
        cb->bytes_written = tbb_add(cb->bytes_written, static_cast<int64_t>(written));
    }

    // Call user data callback if set
    if (cb->data_cb) {
        int64_t ret = cb->data_cb(
            reinterpret_cast<const uint8_t*>(ptr),
            total,
            cb->data_cb_user
        );
        if (ret < 0) return 0;  // Abort
    }

    return total;
}

// Header callback for libcurl
static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    CurlCallbackData* cb = static_cast<CurlCallbackData*>(userdata);
    size_t total = size * nitems;

    std::string line(buffer, total);
    // Trim trailing whitespace
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }

    if (!line.empty() && cb->response_headers) {
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            // Trim leading space from value
            while (!value.empty() && value.front() == ' ') {
                value.erase(0, 1);
            }
            cb->response_headers->push_back({name, value});
        }
    }

    // Log to telemetry
    if (cb->telemetry && !line.empty()) {
        cb->telemetry->log_header_in(line);
    }

    return total;
}

// Debug callback for libcurl
static int debug_callback(CURL* handle, curl_infotype type, char* data, size_t size, void* userp) {
    (void)handle;
    CurlCallbackData* cb = static_cast<CurlCallbackData*>(userp);

    std::string text(data, size);
    // Trim trailing whitespace
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) {
        text.pop_back();
    }

    if (text.empty()) return 0;

    DebugType dtype;
    switch (type) {
        case CURLINFO_TEXT:
            dtype = DebugType::TEXT;
            break;
        case CURLINFO_HEADER_IN:
            dtype = DebugType::HEADER_IN;
            break;
        case CURLINFO_HEADER_OUT:
            dtype = DebugType::HEADER_OUT;
            if (cb->telemetry) {
                cb->telemetry->log_header_out(text);
            }
            break;
        case CURLINFO_SSL_DATA_IN:
        case CURLINFO_SSL_DATA_OUT:
            dtype = DebugType::SSL_INFO;
            if (cb->telemetry) {
                cb->telemetry->log_ssl_info(text);
            }
            break;
        case CURLINFO_DATA_IN:
            dtype = DebugType::DATA_IN;
            text = "[" + std::to_string(size) + " bytes received]";
            break;
        case CURLINFO_DATA_OUT:
            dtype = DebugType::DATA_OUT;
            text = "[" + std::to_string(size) + " bytes sent]";
            break;
        default:
            return 0;
    }

    if (cb->debug_cb) {
        cb->debug_cb(dtype, text, cb->debug_cb_user);
    }

    return 0;
}

// Progress callback for libcurl
static int progress_callback(void* clientp,
                            curl_off_t dltotal, curl_off_t dlnow,
                            curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal;
    CurlCallbackData* cb = static_cast<CurlCallbackData*>(clientp);

    if (cb->cancelled && *cb->cancelled) {
        return 1;  // Abort
    }

    if (!cb->stats) return 0;

    TransferStats& stats = *cb->stats;
    stats.bytes_total = static_cast<int64_t>(dltotal);
    stats.bytes_downloaded = static_cast<int64_t>(dlnow);
    stats.bytes_uploaded = static_cast<int64_t>(ulnow);

    if (dltotal > 0) {
        stats.progress_percent = (static_cast<double>(dlnow) / dltotal) * 100.0;
    }

    stats.last_update = std::chrono::steady_clock::now();
    auto elapsed = stats.last_update - stats.start_time;
    double secs = std::chrono::duration<double>(elapsed).count();
    if (secs > 0.001) {
        stats.speed_download = static_cast<double>(dlnow) / secs;
        stats.speed_upload = static_cast<double>(ulnow) / secs;
    }

    if (cb->progress_cb) {
        if (!cb->progress_cb(stats, cb->progress_cb_user)) {
            return 1;  // Abort
        }
    }

    return 0;
}

std::pair<HttpResponse, ErrorCode> HttpClient::perform(const HttpRequest& request) {
    CurlCallbackData* cb = impl_->cb_data.get();
    if (!cb->curl) {
        return {{}, ErrorCode::UNKNOWN_ERROR};
    }

    CURL* curl = cb->curl;
    cancelled_ = false;

    // Reset state
    stats_ = TransferStats();
    stats_.start_time = std::chrono::steady_clock::now();
    cb->bytes_written = 0;
    cb->bytes_spliced = 0;

    HttpResponse response;
    cb->response_headers = &response.headers;

    // Clear previous headers
    if (cb->headers) {
        curl_slist_free_all(cb->headers);
        cb->headers = nullptr;
    }

    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());

    // Set method
    switch (request.method) {
        case HttpMethod::GET:
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            break;
        case HttpMethod::HEAD:
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
            break;
        case HttpMethod::POST:
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.body.size());
            break;
        case HttpMethod::PUT:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.body.size());
            break;
        case HttpMethod::DELETE:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
        case HttpMethod::PATCH:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.body.size());
            break;
        case HttpMethod::OPTIONS:
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS");
            break;
    }

    // Set headers
    for (const auto& h : request.headers) {
        std::string header = h.name + ": " + h.value;
        cb->headers = curl_slist_append(cb->headers, header.c_str());
    }
    if (cb->headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, cb->headers);
    }

    // Set timeouts
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, static_cast<long>(request.connect_timeout));
    if (request.transfer_timeout > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(request.transfer_timeout));
    }

    // Redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, request.follow_redirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(request.max_redirects));

    // SSL
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, request.verify_ssl ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, request.verify_ssl ? 2L : 0L);
    if (!request.ca_bundle.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, request.ca_bundle.c_str());
    }

    // Authentication
    if (!request.username.empty()) {
        curl_easy_setopt(curl, CURLOPT_USERNAME, request.username.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, request.password.c_str());
    }
    if (!request.bearer_token.empty()) {
        std::string auth = "Authorization: Bearer " + request.bearer_token;
        cb->headers = curl_slist_append(cb->headers, auth.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, cb->headers);
    }

    // User agent
    curl_easy_setopt(curl, CURLOPT_USERAGENT, request.user_agent.c_str());

    // Callbacks
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, cb);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, cb);

    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, cb);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    if (debug_cb_) {
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_callback);
        curl_easy_setopt(curl, CURLOPT_DEBUGDATA, cb);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    }

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    // Get response info
    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    response.status_code = static_cast<int32_t>(status_code);

    curl_off_t cl = 0;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
    response.content_length = static_cast<int64_t>(cl);

    char* ct = nullptr;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct);
    if (ct) response.content_type = ct;

    // Timing info (in microseconds)
    double dns_time = 0, connect_time = 0, ssl_time = 0, ttfb = 0, total_time = 0;
    curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME, &dns_time);
    curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME, &connect_time);
    curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME, &ssl_time);
    curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &ttfb);
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);

    response.dns_time_us = static_cast<int64_t>(dns_time * 1000000);
    response.connect_time_us = static_cast<int64_t>(connect_time * 1000000);
    response.ssl_time_us = static_cast<int64_t>(ssl_time * 1000000);
    response.ttfb_us = static_cast<int64_t>(ttfb * 1000000);
    response.total_time_us = static_cast<int64_t>(total_time * 1000000);

    ErrorCode error = curl_to_error(res);
    if (cancelled_ && error == ErrorCode::OK) {
        error = ErrorCode::CANCELLED;
    }

    // Check for TBB overflow
    if (is_tbb_err(cb->bytes_written)) {
        error = ErrorCode::OVERFLOW_ERROR;
    }

    return {response, error};
}

// =============================================================================
// High-Level Hex-Stream API
// =============================================================================

TransferResult hex_transfer(const HttpRequest& request,
                            bool show_progress,
                            bool verbose) {
    TransferResult result;

    // Ensure streams are set up
    result.error = ensure_hex_streams();
    if (result.error != ErrorCode::OK) {
        return result;
    }

    // Create client
    HttpClient client;
    client.set_stddato_fd(FD_STDDATO);
    client.set_zero_copy(true);

    // Telemetry writer
    TelemetryWriter telemetry(FD_STDDBG);

    if (verbose && telemetry.is_open()) {
        telemetry.log_request_start(method_to_string(request.method), request.url);

        client.set_debug_callback([&telemetry](DebugType type, const std::string& data, void*) {
            switch (type) {
                case DebugType::HEADER_IN:
                    telemetry.log_header_in(data);
                    break;
                case DebugType::HEADER_OUT:
                    telemetry.log_header_out(data);
                    break;
                case DebugType::SSL_INFO:
                    telemetry.log_ssl_info(data);
                    break;
                default:
                    break;
            }
        }, nullptr);
    }

    // Progress callback (stdout)
    if (show_progress) {
        client.set_progress_callback([&verbose, &telemetry](const TransferStats& stats, void*) {
            // Format progress bar for stdout
            int width = 50;
            int filled = static_cast<int>(stats.progress_percent / 100.0 * width);

            std::ostringstream oss;
            oss << "\r[";
            for (int i = 0; i < width; i++) {
                oss << (i < filled ? "=" : " ");
            }
            oss << "] " << std::fixed << std::setprecision(1) << stats.progress_percent << "% ";
            oss << format_bytes(stats.bytes_downloaded);
            if (stats.bytes_total > 0) {
                oss << "/" << format_bytes(stats.bytes_total);
            }
            oss << " " << format_speed(stats.speed_download);

            std::string line = oss.str();
            ssize_t written = write(FD_STDOUT, line.c_str(), line.size());
            (void)written;

            // Log to telemetry periodically
            if (verbose && telemetry.is_open()) {
                telemetry.log_transfer_progress(stats.bytes_downloaded,
                                                stats.bytes_total,
                                                stats.speed_download);
            }

            return true;
        }, nullptr);
    }

    // Perform transfer
    auto [response, error] = client.perform(request);

    result.error = error;
    result.response = response;
    result.stats = client.get_stats();
    result.bytes_to_stddato = result.stats.bytes_downloaded;

    // Log completion
    if (verbose && telemetry.is_open()) {
        if (error == ErrorCode::OK) {
            telemetry.log_transfer_complete(response.status_code,
                                           result.stats.bytes_downloaded,
                                           response.total_time_us);
        } else {
            telemetry.log_error(error_to_string(error), error);
        }
    }

    // Final newline for progress bar
    if (show_progress) {
        ssize_t written = write(FD_STDOUT, "\n", 1);
        (void)written;
    }

    return result;
}

TransferResult hex_get(const std::string& url, bool verbose) {
    HttpRequest request;
    request.method = HttpMethod::GET;
    request.url = url;
    return hex_transfer(request, true, verbose);
}

TransferResult hex_post(const std::string& url,
                        const std::string& body,
                        const std::string& content_type,
                        bool verbose) {
    HttpRequest request;
    request.method = HttpMethod::POST;
    request.url = url;
    request.body = body;
    request.headers.push_back({"Content-Type", content_type});
    return hex_transfer(request, true, verbose);
}

}  // namespace aria::acurl
