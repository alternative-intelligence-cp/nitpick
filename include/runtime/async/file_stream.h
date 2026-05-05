// Async file stream using io_uring
// Read files as async byte streams

#ifndef ARIA_RUNTIME_ASYNC_FILE_STREAM_H
#define ARIA_RUNTIME_ASYNC_FILE_STREAM_H

#include "runtime/async/async_stream.h"
#include "runtime/async/io_uring.h"
#include <string>
#include <cstdint>

namespace npk {
namespace runtime {

/**
 * Async file stream - reads file as stream of bytes
 * 
 * Uses io_uring backend for high-performance async reads.
 * Reads file in chunks with configurable chunk size.
 */
class FileStream : public AsyncStream<uint8_t> {
private:
    IoUringBackend* backend;
    std::string path;
    size_t chunk_size;
    uint64_t offset;
    int fd;
    bool is_open;
    bool is_closed;
    bool reached_eof;
    
    // Current chunk being read
    Future* pending_read;
    std::vector<uint8_t> current_chunk;
    size_t chunk_index;
    
public:
    /**
     * Create file stream
     * @param backend io_uring backend for async I/O
     * @param path File path to read
     * @param chunk_size Size of chunks to read (default 4KB)
     */
    FileStream(IoUringBackend* backend, const std::string& path, size_t chunk_size = 4096);
    
    /**
     * Destructor - closes file if open
     */
    ~FileStream() override;
    
    /**
     * Open the file
     * @return true on success
     */
    bool open();
    
    /**
     * Get next byte from stream
     * @return Future<StreamItem<uint8_t>> - next byte or end
     */
    Future* next() override;
    
    /**
     * Close stream and file
     */
    void close() override;
    
    /**
     * Check if stream is closed
     */
    bool isClosed() const { return is_closed; }
    
    /**
     * Get current read offset
     */
    uint64_t getOffset() const { return offset; }
};

} // namespace runtime
} // namespace npk

#endif // ARIA_RUNTIME_ASYNC_FILE_STREAM_H
