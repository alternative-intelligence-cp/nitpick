// Runtime Future<T> implementation
// Represents the result of an async computation
// Reference: research_029 Section 4 (Future Trait & Poll)

#ifndef ARIA_RUNTIME_ASYNC_FUTURE_H
#define ARIA_RUNTIME_ASYNC_FUTURE_H

#include <atomic>   // For atomic state transitions (BUG-06 fix)
#include <cstdint>
#include <cstdio>   // For fprintf in buffer overflow check
#include <cstdlib>  // For malloc/free
#include <cstring>
#include <exception> // For std::terminate

namespace aria {
namespace runtime {

/**
 * FutureState - State of a Future computation
 */
enum class FutureState {
    PENDING,    // Not yet completed
    READY,      // Completed with value
    ERROR       // Completed with error
};

/**
 * PollResult - Result of polling a Future
 */
enum class PollResult {
    PENDING,    // Future not ready yet, suspend
    READY       // Future ready, value available
};

/**
 * Future<T> - Runtime representation of async computation result
 * 
 * Generic over type T using void* for type erasure
 * Actual type information tracked separately in type system
 * 
 * Layout:
 *   - state: Current future state (PENDING/READY/ERROR)
 *   - value: Pointer to result value (type T)
 *   - error: Error flag for TBB ERR propagation
 * 
 * Reference: research_029 Section 4 (Future Trait)
 */
class Future {
private:
    std::atomic<FutureState> state;  // AUDIT FIX BUG-06: Atomic for thread-safe state transitions
    void* value;          // Points to T (type-erased)
    bool hasError;        // For TBB ERR propagation
    size_t valueSize;     // Size of T for memory management
    
public:
    Future(size_t typeSize = 0)
        : state(FutureState::PENDING), value(nullptr), 
          hasError(false), valueSize(typeSize) {
        // Allocate storage for value if size known
        if (valueSize > 0) {
            value = malloc(valueSize);
        }
    }
    
    ~Future() {
        if (value) {
            free(value);
            value = nullptr;
        }
    }
    
    // Disable copy (Futures are move-only)
    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;
    
    // Enable move
    Future(Future&& other) noexcept
        : state(other.state.load(std::memory_order_relaxed)),  // Load from atomic
          value(other.value), 
          hasError(other.hasError), 
          valueSize(other.valueSize) {
        other.value = nullptr;
        other.valueSize = 0;
    }
    
    /**
     * Poll the future to check if ready
     * 
     * Called by await expressions to check completion status
     * Returns PENDING if not ready (caller should suspend)
     * Returns READY if value available (caller can extract)
     */
    PollResult poll() {
        // AUDIT FIX BUG-06: Acquire semantics - see value writes if we see READY
        FutureState current = state.load(std::memory_order_acquire);
        if (current == FutureState::READY || current == FutureState::ERROR) {
            return PollResult::READY;
        }
        return PollResult::PENDING;
    }
    
    /**
     * Check if future is ready
     */
    bool isReady() const {
        FutureState current = state.load(std::memory_order_acquire);
        return current == FutureState::READY || current == FutureState::ERROR;
    }
    
    /**
     * Check if future is pending
     */
    bool isPending() const {
        return state.load(std::memory_order_acquire) == FutureState::PENDING;
    }
    
    /**
     * Check if future has error
     */
    bool hasErrorFlag() const {
        return hasError;
    }
    
    /**
     * Get current state
     */
    FutureState getState() const {
        return state.load(std::memory_order_acquire);
    }
    
    /**
     * Set value (marks future as ready)
     * @param val Pointer to value of type T
     * @param size Size of value in bytes
     */
    void setValue(const void* val, size_t size) {
        // AUDIT FIX BUG-07: Bounds check for buffer overflow prevention
        // For SIL-4 safety: fail hard rather than corrupt memory
        if (valueSize > 0 && size > valueSize) {
            fprintf(stderr, 
                "FATAL: Future buffer overflow detected.\n"
                "  Allocated: %zu bytes\n"
                "  Required:  %zu bytes\n"
                "  This indicates a type mismatch in async code.\n",
                valueSize, size);
            std::terminate();
        }
        
        // Allocate if needed (first-time initialization)
        if (!value && size > 0) {
            value = malloc(size);
            valueSize = size;
        }
        
        // Safe to copy - bounds checked above
        if (value && val) {
            memcpy(value, val, size);
        }
        
        // AUDIT FIX BUG-06: Release semantics - ensure value writes visible before READY
        state.store(FutureState::READY, std::memory_order_release);
    }
    
    /**
     * Set error (marks future as error)
     */
    void setError(bool error = true) {
        hasError = error;
        // AUDIT FIX BUG-06: Release semantics for error state
        state.store(FutureState::ERROR, std::memory_order_release);
    }
    
    /**
     * Get value pointer (type-erased)
     * Caller must cast to appropriate type
     */
    void* getValue() const {
        return value;
    }
    
    /**
     * Extract value (move semantics)
     * Future becomes invalid after extraction
     */
    void* extractValue() {
        void* result = value;
        value = nullptr;
        valueSize = 0;
        return result;
    }
    
    /**
     * Get value size
     */
    size_t getValueSize() const {
        return valueSize;
    }
};

/**
 * FutureBox - Boxed future for heap allocation
 * Used when futures need to outlive stack frames
 */
class FutureBox {
private:
    Future* future;
    
public:
    FutureBox(size_t typeSize = 0) {
        future = new Future(typeSize);
    }
    
    ~FutureBox() {
        delete future;
    }
    
    // Disable copy
    FutureBox(const FutureBox&) = delete;
    FutureBox& operator=(const FutureBox&) = delete;
    
    Future* get() const { return future; }
    Future* operator->() const { return future; }
};

} // namespace runtime
} // namespace aria

#endif // ARIA_RUNTIME_ASYNC_FUTURE_H
