/**
 * telemetry.cpp
 * Aria Telemetry System Implementation
 *
 * Provides structured NDJSON telemetry for the stddbg (FD 3) stream.
 *
 * ARIA-007: TelemetryDaemon - Ecosystem Integration
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "runtime/telemetry.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <chrono>
#include <condition_variable>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

// ============================================================================
// Constants
// ============================================================================

#define STDDBG_FD 3
#define DEFAULT_BUFFER_SIZE (8 * 1024)
#define MAX_JSON_LINE_SIZE (16 * 1024)

// ============================================================================
// Telemetry Writer Implementation
// ============================================================================

struct AriaTelemetryWriter {
    int fd;
    size_t buffer_size;
    bool async;

    // Buffer for accumulating output
    char* buffer;
    size_t buffer_pos;
    std::mutex buffer_mutex;

    // Async mode: background flusher thread
    std::thread flush_thread;
    std::condition_variable flush_cv;
    std::atomic<bool> shutdown;
    std::vector<std::string> pending_lines;
    std::mutex pending_mutex;
};

// Global writer instance
static AriaTelemetryWriter* g_global_writer = nullptr;
static std::mutex g_global_writer_mutex;

// Async flush thread function
static void async_flush_thread(AriaTelemetryWriter* writer) {
    while (!writer->shutdown.load(std::memory_order_acquire)) {
        std::vector<std::string> to_write;

        {
            std::unique_lock<std::mutex> lock(writer->pending_mutex);
            writer->flush_cv.wait_for(lock, std::chrono::milliseconds(100), [writer]() {
                return !writer->pending_lines.empty() ||
                       writer->shutdown.load(std::memory_order_acquire);
            });

            if (!writer->pending_lines.empty()) {
                to_write = std::move(writer->pending_lines);
                writer->pending_lines.clear();
            }
        }

        // Write outside the lock
        for (const auto& line : to_write) {
            ssize_t written = 0;
            size_t remaining = line.size();
            const char* ptr = line.c_str();

            while (remaining > 0) {
#ifdef _WIN32
                DWORD bytes_written = 0;
                if (!WriteFile((HANDLE)_get_osfhandle(writer->fd), ptr,
                               (DWORD)remaining, &bytes_written, NULL)) {
                    break;
                }
                written = bytes_written;
#else
                written = write(writer->fd, ptr, remaining);
                if (written < 0) {
                    if (errno == EINTR) continue;
                    break;
                }
#endif
                ptr += written;
                remaining -= written;
            }
        }
    }
}

AriaTelemetryWriter* aria_telemetry_writer_create(int fd, size_t buffer_size, bool async) {
    AriaTelemetryWriter* writer = new(std::nothrow) AriaTelemetryWriter();
    if (!writer) return nullptr;

    if (buffer_size == 0) buffer_size = DEFAULT_BUFFER_SIZE;

    writer->fd = fd;
    writer->buffer_size = buffer_size;
    writer->async = async;
    writer->buffer_pos = 0;
    writer->shutdown.store(false, std::memory_order_relaxed);

    writer->buffer = (char*)malloc(buffer_size);
    if (!writer->buffer) {
        delete writer;
        return nullptr;
    }

    if (async) {
        writer->flush_thread = std::thread(async_flush_thread, writer);
    }

    return writer;
}

void aria_telemetry_writer_destroy(AriaTelemetryWriter* writer) {
    if (!writer) return;

    // Signal shutdown
    writer->shutdown.store(true, std::memory_order_release);

    if (writer->async) {
        writer->flush_cv.notify_all();
        if (writer->flush_thread.joinable()) {
            writer->flush_thread.join();
        }
    }

    // Flush any remaining buffered data
    aria_telemetry_flush(writer);

    free(writer->buffer);
    delete writer;
}

void aria_telemetry_flush(AriaTelemetryWriter* writer) {
    if (!writer) return;

    std::lock_guard<std::mutex> lock(writer->buffer_mutex);

    if (writer->buffer_pos > 0) {
        ssize_t written = 0;
        size_t remaining = writer->buffer_pos;
        const char* ptr = writer->buffer;

        while (remaining > 0) {
#ifdef _WIN32
            DWORD bytes_written = 0;
            if (!WriteFile((HANDLE)_get_osfhandle(writer->fd), ptr,
                           (DWORD)remaining, &bytes_written, NULL)) {
                break;
            }
            written = bytes_written;
#else
            written = write(writer->fd, ptr, remaining);
            if (written < 0) {
                if (errno == EINTR) continue;
                break;
            }
#endif
            ptr += written;
            remaining -= written;
        }

        writer->buffer_pos = 0;
    }
}

AriaTelemetryWriter* aria_telemetry_get_writer(void) {
    std::lock_guard<std::mutex> lock(g_global_writer_mutex);

    if (!g_global_writer) {
        g_global_writer = aria_telemetry_writer_create(STDDBG_FD, 0, true);
    }

    return g_global_writer;
}

// ============================================================================
// Low-Level Emission
// ============================================================================

int aria_telemetry_emit_raw(AriaTelemetryWriter* writer, const char* json, size_t len) {
    if (!json) return -1;

    if (!writer) {
        writer = aria_telemetry_get_writer();
        if (!writer) return -1;
    }

    if (len == 0) len = strlen(json);

    // Build the line with newline
    std::string line(json, len);
    line.push_back('\n');

    if (writer->async) {
        // Queue for async writing
        {
            std::lock_guard<std::mutex> lock(writer->pending_mutex);
            writer->pending_lines.push_back(std::move(line));
        }
        writer->flush_cv.notify_one();
    } else {
        // Sync write
        ssize_t written = 0;
        size_t remaining = line.size();
        const char* ptr = line.c_str();

        while (remaining > 0) {
#ifdef _WIN32
            DWORD bytes_written = 0;
            if (!WriteFile((HANDLE)_get_osfhandle(writer->fd), ptr,
                           (DWORD)remaining, &bytes_written, NULL)) {
                return -1;
            }
            written = bytes_written;
#else
            written = write(writer->fd, ptr, remaining);
            if (written < 0) {
                if (errno == EINTR) continue;
                return -1;
            }
#endif
            ptr += written;
            remaining -= written;
        }
    }

    return 0;
}

int aria_telemetry_emit(AriaTelemetryWriter* writer,
                         const char* source,
                         const char* event,
                         const char* payload) {
    if (!source || !event) return -1;

    char json[MAX_JSON_LINE_SIZE];
    int64_t ts = aria_telemetry_timestamp_ns();

    int len;
    if (payload && payload[0] != '\0') {
        len = snprintf(json, sizeof(json),
            "{\"timestamp\":%lld,\"source\":\"%s\",\"event\":\"%s\",\"payload\":%s}",
            (long long)ts, source, event, payload);
    } else {
        len = snprintf(json, sizeof(json),
            "{\"timestamp\":%lld,\"source\":\"%s\",\"event\":\"%s\"}",
            (long long)ts, source, event);
    }

    if (len < 0 || len >= (int)sizeof(json)) {
        return -1;  // Truncated
    }

    return aria_telemetry_emit_raw(writer, json, len);
}

int aria_telemetry_log(AriaTelemetryWriter* writer,
                        AriaTelemetryLevel level,
                        const char* source,
                        const char* message) {
    if (!source || !message) return -1;

    char payload[MAX_JSON_LINE_SIZE];
    char escaped_msg[MAX_JSON_LINE_SIZE];

    aria_json_escape(message, escaped_msg, sizeof(escaped_msg));

    snprintf(payload, sizeof(payload),
        "{\"level\":\"%s\",\"message\":\"%s\"}",
        aria_telemetry_level_string(level), escaped_msg);

    return aria_telemetry_emit(writer, source, "log", payload);
}

// ============================================================================
// Metric Emission
// ============================================================================

int aria_telemetry_counter(AriaTelemetryWriter* writer,
                            const char* source,
                            const char* name,
                            int64_t value) {
    if (!source || !name) return -1;

    // TBB safety check
    if (aria_telem_is_tbb_err(value)) {
        return aria_telemetry_log(writer, ARIA_TELEM_ERROR, source,
            "TBB overflow in counter metric");
    }

    char payload[512];
    snprintf(payload, sizeof(payload),
        "{\"type\":\"counter\",\"name\":\"%s\",\"value\":%lld}",
        name, (long long)value);

    return aria_telemetry_emit(writer, source, "metric", payload);
}

int aria_telemetry_gauge(AriaTelemetryWriter* writer,
                          const char* source,
                          const char* name,
                          int64_t value) {
    if (!source || !name) return -1;

    if (aria_telem_is_tbb_err(value)) {
        return aria_telemetry_log(writer, ARIA_TELEM_ERROR, source,
            "TBB overflow in gauge metric");
    }

    char payload[512];
    snprintf(payload, sizeof(payload),
        "{\"type\":\"gauge\",\"name\":\"%s\",\"value\":%lld}",
        name, (long long)value);

    return aria_telemetry_emit(writer, source, "metric", payload);
}

int aria_telemetry_histogram(AriaTelemetryWriter* writer,
                              const char* source,
                              const char* name,
                              int64_t value) {
    if (!source || !name) return -1;

    if (aria_telem_is_tbb_err(value)) {
        return aria_telemetry_log(writer, ARIA_TELEM_ERROR, source,
            "TBB overflow in histogram metric");
    }

    char payload[512];
    snprintf(payload, sizeof(payload),
        "{\"type\":\"histogram\",\"name\":\"%s\",\"value\":%lld}",
        name, (long long)value);

    return aria_telemetry_emit(writer, source, "metric", payload);
}

// ============================================================================
// Trace/Span Implementation
// ============================================================================

struct AriaSpan {
    AriaTelemetryWriter* writer;
    std::string source;
    std::string operation;
    int64_t start_time_ns;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::string error_message;
    bool has_error;
};

AriaSpan* aria_span_start(AriaTelemetryWriter* writer,
                           const char* source,
                           const char* operation) {
    if (!source || !operation) return nullptr;

    AriaSpan* span = new(std::nothrow) AriaSpan();
    if (!span) return nullptr;

    span->writer = writer ? writer : aria_telemetry_get_writer();
    span->source = source;
    span->operation = operation;
    span->start_time_ns = aria_telemetry_timestamp_ns();
    span->has_error = false;

    // Emit span start event
    char payload[512];
    snprintf(payload, sizeof(payload),
        "{\"operation\":\"%s\",\"start_time_ns\":%lld}",
        operation, (long long)span->start_time_ns);

    aria_telemetry_emit(span->writer, source, "span_start", payload);

    return span;
}

void aria_span_set_attribute(AriaSpan* span, const char* key, const char* value) {
    if (!span || !key || !value) return;
    span->attributes.emplace_back(key, value);
}

void aria_span_set_attribute_int(AriaSpan* span, const char* key, int64_t value) {
    if (!span || !key) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)value);
    span->attributes.emplace_back(key, buf);
}

void aria_span_set_error(AriaSpan* span, const char* message) {
    if (!span || !message) return;
    span->has_error = true;
    span->error_message = message;
}

void aria_span_end(AriaSpan* span) {
    if (!span) return;

    int64_t end_time_ns = aria_telemetry_timestamp_ns();
    int64_t duration_ns = end_time_ns - span->start_time_ns;

    // Build payload
    char payload[MAX_JSON_LINE_SIZE];
    int pos = snprintf(payload, sizeof(payload),
        "{\"operation\":\"%s\",\"start_time_ns\":%lld,\"end_time_ns\":%lld,"
        "\"duration_ns\":%lld,\"error\":%s",
        span->operation.c_str(),
        (long long)span->start_time_ns,
        (long long)end_time_ns,
        (long long)duration_ns,
        span->has_error ? "true" : "false");

    // Add error message if present
    if (span->has_error && !span->error_message.empty()) {
        char escaped[1024];
        aria_json_escape(span->error_message.c_str(), escaped, sizeof(escaped));
        pos += snprintf(payload + pos, sizeof(payload) - pos,
            ",\"error_message\":\"%s\"", escaped);
    }

    // Add attributes
    if (!span->attributes.empty()) {
        pos += snprintf(payload + pos, sizeof(payload) - pos, ",\"attributes\":{");
        bool first = true;
        for (const auto& attr : span->attributes) {
            if (!first) {
                pos += snprintf(payload + pos, sizeof(payload) - pos, ",");
            }
            char escaped_key[256], escaped_val[256];
            aria_json_escape(attr.first.c_str(), escaped_key, sizeof(escaped_key));
            aria_json_escape(attr.second.c_str(), escaped_val, sizeof(escaped_val));
            pos += snprintf(payload + pos, sizeof(payload) - pos,
                "\"%s\":\"%s\"", escaped_key, escaped_val);
            first = false;
        }
        pos += snprintf(payload + pos, sizeof(payload) - pos, "}");
    }

    pos += snprintf(payload + pos, sizeof(payload) - pos, "}");

    aria_telemetry_emit(span->writer, span->source.c_str(), "span_end", payload);

    delete span;
}

// ============================================================================
// Session Stats
// ============================================================================

int aria_telemetry_session_stats(AriaTelemetryWriter* writer,
                                  const char* source,
                                  const AriaSessionStats* stats) {
    if (!source || !stats) return -1;

    char payload[1024];
    snprintf(payload, sizeof(payload),
        "{"
        "\"start_time_ns\":%lld,"
        "\"files_processed\":%lld,"
        "\"files_skipped\":%lld,"
        "\"bytes_read\":%lld,"
        "\"bytes_written\":%lld,"
        "\"errors_count\":%lld,"
        "\"current_throughput\":%lld,"
        "\"average_throughput\":%lld,"
        "\"compression_ratio\":%lld"
        "}",
        (long long)stats->start_time_ns,
        (long long)stats->files_processed,
        (long long)stats->files_skipped,
        (long long)stats->bytes_read,
        (long long)stats->bytes_written,
        (long long)stats->errors_count,
        (long long)stats->current_throughput,
        (long long)stats->average_throughput,
        (long long)stats->compression_ratio);

    return aria_telemetry_emit(writer, source, "session_stats", payload);
}

// ============================================================================
// Telemetry Consumer Implementation
// ============================================================================

struct AriaTelemetryConsumer {
    int fd;
    AriaTelemetryCallback callback;
    void* userdata;

    std::thread consume_thread;
    std::atomic<bool> running;
    std::atomic<int64_t> event_count;
    std::atomic<int64_t> error_count;
};

static void consumer_thread(AriaTelemetryConsumer* consumer) {
    char buffer[MAX_JSON_LINE_SIZE];
    std::string line_buffer;

    while (consumer->running.load(std::memory_order_acquire)) {
        ssize_t n;
#ifdef _WIN32
        DWORD bytes_read = 0;
        if (!ReadFile((HANDLE)_get_osfhandle(consumer->fd), buffer,
                       sizeof(buffer) - 1, &bytes_read, NULL)) {
            if (GetLastError() == ERROR_BROKEN_PIPE) break;
            continue;
        }
        n = bytes_read;
#else
        n = read(consumer->fd, buffer, sizeof(buffer) - 1);
#endif

        if (n > 0) {
            buffer[n] = '\0';
            line_buffer.append(buffer, n);

            // Process complete lines
            size_t pos;
            while ((pos = line_buffer.find('\n')) != std::string::npos) {
                std::string line = line_buffer.substr(0, pos);
                line_buffer.erase(0, pos + 1);

                // Skip empty lines
                if (line.empty()) continue;

                // Very basic JSON parsing to extract source and event
                // In production, use a proper JSON parser like simdjson
                const char* source_start = strstr(line.c_str(), "\"source\":\"");
                const char* event_start = strstr(line.c_str(), "\"event\":\"");

                if (source_start && event_start) {
                    source_start += 10;  // Skip "source":"
                    const char* source_end = strchr(source_start, '"');

                    event_start += 9;  // Skip "event":"
                    const char* event_end = strchr(event_start, '"');

                    if (source_end && event_end) {
                        std::string source(source_start, source_end - source_start);
                        std::string event(event_start, event_end - event_start);

                        consumer->callback(source.c_str(), event.c_str(),
                                          line.c_str(), consumer->userdata);
                        consumer->event_count.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        consumer->error_count.fetch_add(1, std::memory_order_relaxed);
                    }
                } else {
                    consumer->error_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        } else if (n == 0) {
            // EOF
            break;
        } else {
#ifndef _WIN32
            if (errno == EINTR) continue;
            if (errno == EAGAIN) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
#endif
            break;
        }
    }
}

AriaTelemetryConsumer* aria_telemetry_consumer_create(int fd,
                                                       AriaTelemetryCallback callback,
                                                       void* userdata) {
    if (!callback) return nullptr;

    AriaTelemetryConsumer* consumer = new(std::nothrow) AriaTelemetryConsumer();
    if (!consumer) return nullptr;

    consumer->fd = fd;
    consumer->callback = callback;
    consumer->userdata = userdata;
    consumer->running.store(false, std::memory_order_relaxed);
    consumer->event_count.store(0, std::memory_order_relaxed);
    consumer->error_count.store(0, std::memory_order_relaxed);

    return consumer;
}

int aria_telemetry_consumer_start(AriaTelemetryConsumer* consumer) {
    if (!consumer) return -1;
    if (consumer->running.load(std::memory_order_acquire)) return -1;

    consumer->running.store(true, std::memory_order_release);
    consumer->consume_thread = std::thread(consumer_thread, consumer);

    return 0;
}

void aria_telemetry_consumer_stop(AriaTelemetryConsumer* consumer) {
    if (!consumer) return;

    consumer->running.store(false, std::memory_order_release);

    if (consumer->consume_thread.joinable()) {
        consumer->consume_thread.join();
    }
}

void aria_telemetry_consumer_destroy(AriaTelemetryConsumer* consumer) {
    if (!consumer) return;

    aria_telemetry_consumer_stop(consumer);
    delete consumer;
}

int64_t aria_telemetry_consumer_event_count(AriaTelemetryConsumer* consumer) {
    if (!consumer) return 0;
    return consumer->event_count.load(std::memory_order_acquire);
}

int64_t aria_telemetry_consumer_error_count(AriaTelemetryConsumer* consumer) {
    if (!consumer) return 0;
    return consumer->error_count.load(std::memory_order_acquire);
}

// ============================================================================
// Utility Functions
// ============================================================================

int64_t aria_telemetry_timestamp_ns(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    // Convert to Unix timestamp in nanoseconds
    uint64_t time = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    // Windows FILETIME is in 100-nanosecond intervals since 1601
    // Unix time is in nanoseconds since 1970
    // Difference: 11644473600 seconds
    time -= 116444736000000000ULL;
    return (int64_t)(time * 100);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
}

size_t aria_json_escape(const char* input, char* output, size_t outlen) {
    if (!input || !output || outlen == 0) return 0;

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (input[in_pos] != '\0' && out_pos < outlen - 1) {
        char c = input[in_pos++];

        switch (c) {
            case '"':
                if (out_pos + 2 >= outlen) goto done;
                output[out_pos++] = '\\';
                output[out_pos++] = '"';
                break;
            case '\\':
                if (out_pos + 2 >= outlen) goto done;
                output[out_pos++] = '\\';
                output[out_pos++] = '\\';
                break;
            case '\n':
                if (out_pos + 2 >= outlen) goto done;
                output[out_pos++] = '\\';
                output[out_pos++] = 'n';
                break;
            case '\r':
                if (out_pos + 2 >= outlen) goto done;
                output[out_pos++] = '\\';
                output[out_pos++] = 'r';
                break;
            case '\t':
                if (out_pos + 2 >= outlen) goto done;
                output[out_pos++] = '\\';
                output[out_pos++] = 't';
                break;
            default:
                if ((unsigned char)c < 0x20) {
                    // Control character - escape as \uXXXX
                    if (out_pos + 6 >= outlen) goto done;
                    snprintf(output + out_pos, 7, "\\u%04x", (unsigned char)c);
                    out_pos += 6;
                } else {
                    output[out_pos++] = c;
                }
                break;
        }
    }

done:
    output[out_pos] = '\0';
    return out_pos;
}

const char* aria_telemetry_level_string(AriaTelemetryLevel level) {
    switch (level) {
        case ARIA_TELEM_DEBUG: return "debug";
        case ARIA_TELEM_INFO:  return "info";
        case ARIA_TELEM_WARN:  return "warn";
        case ARIA_TELEM_ERROR: return "error";
        case ARIA_TELEM_METRIC: return "metric";
        case ARIA_TELEM_SPAN_START: return "span_start";
        case ARIA_TELEM_SPAN_END: return "span_end";
        case ARIA_TELEM_EVENT: return "event";
        default: return "unknown";
    }
}
