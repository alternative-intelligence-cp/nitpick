// Async file stream implementation

#include "runtime/async/file_stream.h"
#include <fcntl.h>
#include <unistd.h>

namespace npk {
namespace runtime {

// Result structure for file read operations (same as in io_uring.cpp)
struct ReadResult {
    uint8_t* data;
    size_t size;
};

FileStream::FileStream(IoUringBackend* backend, const std::string& path, size_t chunk_size)
    : backend(backend), path(path), chunk_size(chunk_size), offset(0),
      fd(-1), is_open(false), is_closed(false), reached_eof(false),
      pending_read(nullptr), chunk_index(0) {}

FileStream::~FileStream() {
    close();
}

bool FileStream::open() {
    if (is_open || is_closed) {
        return false;
    }
    
    // Open file using io_uring backend
    Future* open_future = backend->open_file(path, O_RDONLY, 0644);
    
    // Wait for open to complete
    while (!open_future->isReady()) {
        backend->process_completions();
    }
    
    int* result_fd = (int*)open_future->getValue();
    fd = *result_fd;
    delete open_future;
    
    if (fd < 0) {
        return false;
    }
    
    is_open = true;
    return true;
}

Future* FileStream::next() {
    Future* result_future = new Future(sizeof(StreamItem<uint8_t>));
    
    // If closed or error, return end
    if (is_closed || !is_open || reached_eof) {
        StreamItem<uint8_t> end = StreamItem<uint8_t>::end();
        result_future->setValue(&end, sizeof(end));
        return result_future;
    }
    
    // If we have bytes in current chunk, return next byte
    if (chunk_index < current_chunk.size()) {
        StreamItem<uint8_t> item(current_chunk[chunk_index++]);
        result_future->setValue(&item, sizeof(item));
        return result_future;
    }
    
    // Need to read next chunk
    current_chunk.clear();
    chunk_index = 0;
    
    // Issue read request
    Future* read_future = backend->read_file(path, offset, chunk_size);
    
    // Wait for read to complete
    while (!read_future->isReady()) {
        backend->process_completions();
    }
    
    ReadResult* read_result = (ReadResult*)read_future->getValue();
    
    if (read_result->size == 0 || read_result->data == nullptr) {
        // EOF or error
        reached_eof = true;
        delete read_future;
        
        StreamItem<uint8_t> end = StreamItem<uint8_t>::end();
        result_future->setValue(&end, sizeof(end));
        return result_future;
    }
    
    // Store chunk data
    current_chunk.assign(read_result->data, read_result->data + read_result->size);
    offset += read_result->size;
    
    // Free the read buffer
    free(read_result->data);
    delete read_future;
    
    // Return first byte from new chunk
    if (current_chunk.size() > 0) {
        StreamItem<uint8_t> item(current_chunk[chunk_index++]);
        result_future->setValue(&item, sizeof(item));
    } else {
        // Empty chunk = EOF
        reached_eof = true;
        StreamItem<uint8_t> end = StreamItem<uint8_t>::end();
        result_future->setValue(&end, sizeof(end));
    }
    
    return result_future;
}

void FileStream::close() {
    if (is_closed) {
        return;
    }
    
    is_closed = true;
    
    if (is_open && fd >= 0) {
        // Close file using io_uring backend
        Future* close_future = backend->close_file(fd);
        
        // Wait for close to complete
        while (!close_future->isReady()) {
            backend->process_completions();
        }
        
        delete close_future;
        fd = -1;
        is_open = false;
    }
    
    current_chunk.clear();
}

} // namespace runtime
} // namespace npk
