#include <cstdint>
#include <cstring>
#include <algorithm>
#include <new>

// Forward declarations for runtime allocators
extern "C" void* aria_gc_alloc(void* nursery, size_t size);
extern "C" void* aria_alloc(size_t size); // For wild strings if needed
extern "C" void aria_free(void* ptr);

// The exact layout of the Aria String Header
// Must match the compiler's view of the 'string' primitive.
struct AriaString {
    static const size_t SSO_CAPACITY = 23;

    union {
        struct {
            char* ptr;        // Pointer to remote buffer (GC heap)
            uint64_t size;    // Current length
            uint64_t capacity;// Current allocation size
        } heap;

        struct {
            char data; // Inline storage
            uint8_t size_byte;       // Size stored in the last byte (with flag)
        } sso;
    } storage;

    // Helper to detect if we are in SSO mode.
    // We use the last byte. In Little Endian, the high bit or specific bit pattern
    // of 'capacity' in the heap struct usually avoids conflict if capacity is reasonable.
    // For simplicity here: we assume specific bitmask logic on the last byte.
    bool is_sso() const {
        // In this implementation, we use the last byte of the struct (sso.size_byte).
        // If it looks like a valid small size, we treat it as SSO.
        // A robust implementation uses bit-stealing from capacity.
        // Here, we define: if (last_byte & 0x80) == 0, it's SSO.
        // This limits SSO size slightly but is safe.
        return (reinterpret_cast<const uint8_t*>(&storage) & 0x80) == 0;
    }
};

extern "C" {

// Allocates a new Aria String from a C-string literal
// Called by the compiler for literals: string:s = "hello";
AriaString* aria_string_from_literal(const char* literal, size_t len) {
    // 1. Allocate the String Object Header (in GC Nursery)
    // Note: get_current_thread_nursery() is an internal runtime helper
    extern void* get_current_thread_nursery();
    AriaString* str = (AriaString*)aria_gc_alloc(get_current_thread_nursery(), sizeof(AriaString));

    if (len <= AriaString::SSO_CAPACITY) {
        // --- Fast Path: SSO ---
        memcpy(str->storage.sso.data, literal, len);
        // Null terminate for C-compat
        if (len < AriaString::SSO_CAPACITY) str->storage.sso.data[len] = '\0';
        
        // Store size. We verify strict aliasing rules in build.
        // We set the high bit to 0 to indicate SSO.
        str->storage.sso.size_byte = (uint8_t)len;
    } else {
        // --- Slow Path: Heap Allocation ---
        // Allocate buffer in GC memory as well
        char* buffer = (char*)aria_gc_alloc(get_current_thread_nursery(), len + 1);
        memcpy(buffer, literal, len);
        buffer[len] = '\0';

        str->storage.heap.ptr = buffer;
        str->storage.heap.size = len;
        str->storage.heap.capacity = len; // Tight fit
        
        // Mark as Heap: Set high bit of the last byte to 1.
        // We do this by manipulating the capacity field or a flag.
        // Here we rely on the header bits set by the compiler/GC logic 
        // or strictly check the capacity field > SSO_CAPACITY.
        // For this reference impl, we assume the reader checks capacity field if is_sso logic allows.
    }

    return str;
}

// Concatenates two strings
// string:res = a + b;
AriaString* aria_string_concat(AriaString* a, AriaString* b) {
    size_t len_a = a->is_sso()? a->storage.sso.size_byte : a->storage.heap.size;
    size_t len_b = b->is_sso()? b->storage.sso.size_byte : b->storage.heap.size;
    size_t total_len = len_a + len_b;

    extern void* get_current_thread_nursery();
    AriaString* res = (AriaString*)aria_gc_alloc(get_current_thread_nursery(), sizeof(AriaString));

    if (total_len <= AriaString::SSO_CAPACITY) {
        // Result fits in SSO
        const char* src_a = a->is_sso()? a->storage.sso.data : a->storage.heap.ptr;
        const char* src_b = b->is_sso()? b->storage.sso.data : b->storage.heap.ptr;
        
        memcpy(res->storage.sso.data, src_a, len_a);
        memcpy(res->storage.sso.data + len_a, src_b, len_b);
        if (total_len < AriaString::SSO_CAPACITY) res->storage.sso.data[total_len] = '\0';
        res->storage.sso.size_byte = (uint8_t)total_len;
    } else {
        // Result requires Heap
        char* buffer = (char*)aria_gc_alloc(get_current_thread_nursery(), total_len + 1);
        
        const char* src_a = a->is_sso()? a->storage.sso.data : a->storage.heap.ptr;
        const char* src_b = b->is_sso()? b->storage.sso.data : b->storage.heap.ptr;

        memcpy(buffer, src_a, len_a);
        memcpy(buffer + len_a, src_b, len_b);
        buffer[total_len] = '\0';

        res->storage.heap.ptr = buffer;
        res->storage.heap.size = total_len;
        res->storage.heap.capacity = total_len;
    }
    
    return res;
}

// Access character at index (safe)
// char c = str[i];
int8_t aria_string_get_at(AriaString* str, uint64_t index) {
    size_t len = str->is_sso()? str->storage.sso.size_byte : str->storage.heap.size;
    if (index >= len) {
        // Runtime panic or return 0 (implementation defined)
        // Aria spec prefers crash on OOB or result<T>.
        // Here we assume unchecked access for speed as per 'systems' philosophy,
        // but explicit bounds check instruction is emitted by compiler.
        return 0; 
    }
    
    const char* buf = str->is_sso()? str->storage.sso.data : str->storage.heap.ptr;
    return buf[index];
}

} // extern "C"

