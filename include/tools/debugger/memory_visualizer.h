/**
 * Memory Map Visualization
 * Phase 7.4.6: Interactive memory layout viewer
 * 
 * Provides facilities for visualizing Aria's hybrid memory model:
 * - GC Heap (Nursery/Old Generation)
 * - Wild Heap (manual allocation)
 * - WildX (executable memory regions)
 * 
 * Features:
 * - Canvas-based visualization with zoomable regions
 * - Object header decoding on hover
 * - GC statistics overlay
 * - Heap fragmentation heatmap
 * 
 * Reference: request_036_debugger.txt (Phase 4)
 */

#ifndef ARIA_DEBUGGER_MEMORY_VISUALIZER_H
#define ARIA_DEBUGGER_MEMORY_VISUALIZER_H

#include <lldb/API/LLDB.h>
#include <string>
#include <vector>
#include <cstdint>

namespace aria {
namespace debugger {

/**
 * MemoryRegionType - Classification of memory regions
 */
enum class MemoryRegionType {
    GC_NURSERY,     // Young generation (green in UI)
    GC_OLD_GEN,     // Old generation (blue in UI)
    WILD,           // Manual allocations (grey in UI)
    WILDX,          // Executable memory (purple/red in UI)
    FREE,           // Unallocated regions (white in UI)
    UNKNOWN         // Unclassified regions (dark grey in UI)
};

/**
 * MemoryRegion - Represents a contiguous memory region
 */
struct MemoryRegion {
    lldb::addr_t start_address;      // Region start address
    lldb::addr_t end_address;        // Region end address (exclusive)
    size_t size;                     // Region size in bytes
    MemoryRegionType type;           // Region classification
    std::string name;                // Human-readable name
    bool is_readable;                // Read permission
    bool is_writable;                // Write permission
    bool is_executable;              // Execute permission
    uint32_t object_count;           // Number of objects in region
    double fragmentation_ratio;      // 0.0-1.0, higher = more fragmented
};

/**
 * ObjHeader - GC object header structure
 * 
 * 64-bit header preceding every GC-allocated object:
 * Bits 0-7:   Mark bit, pinned bit, forwarded bit, is_nursery (flags)
 * Bits 8-31:  Size class (24 bits)
 * Bits 32-63: Type ID (32 bits)
 */
struct ObjHeader {
    uint8_t color;                   // Tri-color: 0=WHITE, 1=GRAY, 2=BLACK
    bool pinned_bit;                 // Object pinned (non-movable)
    bool forwarded_bit;              // Object was moved during GC
    bool is_nursery;                 // Object in nursery generation
    uint32_t size_class;             // Object size class
    uint32_t type_id;                // Runtime type identifier
    size_t object_size;              // Total object size (including header)
    lldb::addr_t object_address;     // Address of object (after header)
};

/**
 * GCStatistics - Garbage collection statistics
 */
struct GCStatistics {
    uint64_t total_allocations;      // Total allocations since start
    uint64_t total_collections;      // Total GC runs
    uint64_t nursery_collections;    // Minor collections
    uint64_t old_gen_collections;    // Major collections
    size_t nursery_size;             // Nursery size in bytes
    size_t nursery_used;             // Nursery bytes used
    size_t old_gen_size;             // Old gen size in bytes
    size_t old_gen_used;             // Old gen bytes used
    double fragmentation_percent;    // Overall fragmentation (0-100)
    uint64_t live_objects;           // Current live object count
    uint64_t total_bytes_allocated;  // Cumulative bytes allocated
    uint64_t total_bytes_freed;      // Cumulative bytes freed
};

/**
 * HeapBlock - Single block in heap visualization
 * 
 * Represents a chunk of memory for display in the
 * canvas-based visualizer
 */
struct HeapBlock {
    lldb::addr_t address;            // Block start address
    size_t size;                     // Block size
    MemoryRegionType type;           // Block classification
    bool is_allocated;               // Block currently in use
    ObjHeader* header;               // Object header (if GC object)
    std::string tooltip;             // Hover information
};

/**
 * MemoryMap - Complete memory layout snapshot
 */
struct MemoryMap {
    std::vector<MemoryRegion> regions;  // All memory regions
    std::vector<HeapBlock> blocks;      // Detailed block map
    GCStatistics gc_stats;              // GC statistics
    lldb::addr_t heap_start;            // Heap base address
    lldb::addr_t heap_end;              // Heap top address
    size_t total_heap_size;             // Total heap size
    uint64_t timestamp;                 // Snapshot timestamp
};

/**
 * MemoryVisualizer - Memory map generation and analysis
 * 
 * Scans the target process memory and builds a comprehensive
 * memory map for visualization in the web UI
 */
class MemoryVisualizer {
public:
    MemoryVisualizer(lldb::SBTarget target, lldb::SBProcess process);
    ~MemoryVisualizer() = default;
    
    /**
     * Scan target process memory and build memory map
     * 
     * This is the main entry point. It:
     * 1. Queries OS for memory regions (/proc/self/maps on Linux)
     * 2. Classifies regions (GC/Wild/WildX)
     * 3. Scans GC heap for object headers
     * 4. Computes fragmentation statistics
     * 
     * @return Complete memory map snapshot
     */
    MemoryMap scanMemory();
    
    /**
     * Decode object header at given address
     * 
     * Reads the 8-byte ObjHeader structure that precedes
     * every GC-allocated object
     * 
     * @param address Address of object (not header)
     * @return Decoded object header
     */
    ObjHeader decodeObjectHeader(lldb::addr_t address);
    
    /**
     * Get GC statistics from runtime
     * 
     * Queries the GC runtime for allocation/collection statistics
     * 
     * @return Current GC statistics
     */
    GCStatistics getGCStatistics();
    
    /**
     * Compute fragmentation ratio for a region
     * 
     * Analyzes block sizes and gaps to compute fragmentation:
     * - 0.0 = perfectly packed
     * - 1.0 = maximally fragmented
     * 
     * @param region Region to analyze
     * @return Fragmentation ratio (0.0-1.0)
     */
    double computeFragmentation(const MemoryRegion& region);
    
    /**
     * Find all GC objects in a region
     * 
     * Scans memory for ObjHeader patterns and extracts
     * all GC objects within the specified region
     * 
     * @param region Region to scan
     * @return Vector of object headers
     */
    std::vector<ObjHeader> findGCObjects(const MemoryRegion& region);
    
    /**
     * Classify memory region by heuristics
     * 
     * Determines if a region is GC/Wild/WildX based on:
     * - Permissions (RWX patterns)
     * - Address ranges (heap vs stack)
     * - Object header signatures
     * 
     * @param start_addr Region start address
     * @param end_addr Region end address
     * @param perms Permission string (e.g., "rwx")
     * @return Classified region type
     */
    MemoryRegionType classifyRegion(lldb::addr_t start_addr, 
                                     lldb::addr_t end_addr,
                                     const std::string& perms);
    
    /**
     * Generate heatmap data for fragmentation visualization
     * 
     * Divides memory into uniform blocks and computes
     * per-block fragmentation for color-coded display
     * 
     * @param regions Regions to analyze
     * @param block_size Heatmap block size (bytes)
     * @return Vector of blocks with fragmentation data
     */
    std::vector<HeapBlock> generateFragmentationHeatmap(
        const std::vector<MemoryRegion>& regions,
        size_t block_size = 4096
    );

private:
    lldb::SBTarget m_target;
    lldb::SBProcess m_process;
    
    /**
     * Query OS memory map
     * 
     * Platform-specific memory region enumeration:
     * - Linux: Parse /proc/self/maps
     * - macOS: Use mach_vm_region
     * - Windows: VirtualQuery
     * 
     * @return Vector of raw memory regions
     */
    std::vector<MemoryRegion> queryOSMemoryMap();
    
    /**
     * Read memory safely
     * 
     * Attempts to read from target memory, handling
     * permission errors gracefully
     * 
     * @param address Address to read from
     * @param buffer Output buffer
     * @param size Bytes to read
     * @return True if successful
     */
    bool readMemorySafe(lldb::addr_t address, void* buffer, size_t size);
    
    /**
     * Decode 64-bit packed header
     * 
     * Unpacks the bit-field structure of ObjHeader
     * 
     * @param raw_header Raw 64-bit value
     * @return Decoded header structure
     */
    ObjHeader unpackHeader(uint64_t raw_header);
    
    /**
     * Detect GC heap boundaries
     * 
     * Searches for GC runtime symbols to determine
     * heap start/end addresses
     * 
     * @param heap_start Output: heap base address
     * @param heap_end Output: heap top address
     * @return True if heap detected
     */
    bool detectGCHeapBounds(lldb::addr_t& heap_start, lldb::addr_t& heap_end);
    
    /**
     * Check if address is valid GC object
     * 
     * Validates that an address points to a valid
     * GC object by checking header signature
     * 
     * @param address Address to check
     * @return True if valid GC object
     */
    bool isValidGCObject(lldb::addr_t address);
};

} // namespace debugger
} // namespace aria

#endif // ARIA_DEBUGGER_MEMORY_VISUALIZER_H
