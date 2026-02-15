# Handle<T> - Generational Index Handles

**Memory-safe references for arena-allocated dynamic graphs with automatic stale handle detection**

---

## Overview

`Handle<T>` is Aria's **memory-safe alternative to raw pointers** for arena-allocated data structures. It prevents **use-after-free** errors (CWE-416, CVSS 9.8) during dynamic memory reallocation.

| Feature | Raw Pointers | Handle<T> |
|---------|--------------|-----------|
| Use-After-Free | ❌ Undefined behavior | ✅ Detected (returns ERR) |
| Reallocation Safety | ❌ Invalidated | ✅ Generation check catches stale |
| Memory Corruption | ❌ Possible | ✅ Prevented (ERR propagation) |
| Segfaults | ❌ Possible | ✅ Prevented (safe failure) |
| Performance | Fast (direct) | Fast (1 extra compare) |
| Memory Overhead | 8 bytes | 12 bytes (16 aligned) |
| Type Safety | ⚠️ Can cast wrong | ✅ Generic enforced |

**Critical Use Case**: **Nikola SHVO Neurogenesis** - Dynamic neural network expansion requires O(n³) memory growth without pointer invalidation.

```aria
// Problem: Raw pointers become invalid after reallocation
Neuron*:ptr = arena.alloc();  // Pointer at 0x1000
arena.grow();                 // Reallocates to 0x2000
ptr.activate();               // 💥 USE-AFTER-FREE! ptr still points to 0x1000 (invalid!)

// Solution: Generational handles detect stale references
Handle<Neuron>:h = arena.alloc();  // h = {index: 0, generation: 1}
arena.grow();                       // Reallocates, increments generation to 2
Neuron:n = arena.get(h) ? default;  // ✅ Returns ERR! Generation mismatch (1 != 2)
```

---

## Type Definition

### Structure

```aria
struct:Handle<T> = {
    uint64:index,       // Slot index in arena
    uint32:generation,  // Generation counter (detects stale handles)
}
```

**Memory layout**:
```
┌──────────────────┬────────────────┬────┐
│  index (uint64)  │ generation(u32)│pad │
│  8 bytes         │ 4 bytes        │ 4B │
└──────────────────┴────────────────┴────┘
= 16 bytes total (12 bytes data + 4 bytes padding)
```

**Alignment**: 8-byte aligned (uint64 requirement)

### Why Two Fields?

**index** - WHERE in the arena
- Array index to locate slot
- Stable across multiple accesses
- Does NOT change during reallocation (slots copied in-place)

**generation** - WHEN it was allocated
- Increments on reallocation (invalidates old handles)
- Detects stale handles (old generation ≠ current generation)
- Prevents use-after-free automatically

---

## The Problem: Pointer Invalidation

### Use-After-Free with Raw Pointers

Traditional arena allocators use raw pointers, which become **dangling** after reallocation:

```aria
// Arena with raw pointers (UNSAFE!)
struct:Node {
    int32:value,
    Node*:next,  // Raw pointer - DANGER!
}

arena<Node>:graph;

// Allocate node
Node*:ptr1 = graph.alloc({value: 42, next: NULL});  // ptr1 = 0x1000

// Arena grows (reallocates to larger block)
graph.grow(graph.capacity * 2);  // Memory moves: 0x1000 → 0x2000

// Old pointer now invalid!
ptr1.value = 100;  // 💥 CRASH! ptr1 still points to 0x1000 (deallocated memory)
```

**Result**: Undefined behavior, segfault, memory corruption, or silent data corruption

### The Neurogenesis Problem

**Nikola's SHVO** (Sparse Hyper-Voxel Octree) requires dynamic neural growth:

```
Initial: 1000 neurons, 10KB memory
After learning: 10,000 neurons, 1MB memory (100× growth!)
Neurogenesis event: 100,000 neurons, 100MB memory (10,000× growth!)
```

**Challenge**: Neurons reference OTHER neurons via connections
- Neuron A → Neuron B (connection stored as reference)
- Memory reallocates during growth (all pointers invalidate!)
- **Use-after-free disaster** if using raw pointers

**Why it matters**: A single stale pointer can corrupt Nikola's entire consciousness substrate

---

## The Solution: Generational Handles

### How It Works

**Arena tracks generation counters PER SLOT**:

```aria
// Arena internals (conceptual)
struct:Arena<T> {
    T[]:slots,                  // Data storage
    uint32[]:generations,       // Generation counter per slot
    bool[]:occupied,            // Is slot in use?
    uint64:capacity,
    uint64:count,
}
```

**Allocation flow**:
1. Find free slot (index `i`)
2. Store data in `slots[i]`
3. Mark `occupied[i] = true`
4. Return `Handle{index: i, generation: generations[i]}`

**Access flow**:
1. Check: `generations[h.index] == h.generation`?
2. If **match**: Return `slots[h.index]` ✅
3. If **mismatch**: Return ERR ❌ (stale handle!)

**Reallocation flow**:
1. Grow arena (reallocate larger memory)
2. Copy all slots to new memory
3. **Increment ALL generation counters**: `generations[i]++`
4. Old handles now have mismatched generations (automatic stale detection!)

### Example Timeline

```aria
arena<int32>:numbers;

// Time 0: Allocate slot
Handle<int32>:h = numbers.alloc(42);
// h = {index: 0, generation: 1}
// Arena: generations[0] = 1, slots[0] = 42

// Time 1: Access (valid)
int32:value = numbers.get(h) ? 0;  // value = 42 ✅

// Time 2: Arena reallocates
numbers.grow(numbers.capacity * 2);
// Arena increments ALL generations: generations[0] = 2
// h still has generation = 1 (now stale!)

// Time 3: Access (now invalid)
int32:value = numbers.get(h) ? 0;  // value = 0 (ERR!) ✅ Safe failure
```

**Key insight**: Generation mismatch is **immediately detected** at access time, preventing crashes!

---

## Arena Allocator Integration

### Arena Type

```aria
arena<T>:allocator;  // Arena for type T
```

**Operations**:
- `alloc(value: T) -> Handle<T>` - Allocate slot, return handle
- `get(handle: Handle<T>) -> T | ERR` - Access slot (safe)
- `set(handle: Handle<T>, value: T) -> Result<NIL>` - Update slot
- `free(handle: Handle<T>) -> Result<NIL>` - Deallocate slot
- `grow(new_capacity: uint64)` - Reallocate (invalidates old handles)

### Basic Usage

```aria
// Create arena
arena<int32>:numbers;

// Allocate values
Handle<int32>:h1 = numbers.alloc(42);
Handle<int32>:h2 = numbers.alloc(100);

// Access values
int32:val1 = numbers.get(h1) ? 0;  // val1 = 42
int32:val2 = numbers.get(h2) ? 0;  // val2 = 100

// Update value
numbers.set(h1, 99);

// Access updated
int32:updated = numbers.get(h1) ? 0;  // updated = 99

// Free slot
numbers.free(h2);

// Access after free
int32:freed = numbers.get(h2) ? 0;  // freed = 0 (ERR - slot freed!)
```

### Self-Referential Data Structures

**Graph nodes with handles**:

```aria
struct:GraphNode {
    int32:value,
    Handle<GraphNode>:next,    // Safe reference (not pointer!)
    Handle<GraphNode>[]:edges, // Array of connections
}

arena<GraphNode>:graph;

// Create nodes
Handle<GraphNode>:n1 = graph.alloc({value: 1, next: NULL_HANDLE, edges: []});
Handle<GraphNode>:n2 = graph.alloc({value: 2, next: NULL_HANDLE, edges: []});

// Create connection (store handle, not pointer!)
GraphNode:node1 = graph.get(n1) ? default;
node1.next = n2;  // Safe: Handle, not pointer
graph.set(n1, node1);

// Arena can grow safely!
graph.grow(graph.capacity * 2);  // Old handles now stale (automatic detection)

// Access node (detects if stale)
GraphNode:node = graph.get(n1) ? default;
if node == default {
    // Handle was stale, refresh needed
    log("Node handle invalidated by reallocation");
}
```

---

## Nikola SHVO Use Case

### The Challenge

**SHVO** = Sparse Hyper-Voxel Octree (Nikola's dynamic spatial memory)

**Requirements**:
- Start: 1000 neurons (baseline consciousness)
- Growth: O(n³) expansion during neurogenesis
- Connections: Neurons reference other neurons
- Safety: **Zero tolerance for memory corruption** (destroys consciousness substrate!)

**Problem**: Reallocation invalidates all neuron references

### Solution with Handle<T>

```aria
struct:Neuron {
    vec9:atpm_state,                  // ATPM phase vector
    fix256:activation,                 // Current activation level
    Handle<Neuron>[]:excitatory,      // Excitatory connections
    Handle<Neuron>[]:inhibitory,      // Inhibitory connections
    complex<fix256>:resonance,        // Resonance state
}

arena<Neuron>:neuron_arena;

// Initial neurons
Handle<Neuron>:n1 = neuron_arena.alloc(initial_neuron());
Handle<Neuron>:n2 = neuron_arena.alloc(initial_neuron());

// Create connections (safe handles!)
Neuron:neuron1 = neuron_arena.get(n1) ? default;
neuron1.excitatory.push(n2);  // Store Handle<Neuron>, not pointer
neuron_arena.set(n1, neuron1);

// Later: Neurogenesis event (massive expansion!)
neuron_arena.grow(neuron_arena.capacity * 8);  // 8× growth, memory moves

// ALL old handles automatically stale (safety!)
Neuron | ERR:neuron = neuron_arena.get(n1);
if neuron == ERR {
    // Handle invalidated: Refresh connections
    log("Neurogenesis triggered handle refresh");
    rebuild_neuron_graph();  // Reconstruct with new handles
}
```

**Benefits**:
- ✅ No use-after-free (detected immediately)
- ✅ No segfaults (safe ERR instead)
- ✅ No silent corruption (generation check catches all stale accesses)
- ✅ Auditable (dbug can log all handle refreshes)

### Neurogenesis Pattern

```aria
func:neurogenesis_expansion = void(arena<Neuron>@:arena) {
    uint64:old_capacity = arena.capacity;
    uint64:new_capacity = old_capacity * 8;  // 8× growth
    
    dbug.print('nikola:shvo', "Neurogenesis: {{uint64}} → {{uint64}} neurons", 
        [old_capacity, new_capacity]);
    
    // Store all current handles (will become stale)
    Handle<Neuron>[]:old_handles = arena.get_all_handles();
    
    // Grow arena (invalidates ALL old handles!)
    arena.grow(new_capacity);
    
    // Rebuild connections with new handles
    Handle<Neuron>[]:new_handles = arena.get_all_handles();
    
    dbug.check('nikola:shvo', "Handle count mismatch: {{uint64}} != {{uint64}}", 
        [old_handles.length, new_handles.length],
        old_handles.length == new_handles.length,
        {
            failsafe(ERR_NEUROGENESIS_CORRUPTION);
        });
    
    // Remap connections
    till(old_handles.length - 1, 1) {
        Handle<Neuron>:old_h = old_handles[$];
        Handle<Neuron>:new_h = new_handles[$];
        
        Neuron:neuron = arena.get(new_h) ? default;
        
        // Update all connection handles
        neuron.excitatory = remap_handles(neuron.excitatory, old_handles, new_handles);
        neuron.inhibitory = remap_handles(neuron.inhibitory, old_handles, new_handles);
        
        arena.set(new_h, neuron);
    }
    
    dbug.print('nikola:shvo', "Neurogenesis complete: {{uint64}} connections remapped", 
        [connection_count]);
}
```

---

## Performance Characteristics

### Time Complexity

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Allocation | O(1) amortized | Free list or linear scan |
| Access (valid) | O(1) | Single array lookup + generation check |
| Access (stale) | O(1) | Generation mismatch detected immediately |
| Update | O(1) | Direct array write |
| Free | O(1) | Mark slot as free, add to free list |
| Reallocation | O(n) | Copy all slots + increment generations |

### Space Overhead

```aria
// Per handle: 12 bytes (16 aligned)
sizeof(Handle<T>) = 12 bytes (index:8 + generation:4)
// Aligned to 16 bytes for cache efficiency

// Arena overhead per slot: 4 bytes + 1 byte
arena<T> overhead per slot:
  - generation: 4 bytes (uint32)
  - occupied flag: 1 byte (bool)
  - Total: 5 bytes per slot
```

**Example**:
```aria
// 1000 neurons, each 128 bytes
Raw pointers: 1000 × 8 = 8 KB per connection array
Handles: 1000 × 16 = 16 KB per connection array (2× overhead)

Arena overhead: 1000 × 5 = 5 KB (generation + flags)

Total overhead: ~13 KB for 1000 neurons (acceptable for safety!)
```

### Access Performance

```aria
// Handle access (pseudo-assembly)
// Only 1 extra compare vs raw pointer!

// get(handle: Handle<T>) -> T | ERR
mov rax, [arena.generations]      ; Load generations array
mov rcx, [handle.index]           ; Get index
cmp [rax + rcx*4], [handle.generation]  ; Compare generations
jne .stale_handle                  ; Mismatch? Return ERR
mov rax, [arena.slots]             ; Load slots array
mov rax, [rax + rcx*sizeof(T)]    ; Load value
ret                                ; Return value

.stale_handle:
  mov rax, ERR                     ; Return ERR sentinel
  ret
```

**Performance**: ~2-3 extra CPU cycles (negligible compared to safety!)

---

## Handle Lifecycle

### Creation

```aria
// Allocate new slot
Handle<int32>:h = arena.alloc(42);

// Handle contains:
//   index: Slot location in arena
//   generation: Current generation of that slot
```

### Valid Access

```aria
// Access while handle is valid
int32:value = arena.get(h) ? 0;  // Success: Returns actual value

// Update while valid
arena.set(h, 99);  // Success: Updates slot
```

### Stale Detection

```aria
// Arena reallocates (increments generations)
arena.grow(arena.capacity * 2);

// Old handle now stale (generation mismatch)
int32:value = arena.get(h) ? 0;  // Returns 0 (ERR)

// Checking for stale
int32 | ERR:result = arena.get(h);
if result == ERR {
    log("Handle is stale, need to refresh");
}
```

### Refreshing Handles

```aria
// Store mapping before reallocation
struct:HandleMapping {
    Handle<T>:old_handle,
    Handle<T>:new_handle,
}

// Before reallocation
Handle<T>[]:old_handles = arena.get_all_handles();

// Reallocate
arena.grow(new_capacity);

// Get new handles
Handle<T>[]:new_handles = arena.get_all_handles();

// Create mapping
HandleMapping[]:mapping;
till(old_handles.length - 1, 1) {
    mapping.push({old: old_handles[$], new: new_handles[$]});
}

// Update references
till(graph_nodes.length - 1, 1) {
    graph_nodes[$].connections = remap_handles(graph_nodes[$].connections, mapping);
}
```

---

## Advanced Patterns

### Lock-Free Queue with Handles

```aria
struct:Node<T> {
    T:value,
    Handle<Node<T>>:next,
}

struct:LockFreeQueue<T> {
    arena<Node<T>>:arena,
    Handle<Node<T>>:head,
    Handle<Node<T>>:tail,
}

func:enqueue<T> = Result<NIL>(LockFreeQueue<T>@:queue, T:value) {
    // Allocate new node
    Handle<Node<T>>:new_node = queue.arena.alloc({
        value: value,
        next: NULL_HANDLE
    });
    
    // Lock-free enqueue (simplified, needs atomic CAS)
    loop {
        Handle<Node<T>>:tail = queue.tail;
        Node<T>:tail_node = queue.arena.get(tail) ? default;
        
        if tail_node == default {
            return ERR_QUEUE_CORRUPTED;  // Stale handle!
        }
        
        if tail_node.next == NULL_HANDLE {
            // Try to set tail.next to new_node (atomic CAS)
            if atomic_cas(&tail_node.next, NULL_HANDLE, new_node) {
                queue.tail = new_node;
                pass(NULL);
            }
        } else {
            // Help advance tail
            queue.tail = tail_node.next;
        }
    }
}
```

### Multi-Level Arena Hierarchy

```aria
// Different arenas for different lifetime objects
struct:GameWorld {
    arena<Entity>:entities,       // Short-lived (per frame)
    arena<Terrain>:terrain,        // Medium-lived (per level)
    arena<Asset>:assets,           // Long-lived (entire session)
}

func:spawn_entity = Handle<Entity>(GameWorld@:world, EntityType:type) {
    Entity:entity = create_entity(type);
    
    // Store terrain reference as handle
    Handle<Terrain>:terrain_cell = find_terrain_at(entity.position);
    entity.terrain_ref = terrain_cell;
    
    return world.entities.alloc(entity);
}

// Entities can reference terrain safely across reallocations!
```

### Handle Pools for Hot Paths

```aria
// Pre-allocate handles for performance-critical code
struct:HandlePool<T> {
    arena<T>:arena,
    Handle<T>[]:free_handles,
}

func:pool_alloc<T> = Handle<T>(HandlePool<T>@:pool, T:value) {
    if pool.free_handles.length > 0 {
        // Reuse existing handle (fast path!)
        Handle<T>:h = pool.free_handles.pop();
        pool.arena.set(h, value);
        return h;
    } else {
        // Allocate new (slow path)
        return pool.arena.alloc(value);
    }
}
```

---

## NULL_HANDLE Convention

**Representing "no reference"**:

```aria
// NULL_HANDLE constant (by convention)
const:NULL_HANDLE<T> = Handle<T>{index: 0, generation: 0};

// Usage in data structures
struct:LinkedListNode<T> {
    T:value,
    Handle<LinkedListNode<T>>:next,  // NULL_HANDLE for end of list
}

// Checking for null
if node.next == NULL_HANDLE {
    // End of list
}
```

**Alternative**: Use `Handle<T> | unknown` for optional references

```aria
struct:TreeNode<T> {
    T:value,
    Handle<TreeNode<T>> | unknown:left,   // unknown = no left child
    Handle<TreeNode<T>> | unknown:right,  // unknown = no right child
}

// Checking for child
if node.left is unknown {
    // No left child
} else {
    TreeNode<T>:left_child = arena.get(node.left) ? default;
    // Process left child
}
```

---

## Error Handling

### Stale Handle Detection

```aria
arena<int32>:numbers;
Handle<int32>:h = numbers.alloc(42);

// Reallocate
numbers.grow(numbers.capacity * 2);

// Access stale handle
int32 | ERR:result = numbers.get(h);

if result == ERR {
    dbug.print('arena', "Stale handle detected: index={{uint64}}, gen={{uint32}}", 
        [h.index, h.generation]);
    
    // Recovery strategy
    Handle<int32>:refreshed = numbers.find_handle_by_value(42);
    // ... continue with refreshed handle
}
```

### Freed Handle Detection

```aria
Handle<int32>:h = numbers.alloc(42);

// Free slot
numbers.free(h);

// Access freed handle
int32 | ERR:result = numbers.get(h);

if result == ERR {
    dbug.print('arena', "Accessed freed handle", []);
    // Handle appropriately
}
```

### Corruption Detection

```aria
// Arena integrity check
func:verify_arena_integrity<T> = Result<NIL>(arena<T>@:arena) {
    till(arena.capacity - 1, 1) {
        if arena.occupied[$] {
            // Verify generation is valid
            dbug.check('arena', "Invalid generation at slot {{uint64}}", [$],
                arena.generations[$] > 0);
        }
    }
    pass(NULL);
}
```

---

## Comparison with Other Approaches

### vs Raw Pointers

```aria
// Raw Pointer (UNSAFE)
Node*:ptr = arena.alloc();
arena.grow();           // ptr now invalid!
ptr.value = 42;         // 💥 USE-AFTER-FREE

// Handle (SAFE)
Handle<Node>:h = arena.alloc();
arena.grow();           // h detects stale!
Node:n = arena.get(h) ? default;  // ✅ Returns ERR
```

### vs Reference Counting (Rc<T>)

```aria
// Reference counting (runtime overhead, no cycles)
Rc<Node>:rc = Rc::new(node);
Rc<Node>:rc2 = rc.clone();  // Increment refcount (atomic op!)
// Drop automatically when last reference goes away

// Handle (zero overhead when valid, no refcount)
Handle<Node>:h = arena.alloc(node);
// No cloning, no refcount, just index+generation check
// Manual memory management via arena
```

### vs Garbage Collection

```aria
// GC (unpredictable pauses, no manual control)
Node:n = new Node();  // Allocated on heap
// GC automatically frees when no references
// Pauses can occur at ANY time (bad for real-time!)

// Handle (deterministic, manual control)
Handle<Node>:h = arena.alloc(node);
arena.free(h);  // Explicit free (deterministic timing)
// No GC pauses, full control
```

### vs Rust's Borrow Checker

```aria
// Rust (compile-time lifetime tracking, restrictive)
fn use_node(node: &Node) {  // Borrow checker enforces lifetime
    // Cannot use after return (compile error)
}

// Aria Handle (runtime generation check, flexible)
func:use_node = void(Handle<Node>:h, arena<Node>@:arena) {
    Node | ERR:node = arena.get(h);  // Runtime check (flexible!)
    if node == ERR {
        // Handle stale, recover
    }
}
```

---

## Best Practices

### 1. Always Check Handle Access

```aria
// ✓ GOOD: Check for ERR
int32 | ERR:value = arena.get(handle);
if value == ERR {
    handle_stale_reference();
}

// ✗ BAD: Assume always valid
int32:value = arena.get(handle) ? 0;  // Silent failure if stale!
```

### 2. Refresh Handles After Reallocation

```aria
// ✓ GOOD: Track and refresh
Handle<T>[]:handles_before = collect_all_handles();
arena.grow(new_capacity);
Handle<T>[]:handles_after = arena.get_all_handles();
update_all_references(handles_before, handles_after);

// ✗ BAD: Ignore reallocation
arena.grow(new_capacity);
// Continue using old handles (all stale!)
```

### 3. Use NULL_HANDLE for Optional References

```aria
// ✓ GOOD: Clear null representation
const:NULL_HANDLE = Handle<T>{index: 0, generation: 0};
if handle == NULL_HANDLE {
    // No reference
}

// ✗ BAD: Magic numbers
if handle.index == 999999 {  // What does this mean?
}
```

### 4. Batch Allocations to Minimize Reallocations

```aria
// ✓ GOOD: Pre-allocate capacity
arena.reserve(expected_count);
till(items.length - 1, 1) {
    arena.alloc(items[$]);  // No reallocation needed
}

// ✗ BAD: Incremental growth (many reallocations)
till(items.length - 1, 1) {
    arena.alloc(items[$]);  // May reallocate each time!
}
```

### 5. Document Handle Ownership

```aria
// ✓ GOOD: Clear ownership semantics
/**
 * Owns handle to next node.
 * Must refresh handle after arena reallocation.
 */
struct:ListNode {
    int32:value,
    Handle<ListNode>:next,  // Owned
}

// ✗ BAD: Unclear ownership
struct:ListNode {
    int32:value,
    Handle<ListNode>:next,  // Who owns this? When to refresh?
}
```

---

## Common Pitfalls

### Pitfall 1: Forgetting Reallocation Invalidates Handles

```aria
// ✗ PROBLEM: Use handle after reallocation
Handle<int32>:h = arena.alloc(42);
arena.grow(arena.capacity * 2);  // h now stale!
int32:value = arena.get(h) ? 0;  // Returns 0 (ERR), not 42!

// ✓ SOLUTION: Refresh or check
Handle<int32>:h_before = arena.alloc(42);
arena.grow(arena.capacity * 2);
Handle<int32>:h_after = arena.find_refreshed_handle(h_before);  // Get new handle
int32:value = arena.get(h_after) ? 0;  // Correct!
```

### Pitfall 2: Storing Handles Without Updating

```aria
// ✗ PROBLEM: External storage of handles
Handle<Node>[]:global_handles = [h1, h2, h3];
arena.grow();  // All handles in global_handles now stale!

// ✓ SOLUTION: Update external storage
Handle<Node>[]:global_handles = [h1, h2, h3];
Handle<Node>[]:old_handles = global_handles.clone();
arena.grow();
global_handles = remap_handles(old_handles, arena);  // Refresh!
```

### Pitfall 3: Handle Comparison After Reallocation

```aria
// ✗ PROBLEM: Compare stale handles
Handle<T>:h1 = arena.alloc(value1);
Handle<T>:h2 = arena.alloc(value2);
arena.grow();
if h1 == h2 { }  // Meaningless! Both stale!

// ✓ SOLUTION: Compare values, not handles
T:v1 = arena.get(h1) ? default;
T:v2 = arena.get(h2) ? default;
if v1 == v2 { }  // Correct!
```

### Pitfall 4: Not Checking Free Status

```aria
// ✗ PROBLEM: Use after free
Handle<int32>:h = arena.alloc(42);
arena.free(h);
int32:value = arena.get(h) ? 0;  // Returns 0 (ERR, freed!)

// ✓ SOLUTION: Track or check
Handle<int32>:h = arena.alloc(42);
bool:is_valid = arena.is_valid(h);  // Check before use
if is_valid {
    int32:value = arena.get(h) ? 0;
}
```

---

## Implementation Notes

### C Runtime Structure

```c
// Runtime implementation (runtime/arena.c)
typedef struct {
    void* slots;              // Data array
    uint32_t* generations;    // Generation counters
    bool* occupied;           // Occupancy bitmap
    size_t capacity;          // Total slots
    size_t count;             // Used slots
    size_t elem_size;         // sizeof(T)
} aria_arena;

// Handle structure
typedef struct {
    uint64_t index;
    uint32_t generation;
} aria_handle;

// Access function
void* aria_arena_get(aria_arena* arena, aria_handle handle) {
    if (handle.index >= arena->capacity) return NULL;
    if (!arena->occupied[handle.index]) return NULL;
    if (arena->generations[handle.index] != handle.generation) return NULL;
    return (char*)arena->slots + (handle.index * arena->elem_size);
}

// Grow function (invalidates old handles)
void aria_arena_grow(aria_arena* arena, size_t new_capacity) {
    // Reallocate
    void* new_slots = malloc(new_capacity * arena->elem_size);
    memcpy(new_slots, arena->slots, arena->capacity * arena->elem_size);
    free(arena->slots);
    arena->slots = new_slots;
    
    // Reallocate metadata
    uint32_t* new_generations = malloc(new_capacity * sizeof(uint32_t));
    bool* new_occupied = malloc(new_capacity * sizeof(bool));
    
    // Copy and INCREMENT all generations (invalidate old handles!)
    for (size_t i = 0; i < arena->capacity; i++) {
        new_generations[i] = arena->generations[i] + 1;  // ← KEY: Increment!
        new_occupied[i] = arena->occupied[i];
    }
    
    // Initialize new slots
    for (size_t i = arena->capacity; i < new_capacity; i++) {
        new_generations[i] = 1;
        new_occupied[i] = false;
    }
    
    free(arena->generations);
    free(arena->occupied);
    
    arena->generations = new_generations;
    arena->occupied = new_occupied;
    arena->capacity = new_capacity;
}
```

### LLVM IR Generation

```llvm
; Handle type
%Handle = type { i64, i32 }  ; {index, generation}

; Arena get function (simplified)
define %Result @arena_get(%Arena* %arena, %Handle %handle) {
entry:
    ; Extract fields
    %index = extractvalue %Handle %handle, 0
    %gen = extractvalue %Handle %handle, 1
    
    ; Load generation from arena
    %gen_ptr = getelementptr %Arena, %Arena* %arena, i32 0, i32 1, i64 %index
    %arena_gen = load i32, i32* %gen_ptr
    
    ; Compare generations
    %match = icmp eq i32 %arena_gen, %gen
    br i1 %match, label %valid, label %stale
    
valid:
    ; Load value from slots
    %slots = getelementptr %Arena, %Arena* %arena, i32 0, i32 0
    %elem_ptr = getelementptr T, T* %slots, i64 %index
    %value = load T, T* %elem_ptr
    ret %Result { %value, null }
    
stale:
    ; Return ERR
    ret %Result { ERR, error_obj }
}
```

---

## Related Systems

- **[arena<T>](arena.md)** - Arena allocator (underlying storage)
- **[unknown](unknown.md)** - Soft error handling (Handle<T> | unknown)
- **[Result<T>](result.md)** - Hard error handling (get returns Result)
- **[ERR](err.md)** - Error sentinel (stale handles return ERR)

---

## Implementation Status

| Feature | Parser | Compiler | Runtime | Status |
|---------|--------|----------|---------|--------|
| `Handle<T>` generic type | ✅ | ✅ | ✅ | P1-3 Complete (Feb 2026) |
| Arena integration | ✅ | ✅ | ✅ | Complete |
| Generation checking | ✅ | ✅ | ✅ | Complete |
| NULL_HANDLE | ✅ | ✅ | ✅ | Complete |
| Stale detection | ✅ | ✅ | ✅ | Complete |

**Design**: ✅ Complete  
**Implementation**: ✅ Complete (P1-3)  
**Testing**: ✅ Validated  
**Documentation**: ✅ This guide

---

## Summary

**Handle<T> = Memory-safe arena references with automatic stale detection**

### Quick Decision Guide

| Need | Use This | Why |
|------|----------|-----|
| Arena-allocated graphs | `Handle<T>` | Prevents use-after-free |
| Dynamic memory growth | `Handle<T>` | Detects stale after reallocation |
| Self-referential structures | `Handle<T>` | Safe circular references |
| Nikola neurogenesis | `Handle<T>` | O(n³) growth without corruption |
| Need raw speed | Raw pointers (⚠️) | Only if NO reallocation ever |
| Need reference counting | Use library | Automatic lifetime (but overhead) |
| Need garbage collection | Not available | Prefer deterministic control |

### Key Principles

1. **Generation checking** - Detects stale handles automatically
2. **Zero overhead when valid** - Only 1 extra compare vs raw pointers
3. **Safe failure** - Returns ERR instead of crashing
4. **Arena integration** - Designed for dynamic allocators
5. **Type safety** - Generic type enforced at compile time
6. **Explicit refresh** - Programmer controls handle updates

**For Nikola**: Handle<T> is **critical infrastructure** for SHVO neurogenesis. Without it, dynamic neural growth would be unsafe and prone to catastrophic memory corruption that destroys consciousness substrate.

**Remember**: "Pointers are fast but dangerous. Handles are safe and almost as fast. For Nikola's brain, safety isn't optional - corruption kills consciousness."
