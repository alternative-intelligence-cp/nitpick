/**
 * Aria Garbage Collection System (AGCS) - Runtime Interface
 * 
 * This is the public API for Aria's hybrid generational garbage collector.
 * The GC implements a copying collector for the nursery (young generation)
 * and a mark-sweep collector for the old generation, with explicit support
 * for object pinning to enable safe interoperation with wild pointers.
 * 
 * Reference: research_021_garbage_collection_system.txt
 */

#ifndef ARIA_RUNTIME_GC_H
#define ARIA_RUNTIME_GC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Object Header (64-bit)
// =============================================================================

/**
 * ObjHeader: Metadata for every GC-managed allocation
 * 
 * This 64-bit header is prepended to every object in the GC heap.
 * The header uses bit-packing to minimize memory overhead while
 * supporting essential GC operations (marking, pinning, forwarding).
 * 
 * Layout (64 bits total):
 * - mark_bit (1): Set during major GC to identify reachable objects
 * - pinned_bit (1): Object cannot be moved (for wild pointer safety)
 * - forwarded_bit (1): Object has been evacuated, payload is forwarding address
 * - is_nursery (1): Object is in young generation
 * - size_class (8): Allocator bucket index for fast size lookup
 * - type_id (16): Runtime type identifier for precise scanning
 * - padding (36): Reserved for future use (identity hash, thin locks)
 */
typedef struct {
    uint64_t mark_bit : 1;        // Mark-sweep status
    uint64_t pinned_bit : 1;      // Address stability flag (#operator)
    uint64_t forwarded_bit : 1;   // Relocation flag (nursery evacuation)
    uint64_t is_nursery : 1;      // Generational tag
    uint64_t size_class : 8;      // Allocator bucket (256 size classes)
    uint64_t type_id : 16;        // Runtime type ID (65536 types)
    uint64_t padding : 36;        // Reserved
} ObjHeader;

// Compile-time assertion to ensure header is exactly 64 bits
static_assert(sizeof(ObjHeader) == 8, "ObjHeader must be 64 bits");

// =============================================================================
// Allocation API
// =============================================================================

/**
 * Allocate memory from the GC heap
 * 
 * This is the primary allocation function for GC-managed objects.
 * Allocation occurs in the nursery (young generation) using a bump
 * pointer allocator. When the nursery is full, a minor GC is triggered.
 * 
 * @param size Size in bytes (excluding header)
 * @param type_id Runtime type identifier for precise scanning
 * @return Pointer to allocated memory (after header), or NULL on OOM
 * 
 * Thread Safety: Safe for concurrent use with thread-local allocation
 * buffers (TLABs). Falls back to synchronized allocation on TLAB exhaustion.
 */
void* aria_gc_alloc(size_t size, uint16_t type_id);

/**
 * Pin a GC object to prevent relocation
 * 
 * Sets the pinned_bit in the object header. Pinned objects:
 * - Are NOT moved during nursery evacuation
 * - Are NOT compacted during major GC
 * - Can be safely referenced by wild pointers
 * 
 * This operation is idempotent (safe to call multiple times).
 * 
 * @param ptr Pointer to GC object (must not be NULL)
 * 
 * Usage: wild T@:ptr = #gc_obj  // Compiler calls aria_gc_pin(gc_obj)
 * 
 * Safety: The # operator is the only safe way to convert a GC reference
 * to a wild pointer. The compiler enforces that pinned objects are not
 * unpinned while wild references exist (Appendage Theory).
 */
void aria_gc_pin(void* ptr);

/**
 * Unpin a GC object (allow relocation again)
 * 
 * Clears the pinned_bit. Only safe when no wild pointers reference
 * the object. The Borrow Checker enforces this at compile time.
 * 
 * @param ptr Pointer to GC object
 * 
 * Note: In practice, unpinning is rarely done explicitly. Objects typically
 * remain pinned until reclaimed. Future optimization: reference counting
 * of wild pointers to enable automatic unpinning.
 */
void aria_gc_unpin(void* ptr);

// =============================================================================
// GC Trigger and Control
// =============================================================================

/**
 * Trigger garbage collection
 * 
 * @param full_collection If true, performs major GC (old generation).
 *                        If false, performs minor GC (nursery only).
 * 
 * The GC runs automatically when allocation fails, but can be invoked
 * manually for predictable latency or before timing-sensitive operations.
 * 
 * Semantics: Stop-the-world collection. All mutator threads are paused
 * at safepoints until the collection completes.
 */
void aria_gc_collect(bool full_collection);

/**
 * Get GC statistics
 * 
 * Provides insight into heap usage and GC performance. Useful for
 * debugging memory leaks or tuning GC parameters.
 */
typedef struct {
    size_t nursery_size;           // Total nursery capacity (bytes)
    size_t nursery_used;           // Current nursery utilization
    size_t old_gen_size;           // Old generation size
    size_t old_gen_used;           // Old generation utilization
    size_t total_allocated;        // Cumulative bytes allocated
    size_t total_collected;        // Cumulative bytes reclaimed
    uint64_t num_minor_collections; // Minor GC count
    uint64_t num_major_collections; // Major GC count
    size_t num_pinned_objects;     // Currently pinned objects
} GCStats;

void aria_gc_get_stats(GCStats* stats);

// =============================================================================
// Shadow Stack API (Root Tracking)
// =============================================================================

/**
 * Shadow Stack: Explicit root tracking for GC-managed references
 * 
 * The shadow stack is a parallel structure to the machine call stack.
 * It tracks pointers to GC objects in local variables. Unlike implicit
 * stack scanning (used by Java/Go), this approach provides:
 * - Portability: No backend-specific stack map generation
 * - Precision: Exact root identification (no conservative scanning)
 * - Safety: Roots cannot be missed due to register allocation
 * 
 * The compiler injects calls to these functions automatically.
 */

/**
 * Push a new shadow stack frame
 * 
 * Called at function entry for functions with GC-managed locals.
 * Allocates space for root tracking in the current activation record.
 * 
 * Compiler Injection: At function prologue
 */
void aria_shadow_stack_push_frame(void);

/**
 * Pop the current shadow stack frame
 * 
 * Called at function exit (all return paths). Discards roots for
 * the current activation record.
 * 
 * Compiler Injection: At function epilogue (all exit blocks)
 */
void aria_shadow_stack_pop_frame(void);

/**
 * Register a root in the current frame
 * 
 * Called when a GC-managed variable is declared or updated.
 * The pointer address (not value) is stored in the shadow frame.
 * 
 * @param root_addr Address of the stack variable (e.g., &x)
 * 
 * Example Lowering:
 *   obj:x = ...;  // Aria source
 *   void* x = aria_gc_alloc(...);  // LLVM IR
 *   aria_shadow_stack_add_root(&x);  // Root registration
 * 
 * Note: For dyn variables (which can change type at runtime), roots
 * are registered/deregistered dynamically as the variable transitions
 * between reference and primitive types.
 */
void aria_shadow_stack_add_root(void** root_addr);

/**
 * Remove a root from the current frame
 * 
 * Called when a GC variable goes out of scope within a function,
 * or when a dyn variable transitions to a non-reference type.
 * 
 * @param root_addr Address of the stack variable to deregister
 */
void aria_shadow_stack_remove_root(void** root_addr);

// =============================================================================
// Write Barrier API (Generational GC Support)
// =============================================================================

/**
 * Write barrier: Track old-to-young references
 * 
 * Called after every pointer store into a GC object. Maintains the
 * Card Table, which identifies old generation memory regions that
 * reference nursery objects. This ensures that minor GCs correctly
 * identify all roots without scanning the entire old generation.
 * 
 * @param obj Address of the object being written to
 * @param ref Address of the reference being stored (the new value)
 * 
 * Implementation: Uses a Card Table (byte array mapping 512-byte regions).
 * If obj is in the old generation, marks the corresponding card as DIRTY.
 * 
 * Compiler Injection:
 *   obj.field = value;  // Aria source
 *   *field_addr = value;  // LLVM store
 *   aria_gc_write_barrier(obj, value);  // Barrier
 * 
 * Optimization: For !is_nursery(obj), the barrier is a no-op at runtime.
 */
void aria_gc_write_barrier(void* obj, void* ref);

// =============================================================================
// Internal Utilities (for testing/debugging)
// =============================================================================

/**
 * Get the ObjHeader for a GC object
 * 
 * @param ptr Pointer to GC object payload
 * @return Pointer to the header (ptr - sizeof(ObjHeader))
 * 
 * Warning: Internal function. Direct header manipulation can corrupt
 * GC state. Only use for debugging or testing.
 */
ObjHeader* aria_gc_get_header(void* ptr);

/**
 * Check if a pointer is in the GC heap
 * 
 * @param ptr Arbitrary pointer
 * @return true if ptr is in nursery or old generation
 * 
 * Useful for assertions and debugging.
 */
bool aria_gc_is_heap_pointer(void* ptr);

// =============================================================================
// GC Initialization and Shutdown
// =============================================================================

/**
 * Initialize the garbage collector
 * 
 * Must be called before any GC allocations. Typically invoked by
 * the runtime startup code (aria_main initialization).
 * 
 * @param nursery_size Initial nursery size (bytes, default: 4MB)
 * @param old_gen_threshold Major GC trigger threshold (bytes, default: 64MB)
 * 
 * This function is idempotent (safe to call multiple times).
 */
void aria_gc_init(size_t nursery_size, size_t old_gen_threshold);

/**
 * Shutdown the garbage collector
 * 
 * Frees all GC heap memory. Called at process exit.
 * 
 * Warning: After shutdown, aria_gc_alloc will fail. Only call this
 * during final cleanup.
 */
void aria_gc_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_GC_H
