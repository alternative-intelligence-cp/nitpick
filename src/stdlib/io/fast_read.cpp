#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>

// Define buffer size: 64KB is optimal for modern pipe throughput
#define IO_BUFFER_SIZE 65536

struct AriaStream {
    int fd;
    unsigned char* buffer;
    size_t pos;
    size_t available;
    bool eof;
};

// Internal helper to refill buffer
static bool refill(AriaStream* s) {
    if (s->eof) return false;
    
    ssize_t bytes_read = read(s->fd, s->buffer, IO_BUFFER_SIZE);
    if (bytes_read <= 0) {
        s->eof = true;
        return false;
    }
    
    s->pos = 0;
    s->available = (size_t)bytes_read;
    return true;
}

extern "C" {

// Initialize a buffered reader for a raw file descriptor
// wild stream:s = openFile(...);
AriaStream* aria_io_create_reader(int fd) {
    // We use mimalloc (aria_alloc) for the stream struct and buffer
    // ensuring wild heap performance.
    extern void* aria_alloc(size_t);
    
    AriaStream* s = (AriaStream*)aria_alloc(sizeof(AriaStream));
    s->fd = fd;
    s->buffer = (unsigned char*)aria_alloc(IO_BUFFER_SIZE);
    s->pos = 0;
    s->available = 0;
    s->eof = false;
    return s;
}

// Cleanup
void aria_io_close(AriaStream* s) {
    extern void aria_free(void*);
    if (!s) return;
    
    close(s->fd);
    aria_free(s->buffer);
    aria_free(s);
}

// Reads until a delimiter (e.g., '\n') is found.
// Returns a managed string (because it returns to user land).
void* aria_io_read_until(AriaStream* s, char delimiter) {
    extern void* aria_string_from_literal(const char*, size_t); // Reuse string impl
    extern void* get_current_thread_nursery();
    extern void* aria_gc_alloc(void*, size_t);

    // Dynamic buffer to hold the line. Since we don't know the length,
    // we might need a growable vector. For simplicity in this systems impl,
    // we use a temporary stack/wild buffer or assume a max line length.
    // Let's implement a growing strategy.
    
    size_t capacity = 128;
    size_t len = 0;
    char* line = (char*)malloc(capacity); // Use raw malloc for temp growable
    
    while (true) {
        // If buffer empty, refill
        if (s->pos >= s->available) {
            if (!refill(s)) break; // EOF
        }
        
        // Scan for delimiter in the available buffer chunk
        unsigned char* start = s->buffer + s->pos;
        unsigned char* end = s->buffer + s->available;
        unsigned char* found = (unsigned char*)memchr(start, delimiter, end - start);
        
        size_t chunk_len;
        bool found_delim = (found!= nullptr);
        
        if (found_delim) {
            chunk_len = found - start;
        } else {
            chunk_len = s->available - s->pos;
        }
        
        // Append chunk to line
        if (len + chunk_len >= capacity) {
            capacity *= 2;
            if (capacity < len + chunk_len) capacity = len + chunk_len + 128;
            line = (char*)realloc(line, capacity);
        }
        memcpy(line + len, start, chunk_len);
        len += chunk_len;
        
        // Advance stream
        s->pos += chunk_len;
        
        if (found_delim) {
            s->pos++; // Skip the delimiter
            break;
        }
    }
    
    if (len == 0 && s->eof) {
        free(line);
        return nullptr; // EOF/Null
    }
    
    // Create final Aria String (GC managed)
    void* str_obj = aria_string_from_literal(line, len);
    free(line); // Free temp buffer
    return str_obj;
}

// Bulk binary read
// buffer:b = stream.readBytes(1024);
size_t aria_io_read_bytes(AriaStream* s, void* dest, size_t count) {
    size_t total_read = 0;
    char* out_ptr = (char*)dest;
    
    while (count > 0) {
        if (s->pos >= s->available) {
            if (!refill(s)) break;
        }
        
        size_t can_copy = std::min(count, s->available - s->pos);
        memcpy(out_ptr, s->buffer + s->pos, can_copy);
        
        s->pos += can_copy;
        out_ptr += can_copy;
        total_read += can_copy;
        count -= can_copy;
    }
    
    return total_read;
}

} // extern "C"

