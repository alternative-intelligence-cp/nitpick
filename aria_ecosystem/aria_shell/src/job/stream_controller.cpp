/**
 * AriaSH Hex-Stream Controller Implementation
 *
 * ARIA-021: Shell Job Control State Machine Design
 *
 * Implements the Threaded Draining Model for deadlock-free I/O.
 * Uses C++20 std::jthread with cooperative interruption for safe thread lifecycle.
 */

#include "job/stream_controller.hpp"
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#endif

namespace ariash {
namespace job {

// =============================================================================
// Ring Buffer Implementation
// =============================================================================

RingBuffer::RingBuffer(size_t cap)
    : buffer(cap), capacity(cap) {}

RingBuffer::~RingBuffer() = default;

size_t RingBuffer::write(const void* data, size_t size) {
    size_t free = freeSpace();
    size_t toWrite = std::min(size, free);

    if (toWrite == 0) return 0;

    size_t wpos = writePos.load(std::memory_order_relaxed);
    const uint8_t* src = static_cast<const uint8_t*>(data);

    // Write in up to two parts (wrap around)
    size_t firstPart = std::min(toWrite, capacity - wpos);
    std::memcpy(buffer.data() + wpos, src, firstPart);

    if (toWrite > firstPart) {
        std::memcpy(buffer.data(), src + firstPart, toWrite - firstPart);
    }

    writePos.store((wpos + toWrite) % capacity, std::memory_order_release);
    return toWrite;
}

size_t RingBuffer::read(void* data, size_t maxSize) {
    size_t avail = available();
    size_t toRead = std::min(maxSize, avail);

    if (toRead == 0) return 0;

    size_t rpos = readPos.load(std::memory_order_relaxed);
    uint8_t* dst = static_cast<uint8_t*>(data);

    // Read in up to two parts (wrap around)
    size_t firstPart = std::min(toRead, capacity - rpos);
    std::memcpy(dst, buffer.data() + rpos, firstPart);

    if (toRead > firstPart) {
        std::memcpy(dst + firstPart, buffer.data(), toRead - firstPart);
    }

    readPos.store((rpos + toRead) % capacity, std::memory_order_release);
    return toRead;
}

size_t RingBuffer::peek(void* data, size_t maxSize) const {
    size_t avail = available();
    size_t toPeek = std::min(maxSize, avail);

    if (toPeek == 0) return 0;

    size_t rpos = readPos.load(std::memory_order_acquire);
    uint8_t* dst = static_cast<uint8_t*>(data);

    size_t firstPart = std::min(toPeek, capacity - rpos);
    std::memcpy(dst, buffer.data() + rpos, firstPart);

    if (toPeek > firstPart) {
        std::memcpy(dst + firstPart, buffer.data(), toPeek - firstPart);
    }

    return toPeek;
}

size_t RingBuffer::available() const {
    size_t w = writePos.load(std::memory_order_acquire);
    size_t r = readPos.load(std::memory_order_acquire);
    return (w >= r) ? (w - r) : (capacity - r + w);
}

size_t RingBuffer::freeSpace() const {
    return capacity - available() - 1;  // -1 to distinguish full from empty
}

bool RingBuffer::empty() const {
    return available() == 0;
}

bool RingBuffer::full() const {
    return freeSpace() == 0;
}

void RingBuffer::clear() {
    readPos.store(0, std::memory_order_release);
    writePos.store(0, std::memory_order_release);
}

// =============================================================================
// Hex-Stream Pipes Implementation
// =============================================================================

HexStreamPipes::HexStreamPipes() {
#ifdef _WIN32
    for (int i = 0; i < 12; ++i) {
        handles[i] = INVALID_HANDLE_VALUE;
    }
#else
    for (int i = 0; i < 12; ++i) {
        fds[i] = -1;
    }
#endif
}

void HexStreamPipes::close() {
#ifdef _WIN32
    for (int i = 0; i < 12; ++i) {
        if (handles[i] != INVALID_HANDLE_VALUE) {
            CloseHandle(handles[i]);
            handles[i] = INVALID_HANDLE_VALUE;
        }
    }
#else
    for (int i = 0; i < 12; ++i) {
        if (fds[i] >= 0) {
            ::close(fds[i]);
            fds[i] = -1;
        }
    }
#endif
}

bool HexStreamPipes::isValid() const {
#ifdef _WIN32
    // Check at least stdin, stdout, stderr are valid
    return handles[0] != INVALID_HANDLE_VALUE &&
           handles[2] != INVALID_HANDLE_VALUE &&
           handles[4] != INVALID_HANDLE_VALUE;
#else
    return fds[0] >= 0 && fds[2] >= 0 && fds[4] >= 0;
#endif
}

// =============================================================================
// Stream Drainer Implementation (C++20 jthread)
// =============================================================================

StreamDrainer::StreamDrainer(StreamIndex stream, int fd, RingBuffer* buffer, bool dropOnOverflow)
    : stream_(stream), fd_(fd), buffer_(buffer), dropOnOverflow_(dropOnOverflow)
{
    // Start worker thread immediately
    worker_ = std::jthread([this](std::stop_token stoken) {
        drainLoop(stoken);
    });
}

void StreamDrainer::drainLoop(std::stop_token stoken) {
    active_.store(true, std::memory_order_release);
    
    std::vector<uint8_t> readBuffer(4096);  // 4KB local buffer

    while (!stoken.stop_requested()) {
        struct pollfd pfd;
        pfd.fd = fd_;
        pfd.events = POLLIN;

        // Wait up to 100ms for data (allows checking stop_token periodically)
        int ret = poll(&pfd, 1, 100);

        if (ret < 0) {
            if (errno == EINTR) continue;  // Signal interruption, retry
            break;  // Fatal error
        }

        if (ret == 0) {
            // Timeout: loop back to check stop_requested()
            continue;
        }

        // Data available
        if (pfd.revents & POLLIN) {
            ssize_t n = read(fd_, readBuffer.data(), readBuffer.size());
            
            if (n > 0) {
                // Successful read
                size_t written = buffer_->write(readBuffer.data(), n);
                
                if (written < static_cast<size_t>(n)) {
                    // Buffer full - apply overflow policy
                    if (dropOnOverflow_) {
                        // DROP MODE: Discard excess data (acceptable for telemetry)
                        // Already written what we could, drop the rest
                    } else {
                        // BLOCK MODE: Apply backpressure
                        size_t remaining = n - written;
                        const uint8_t* remainingData = readBuffer.data() + written;
                        
                        while (remaining > 0 && !stoken.stop_requested()) {
                            size_t chunk = buffer_->write(remainingData, remaining);
                            if (chunk == 0) {
                                // Still full, yield to consumer
                                std::this_thread::yield();
                            } else {
                                remainingData += chunk;
                                remaining -= chunk;
                            }
                        }
                    }
                }
                
                bytesTransferred_.fetch_add(n, std::memory_order_relaxed);
            } else if (n == 0) {
                // EOF: Child closed the pipe (normal exit)
                break;
            } else {
                // Error handling
                if (errno != EAGAIN && errno != EINTR) {
                    break;
                }
            }
        }
        
        if (pfd.revents & (POLLHUP | POLLERR)) {
            // Pipe closed or error
            break;
        }
    }
    
    active_.store(false, std::memory_order_release);
}

// =============================================================================
// Stream Controller Implementation
// =============================================================================

StreamController::StreamController() {
    // Create ring buffers for output streams (1MB each for high throughput)
    for (int i = 0; i < static_cast<int>(StreamIndex::COUNT); ++i) {
        buffers[i] = std::make_unique<RingBuffer>(1024 * 1024);
    }
    
    // Initialize drainers to nullptr
    for (int i = 0; i < 4; ++i) {
        drainers[i] = nullptr;
    }
}

StreamController::~StreamController() {
    stopDraining();
    close();
}

bool StreamController::createPipes() {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // Create pipes for each stream
    for (int i = 0; i < 6; ++i) {
        int readIdx = i * 2;
        int writeIdx = i * 2 + 1;

        if (!CreatePipe(&pipes.handles[readIdx], &pipes.handles[writeIdx],
                        &sa, 0)) {
            close();
            return false;
        }
    }
    return true;
#else
    // Create pipes with O_CLOEXEC for security
    for (int i = 0; i < 6; ++i) {
        int pipefd[2];
#ifdef __linux__
        if (pipe2(pipefd, O_CLOEXEC) < 0) {
#else
        if (pipe(pipefd) < 0) {
#endif
            close();
            return false;
        }
        pipes.fds[i * 2] = pipefd[0];      // Read end
        pipes.fds[i * 2 + 1] = pipefd[1];  // Write end
    }
    return true;
#endif
}

bool StreamController::setupChild() {
#ifndef _WIN32
    // Redirect FDs 0-5 to pipes
    // stdin (stream 0): read from parent's write end (pipes.fds[1])
    if (dup2(pipes.fds[1], STDIN_FILENO) < 0) return false;

    // stdout (stream 1): write to parent's read end (pipes.fds[3] is write end)
    if (dup2(pipes.fds[3], STDOUT_FILENO) < 0) return false;

    // stderr (stream 2): write to parent's read end (pipes.fds[5] is write end)
    if (dup2(pipes.fds[5], STDERR_FILENO) < 0) return false;

    // stddbg (stream 3, FD 3): write to parent's read end (pipes.fds[7] is write end)
    if (dup2(pipes.fds[7], 3) < 0) return false;

    // stddati (stream 4, FD 4): read from parent's write end (pipes.fds[9])
    if (dup2(pipes.fds[9], 4) < 0) return false;

    // stddato (stream 5, FD 5): write to parent's read end (pipes.fds[11] is write end)
    if (dup2(pipes.fds[11], 5) < 0) return false;

    // Close all original pipe FDs
    for (int i = 0; i < 12; ++i) {
        if (pipes.fds[i] >= 0) {
            ::close(pipes.fds[i]);
        }
    }
#endif
    return true;
}

bool StreamController::setupParent() {
#ifndef _WIN32
    // Close child-side FDs

    // stdin: close read end (child uses it)
    ::close(pipes.fds[0]);
    pipes.fds[0] = -1;

    // stdout: close write end (child uses it)
    ::close(pipes.fds[3]);
    pipes.fds[3] = -1;

    // stderr: close write end
    ::close(pipes.fds[5]);
    pipes.fds[5] = -1;

    // stddbg: close write end
    ::close(pipes.fds[7]);
    pipes.fds[7] = -1;

    // stddati: close read end
    ::close(pipes.fds[8]);
    pipes.fds[8] = -1;

    // stddato: close write end
    ::close(pipes.fds[11]);
    pipes.fds[11] = -1;
#endif
    return true;
}

bool StreamController::startDraining() {
#ifndef _WIN32
    // Start C++20 jthread drainers for output streams
    
    // stdout (fd index 2) - block on overflow (user output is critical)
    if (pipes.fds[2] >= 0) {
        drainers[0] = std::make_unique<StreamDrainer>(StreamIndex::STDOUT,
                                                       pipes.fds[2], 
                                                       buffers[static_cast<int>(StreamIndex::STDOUT)].get(),
                                                       false);  // block on overflow
    }

    // stderr (fd index 4) - block on overflow (errors are critical)
    if (pipes.fds[4] >= 0) {
        drainers[1] = std::make_unique<StreamDrainer>(StreamIndex::STDERR,
                                                       pipes.fds[4],
                                                       buffers[static_cast<int>(StreamIndex::STDERR)].get(),
                                                       false);  // block on overflow
    }

    // stddbg (fd index 6) - drop on overflow (telemetry should never block)
    if (pipes.fds[6] >= 0) {
        drainers[2] = std::make_unique<StreamDrainer>(StreamIndex::STDDBG,
                                                       pipes.fds[6],
                                                       buffers[static_cast<int>(StreamIndex::STDDBG)].get(),
                                                       true);  // drop on overflow
    }

    // stddato (fd index 10) - block on overflow (binary data is critical)
    if (pipes.fds[10] >= 0) {
        drainers[3] = std::make_unique<StreamDrainer>(StreamIndex::STDDATO,
                                                       pipes.fds[10],
                                                       buffers[static_cast<int>(StreamIndex::STDDATO)].get(),
                                                       false);  // block on overflow
    }
#endif

    return true;
}

void StreamController::stopDraining() {
    // C++20 jthread auto-stops and joins on destruction
    for (int i = 0; i < 4; ++i) {
        drainers[i].reset();  // Triggers jthread destructor -> request_stop() + join()
    }
}

ssize_t StreamController::writeStdin(const void* data, size_t size) {
#ifdef _WIN32
    DWORD written;
    if (!WriteFile(pipes.handles[1], data, (DWORD)size, &written, NULL)) {
        return -1;
    }
    return written;
#else
    return write(pipes.fds[1], data, size);
#endif
}

void StreamController::closeStdin() {
#ifdef _WIN32
    if (pipes.handles[1] != INVALID_HANDLE_VALUE) {
        CloseHandle(pipes.handles[1]);
        pipes.handles[1] = INVALID_HANDLE_VALUE;
    }
#else
    if (pipes.fds[1] >= 0) {
        ::close(pipes.fds[1]);
        pipes.fds[1] = -1;
    }
#endif
}

size_t StreamController::readBuffer(StreamIndex stream, void* data, size_t maxSize) {
    int idx = static_cast<int>(stream);
    return buffers[idx]->read(data, maxSize);
}

size_t StreamController::availableData(StreamIndex stream) const {
    int idx = static_cast<int>(stream);
    return buffers[idx]->available();
}

bool StreamController::hasPendingData(StreamIndex stream) const {
    return availableData(stream) > 0;
}

void StreamController::onData(StreamCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    callbacks.push_back(callback);
}

void StreamController::notifyData(StreamIndex stream, const void* data, size_t size) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    for (auto& cb : callbacks) {
        cb(stream, data, size);
    }
}

void StreamController::setForegroundMode(bool foreground) {
    foregroundMode.store(foreground);
}

void StreamController::flushBuffers() {
    uint8_t buf[4096];

    for (int i = 0; i < static_cast<int>(StreamIndex::COUNT); ++i) {
        while (!buffers[i]->empty()) {
            size_t n = buffers[i]->read(buf, sizeof(buf));
            if (n > 0) {
                notifyData(static_cast<StreamIndex>(i), buf, n);
            }
        }
    }
}

void StreamController::close() {
    stopDraining();
    pipes.close();
}

size_t StreamController::getTotalBytesTransferred() const {
    size_t total = 0;
    for (int i = 0; i < 4; ++i) {
        if (drainers[i]) {
            total += drainers[i]->bytesTransferred();
        }
    }
    return total;
}

size_t StreamController::getActiveThreadCount() const {
    size_t count = 0;
    for (int i = 0; i < 4; ++i) {
        if (drainers[i] && drainers[i]->isActive()) {
            ++count;
        }
    }
    return count;
}

#ifdef __linux__
// Zero-copy splice optimization for Linux
ssize_t StreamController::splicePipeToPipe(int fdIn, int fdOut, std::stop_token stoken) {
    ssize_t totalBytes = 0;
    
    while (!stoken.stop_requested()) {
        // Attempt to move up to 1MB (splice works in kernel space)
        // SPLICE_F_MOVE: Hint to kernel to move pages instead of copying
        // SPLICE_F_NONBLOCK: Don't hang if pipe is full/empty
        ssize_t ret = splice(fdIn, nullptr, fdOut, nullptr, 1048576,
                            SPLICE_F_MOVE | SPLICE_F_NONBLOCK | SPLICE_F_MORE);

        if (ret > 0) {
            totalBytes += ret;
        } else if (ret == 0) {
            // EOF
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Pipe is empty (read) or full (write)
                // Wait for readiness using poll()
                struct pollfd pfds[2];
                pfds[0].fd = fdIn;   pfds[0].events = POLLIN;
                pfds[1].fd = fdOut;  pfds[1].events = POLLOUT;
                
                poll(pfds, 2, 100);  // 100ms timeout for stop_token check
                continue;
            }
            if (errno == EINTR) continue;
            // Fatal error
            break;
        }
    }
    
    return totalBytes;
}
#endif

} // namespace job
} // namespace ariash
