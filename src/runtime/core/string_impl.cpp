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
    // SSO (Small String Optimization) capacity:
    // Total struct size is 24 bytes (3 uint64_t or equivalent)
    // One byte is used for size/flag, leaving 23 bytes for data
    static const size_t SSO_CAPACITY = 23;

    union {
        struct {
            char* ptr;        // Pointer to remote buffer (GC heap)
            uint64_t size;    // Current length
            uint64_t capacity;// Current allocation size
        } heap;

        struct {
            char data[SSO_CAPACITY]; // Inline storage (23 bytes)
            uint8_t size_byte;       // Size stored in last byte (1 byte)
                                     // High bit = 0 means SSO mode
                                     // Total: 24 bytes matching heap layout
        } sso;
    } storage;

    // Helper to detect if we are in SSO mode.
    // We use the high bit of the last byte:
    // - If (last_byte & 0x80) == 0: SSO mode (size <= 23)
    // - If (last_byte & 0x80) != 0: Heap mode
    bool is_sso() const {
        // Access the last byte of the storage union
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&storage);
        const uint8_t last_byte = bytes[sizeof(storage) - 1];
        return (last_byte & 0x80) == 0;
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

        // Mark as Heap mode: Set high bit of last byte to 1
        // Since capacity uses uint64_t, we can safely set the high bit
        // of the last byte without affecting capacity values < 2^63
        str->storage.heap.capacity |= (1ULL << 63);
    }

    return str;
}

// Concatenates two strings
// string:res = a + b;
AriaString* aria_string_concat(AriaString* a, AriaString* b) {
    // Extract size, masking the SSO flag from size_byte if needed
    size_t len_a = a->is_sso()? (a->storage.sso.size_byte & 0x7F) : a->storage.heap.size;
    size_t len_b = b->is_sso()? (b->storage.sso.size_byte & 0x7F) : b->storage.heap.size;
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

        // Mark as Heap mode
        res->storage.heap.capacity |= (1ULL << 63);
    }
    
    return res;
}

// Access character at index (safe)
// char c = str[i];
int8_t aria_string_get_at(AriaString* str, uint64_t index) {
    // Extract size, masking the SSO flag
    size_t len = str->is_sso()? (str->storage.sso.size_byte & 0x7F) : str->storage.heap.size;
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

