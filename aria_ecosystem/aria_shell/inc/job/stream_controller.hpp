/**
 * AriaSH Hex-Stream Controller
 *
 * ARIA-021: Shell Job Control State Machine Design
 *
 * Manages the six-stream I/O topology for each process:
 * - stdin  (0): Control Input (UTF-8 text)
 * - stdout (1): User Output (UTF-8 text)
 * - stderr (2): Error Channel (UTF-8 text)
 * - stddbg (3): Telemetry (structured JSON/Logfmt)
 * - stddati(4): Data Input (binary/wild)
 * - stddato(5): Data Output (binary/wild)
 *
 * Implements the Threaded Draining Model to prevent pipe deadlock.
 */

#ifndef ARIASH_STREAM_CONTROLLER_HPP
#define ARIASH_STREAM_CONTROLLER_HPP

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <cstdint>

namespace ariash {
namespace job {

/**
 * Stream indices matching the Hex-Stream topology
 */
enum class StreamIndex : uint8_t {
    STDIN   = 0,    // Control Input
    STDOUT  = 1,    // User Output
    STDERR  = 2,    // Error Channel
    STDDBG  = 3,    // Telemetry
    STDDATI = 4,    // Data Input
    STDDATO = 5,    // Data Output

    COUNT   = 6
};

/**
 * Ring buffer for stream data
 *
 * Lock-free single-producer single-consumer queue.
 */
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity = 64 * 1024);
    ~RingBuffer();

    /**
     * Write data to buffer
     *
     * @param data Data to write
     * @param size Size in bytes
     * @return Bytes written (may be less than size if full)
     */
    size_t write(const void* data, size_t size);

    /**
     * Read data from buffer
     *
     * @param data Output buffer
     * @param maxSize Maximum bytes to read
     * @return Bytes read
     */
    size_t read(void* data, size_t maxSize);

    /**
     * Peek at data without consuming
     */
    size_t peek(void* data, size_t maxSize) const;

    /**
     * Get available bytes for reading
     */
    size_t available() const;

    /**
     * Get free space for writing
     */
    size_t freeSpace() const;

    /**
     * Check if buffer is empty
     */
    bool empty() const;

    /**
     * Check if buffer is full
     */
    bool full() const;

    /**
     * Clear the buffer
     */
    void clear();

private:
    std::vector<uint8_t> buffer;
    
    // Cache-line aligned atomics to prevent false sharing
    alignas(64) std::atomic<size_t> readPos{0};
    alignas(64) std::atomic<size_t> writePos{0};
    
    size_t capacity;
};

/**
 * Stream callback for data events
 *
 * @param stream Which stream produced data
 * @param data   Pointer to data (valid only during callback)
 * @param size   Size of data
 */
using StreamCallback = std::function<void(StreamIndex stream,
                                          const void* data, size_t size)>;

/**
 * Hex-Stream Pipe Set
 *
 * Holds the file descriptors for all six streams.
 */
struct HexStreamPipes {
#ifdef _WIN32
    void* handles[12];  // Read/write handles for each stream
#else
    int fds[12];        // fd[0,1] = stdin, fd[2,3] = stdout, etc.
#endif

    HexStreamPipes();
    void close();
    bool isValid() const;
};

/**
 * Stream Drainer Worker
 *
 * C++20 jthread-based worker that continuously drains a single FD.
 */
class StreamDrainer {
public:
    StreamDrainer(StreamIndex stream, int fd, RingBuffer* buffer, bool dropOnOverflow);
    ~StreamDrainer() = default;  // jthread joins automatically

    // Statistics
    size_t bytesTransferred() const { return bytesTransferred_.load(); }
    bool isActive() const { return active_.load(); }
    StreamIndex getStream() const { return stream_; }

private:
    void drainLoop(std::stop_token stoken);

    std::jthread worker_;
    StreamIndex stream_;
    int fd_;
    RingBuffer* buffer_;
    bool dropOnOverflow_;
    std::atomic<size_t> bytesTransferred_{0};
    std::atomic<bool> active_{false};
};

/**
 * Stream Controller
 *
 * Manages I/O for a single job using the Threaded Draining Model.
 *
 * Worker threads continuously drain output pipes into ring buffers,
 * preventing the kernel buffer from filling and causing deadlock.
 */
class StreamController {
public:
    StreamController();
    ~StreamController();

    // Non-copyable
    StreamController(const StreamController&) = delete;
    StreamController& operator=(const StreamController&) = delete;

    /**
     * Create pipes for all six streams
     *
     * @return true if successful
     */
    bool createPipes();

    /**
     * Setup child-side of pipes (call after fork, before exec)
     *
     * Redirects FDs 0-5 to the pipes.
     */
    bool setupChild();

    /**
     * Setup parent-side of pipes (call after fork)
     *
     * Closes child-side FDs and starts drain threads.
     */
    bool setupParent();

    /**
     * Start draining threads
     *
     * Spawns worker threads that continuously read from output pipes.
     */
    bool startDraining();

    /**
     * Stop draining threads
     */
    void stopDraining();

    /**
     * Write to stdin pipe
     *
     * @param data Data to write
     * @param size Size in bytes
     * @return Bytes written
     */
    ssize_t writeStdin(const void* data, size_t size);

    /**
     * Close stdin pipe (signals EOF to child process)
     *
     * Call this when done sending input to a process that reads from stdin.
     * Programs like cat, grep, sort wait for EOF before producing output.
     */
    void closeStdin();

    /**
     * Read from output buffer
     *
     * @param stream Which stream to read
     * @param data Output buffer
     * @param maxSize Maximum bytes to read
     * @return Bytes read
     */
    size_t readBuffer(StreamIndex stream, void* data, size_t maxSize);

    /**
     * Get available data in buffer
     */
    size_t availableData(StreamIndex stream) const;

    /**
     * Check if stream has pending data
     */
    bool hasPendingData(StreamIndex stream) const;

    /**
     * Register callback for stream data
     */
    void onData(StreamCallback callback);

    /**
     * Get pipe set for inspection
     */
    const HexStreamPipes& getPipes() const { return pipes; }

    /**
     * Set foreground mode
     *
     * In foreground mode, stdout/stderr bypass buffers and go to TTY.
     */
    void setForegroundMode(bool foreground);

    /**
     * Flush all buffers to callbacks
     */
    void flushBuffers();

    /**
     * Close all pipes
     */
    void close();

    /**
     * Get performance statistics
     */
    size_t getTotalBytesTransferred() const;
    size_t getActiveThreadCount() const;

private:
    HexStreamPipes pipes;

    // Ring buffers for each output stream (1MB each for high throughput)
    std::unique_ptr<RingBuffer> buffers[static_cast<int>(StreamIndex::COUNT)];

    // C++20 jthread-based drainers (auto-join on destruction)
    std::unique_ptr<StreamDrainer> drainers[4];  // stdout, stderr, stddbg, stddato

    // Callbacks
    std::vector<StreamCallback> callbacks;
    std::mutex callbackMutex;

    // Mode
    std::atomic<bool> foregroundMode{true};

    // Helper functions
    void notifyData(StreamIndex stream, const void* data, size_t size);

#ifdef __linux__
    // Zero-copy optimization using splice()
    ssize_t splicePipeToPipe(int fdIn, int fdOut, std::stop_token stoken);
#endif
};

} // namespace job
} // namespace ariash

#endif // ARIASH_STREAM_CONTROLLER_HPP
