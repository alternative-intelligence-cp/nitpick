/**
 * zero_copy_bridge.h
 * Aria Zero-Copy I/O Bridge
 *
 * High-throughput data transfer infrastructure for the Aria six-stream topology.
 * Uses splice() on Linux for kernel-bypass zero-copy, falls back to an optimized
 * Active Pump on Windows/macOS.
 *
 * ARIA-005: ZeroCopyBridge - Ecosystem Integration
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_RUNTIME_ZERO_COPY_BRIDGE_H
#define ARIA_RUNTIME_ZERO_COPY_BRIDGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Error Codes
// ============================================================================

typedef enum {
    ARIA_SPLICE_OK = 0,
    ARIA_SPLICE_ERR_WOULD_BLOCK = -1,      // EAGAIN - use with event loop
    ARIA_SPLICE_ERR_INVALID_FD = -2,       // Bad file descriptor
    ARIA_SPLICE_ERR_NOT_SUPPORTED = -3,    // Splice not supported for this FD pair
    ARIA_SPLICE_ERR_PIPE_REQUIRED = -4,    // At least one FD must be a pipe
    ARIA_SPLICE_ERR_EOF = -5,              // End of file reached
    ARIA_SPLICE_ERR_TBB_OVERFLOW = -6,     // TBB sentinel detected in length
    ARIA_SPLICE_ERR_MEMORY = -7,           // Memory allocation failed
    ARIA_SPLICE_ERR_SYSTEM = -8,           // System error (check errno)
    ARIA_SPLICE_ERR_UNKNOWN = -99
} AriaSpliceError;

// ============================================================================
// Transfer Options
// ============================================================================

typedef struct {
    bool non_blocking;          // Use non-blocking I/O (for event loop)
    bool hint_more_data;        // Hint: more data follows (TCP_CORK optimization)
    size_t pipe_buffer_size;    // Target pipe buffer size (0 = default)
    bool use_copy_file_range;   // Try copy_file_range for file-to-file (Linux)
} AriaTransferOptions;

// Default options
static const AriaTransferOptions ARIA_TRANSFER_DEFAULTS = {
    .non_blocking = true,
    .hint_more_data = false,
    .pipe_buffer_size = 0,
    .use_copy_file_range = true
};

// ============================================================================
// Transfer Result
// ============================================================================

typedef struct {
    int64_t bytes_transferred;  // Bytes successfully transferred (or negative error)
    int error_code;             // AriaSpliceError code
    bool eof_reached;           // True if source EOF was reached
} AriaTransferResult;

// ============================================================================
// Ring Buffer (Active Pump)
// ============================================================================

/**
 * Lock-free Single-Producer/Single-Consumer ring buffer
 * Used for cross-platform Active Pump fallback
 */
typedef struct AriaRingBuffer AriaRingBuffer;

/**
 * Create a new ring buffer
 *
 * @param capacity Buffer capacity (must be power of 2)
 * @return Ring buffer instance or NULL on failure
 */
AriaRingBuffer* aria_ring_buffer_create(size_t capacity);

/**
 * Destroy a ring buffer
 */
void aria_ring_buffer_destroy(AriaRingBuffer* rb);

/**
 * Write data to ring buffer (producer)
 *
 * @return Bytes written (may be less than requested if buffer full)
 */
size_t aria_ring_buffer_write(AriaRingBuffer* rb, const void* data, size_t len);

/**
 * Read data from ring buffer (consumer)
 *
 * @return Bytes read (may be less than requested if buffer empty)
 */
size_t aria_ring_buffer_read(AriaRingBuffer* rb, void* data, size_t len);

/**
 * Get available space for writing
 */
size_t aria_ring_buffer_write_available(AriaRingBuffer* rb);

/**
 * Get available data for reading
 */
size_t aria_ring_buffer_read_available(AriaRingBuffer* rb);

/**
 * Check if buffer is empty
 */
bool aria_ring_buffer_is_empty(AriaRingBuffer* rb);

/**
 * Check if buffer is full
 */
bool aria_ring_buffer_is_full(AriaRingBuffer* rb);

// ============================================================================
// Zero-Copy Transfer API
// ============================================================================

/**
 * Transfer data between file descriptors with zero-copy optimization
 *
 * On Linux: Uses splice() for pipe-to-pipe/file transfers
 * On other platforms: Uses Active Pump with lock-free ring buffer
 *
 * At least one of fd_in or fd_out must be a pipe for splice() to work.
 * For file-to-file transfers on Linux 4.5+, copy_file_range() is used.
 *
 * @param fd_in   Source file descriptor
 * @param fd_out  Destination file descriptor
 * @param len     Maximum bytes to transfer (use -1 for all available)
 * @param opts    Transfer options (NULL for defaults)
 * @return Transfer result with bytes transferred or error
 */
AriaTransferResult aria_io_transfer(int fd_in, int fd_out, int64_t len,
                                     const AriaTransferOptions* opts);

/**
 * Simple wrapper for aria_io_transfer
 * Returns bytes transferred or negative error code
 */
int64_t aria_io_splice(int fd_in, int fd_out, int64_t len);

/**
 * Create an intermediate pipe for splice operations
 * Useful when neither source nor dest is a pipe
 *
 * @param pipe_fds Array of 2 ints to receive [read_end, write_end]
 * @param size     Target pipe buffer size (0 for system default)
 * @return 0 on success, negative error code on failure
 */
int aria_create_splice_pipe(int pipe_fds[2], size_t size);

/**
 * Resize a pipe's buffer capacity (Linux only)
 *
 * @param fd   Pipe file descriptor
 * @param size New size in bytes
 * @return Actual new size, or negative error code
 */
int64_t aria_resize_pipe(int fd, size_t size);

// ============================================================================
// Active Pump API
// ============================================================================

/**
 * Active Pump handle for async data transfer
 */
typedef struct AriaActivePump AriaActivePump;

/**
 * Pump status
 */
typedef enum {
    ARIA_PUMP_RUNNING = 0,
    ARIA_PUMP_COMPLETED = 1,
    ARIA_PUMP_ERROR = 2,
    ARIA_PUMP_CANCELLED = 3
} AriaPumpStatus;

/**
 * Create an Active Pump for async data transfer
 *
 * The pump runs in a background thread, continuously reading from
 * fd_in and writing to fd_out via a lock-free ring buffer.
 *
 * @param fd_in        Source file descriptor
 * @param fd_out       Destination file descriptor
 * @param buffer_size  Ring buffer size (0 for default 64KB)
 * @return Pump handle or NULL on failure
 */
AriaActivePump* aria_active_pump_create(int fd_in, int fd_out, size_t buffer_size);

/**
 * Get pump status
 */
AriaPumpStatus aria_active_pump_status(AriaActivePump* pump);

/**
 * Get bytes transferred so far
 */
int64_t aria_active_pump_bytes_transferred(AriaActivePump* pump);

/**
 * Cancel the pump
 */
void aria_active_pump_cancel(AriaActivePump* pump);

/**
 * Wait for pump to complete
 *
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @return Final status
 */
AriaPumpStatus aria_active_pump_wait(AriaActivePump* pump, uint32_t timeout_ms);

/**
 * Destroy the pump (cancels if still running)
 */
void aria_active_pump_destroy(AriaActivePump* pump);

// ============================================================================
// Platform Detection
// ============================================================================

/**
 * Check if zero-copy splice is available on this platform
 */
bool aria_splice_available(void);

/**
 * Check if copy_file_range is available (Linux 4.5+)
 */
bool aria_copy_file_range_available(void);

/**
 * Get maximum pipe buffer size on this system
 */
size_t aria_max_pipe_size(void);

// ============================================================================
// TBB Safety
// ============================================================================

// TBB sentinel value for int64_t
#define TBB64_ERR ((int64_t)0x8000000000000000LL)

/**
 * Check if value is a TBB error sentinel
 */
static inline bool aria_is_tbb_err(int64_t value) {
    return value == TBB64_ERR;
}

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_ZERO_COPY_BRIDGE_H
