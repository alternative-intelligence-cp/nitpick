/**
 * Memory Map Visualization Implementation
 * Phase 7.4.6: Interactive memory layout viewer
 */

#include "tools/debugger/memory_visualizer.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <chrono>

#ifdef __linux__
#include <sys/types.h>
#include <unistd.h>
#endif

namespace npk {
namespace debugger {

MemoryVisualizer::MemoryVisualizer(lldb::SBTarget target, lldb::SBProcess process)
    : m_target(target), m_process(process) {
}

MemoryMap MemoryVisualizer::scanMemory() {
    MemoryMap map;
    map.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    
    // Step 1: Query OS memory map
    map.regions = queryOSMemoryMap();
    
    // Step 2: Detect GC heap boundaries
    lldb::addr_t heap_start = 0, heap_end = 0;
    if (detectGCHeapBounds(heap_start, heap_end)) {
        map.heap_start = heap_start;
        map.heap_end = heap_end;
        map.total_heap_size = heap_end - heap_start;
    }
    
    // Step 3: Classify regions and scan for GC objects
    for (auto& region : map.regions) {
        std::string perms;
        if (region.is_readable) perms += "r";
        if (region.is_writable) perms += "w";
        if (region.is_executable) perms += "x";
        region.type = classifyRegion(region.start_address, region.end_address, perms);
        
        // Count objects in GC regions
        if (region.type == MemoryRegionType::GC_NURSERY || 
            region.type == MemoryRegionType::GC_OLD_GEN) {
            auto objects = findGCObjects(region);
            region.object_count = objects.size();
        }
        
        // Calculate fragmentation
        region.fragmentation_ratio = computeFragmentation(region);
    }
    
    // Step 4: Generate detailed block map for visualization
    map.blocks = generateFragmentationHeatmap(map.regions);
    
    // Step 5: Collect GC statistics
    map.gc_stats = getGCStatistics();
    
    return map;
}

std::vector<MemoryRegion> MemoryVisualizer::queryOSMemoryMap() {
    std::vector<MemoryRegion> regions;
    
#ifdef __linux__
    // Linux: Parse /proc/self/maps
    lldb::pid_t pid = m_process.GetProcessID();
    std::string maps_path = "/proc/" + std::to_string(pid) + "/maps";
    std::ifstream maps_file(maps_path);
    
    if (!maps_file.is_open()) {
        return regions;
    }
    
    std::string line;
    while (std::getline(maps_file, line)) {
        // Format: address_range permissions offset dev inode pathname
        // Example: 7f1234567000-7f1234568000 rw-p 00000000 00:00 0 [heap]
        std::istringstream iss(line);
        std::string addr_range, perms, offset, dev, inode;
        std::string pathname;
        
        iss >> addr_range >> perms >> offset >> dev >> inode;
        std::getline(iss, pathname);
        
        // Trim pathname
        size_t start = pathname.find_first_not_of(" \t");
        if (start != std::string::npos) {
            pathname = pathname.substr(start);
        }
        
        // Parse address range
        size_t dash_pos = addr_range.find('-');
        if (dash_pos == std::string::npos) continue;
        
        std::string start_str = addr_range.substr(0, dash_pos);
        std::string end_str = addr_range.substr(dash_pos + 1);
        
        lldb::addr_t start_addr = std::stoull(start_str, nullptr, 16);
        lldb::addr_t end_addr = std::stoull(end_str, nullptr, 16);
        
        MemoryRegion region;
        region.start_address = start_addr;
        region.end_address = end_addr;
        region.size = end_addr - start_addr;
        region.name = pathname.empty() ? "[anonymous]" : pathname;
        region.is_readable = (perms[0] == 'r');
        region.is_writable = (perms[1] == 'w');
        region.is_executable = (perms[2] == 'x');
        region.object_count = 0;
        region.fragmentation_ratio = 0.0;
        region.type = MemoryRegionType::UNKNOWN;
        
        regions.push_back(region);
    }
#else
    // Fallback: Use LLDB's memory region API
    lldb::addr_t addr = 0;
    while (true) {
        lldb::SBMemoryRegionInfo info;
        lldb::SBError error = m_process.GetMemoryRegionInfo(addr, info);
        
        if (!error.Success() || !info.IsValid()) {
            break;
        }
        
        MemoryRegion region;
        region.start_address = info.GetRegionBase();
        region.end_address = info.GetRegionEnd();
        region.size = region.end_address - region.start_address;
        region.name = info.GetName() ? info.GetName() : "[anonymous]";
        region.is_readable = info.IsReadable();
        region.is_writable = info.IsWritable();
        region.is_executable = info.IsExecutable();
        region.object_count = 0;
        region.fragmentation_ratio = 0.0;
        region.type = MemoryRegionType::UNKNOWN;
        
        regions.push_back(region);
        
        // Move to next region
        addr = info.GetRegionEnd();
        if (addr == 0) break;
    }
#endif
    
    return regions;
}

MemoryRegionType MemoryVisualizer::classifyRegion(lldb::addr_t start_addr, 
                                                    lldb::addr_t end_addr,
                                                    const std::string& perms) {
    // Check if in GC heap range
    lldb::addr_t heap_start = 0, heap_end = 0;
    if (detectGCHeapBounds(heap_start, heap_end)) {
        if (start_addr >= heap_start && end_addr <= heap_end) {
            // Check if nursery or old gen by probing for objects
            uint64_t header;
            if (readMemorySafe(start_addr, &header, sizeof(header))) {
                ObjHeader obj = unpackHeader(header);
                if (obj.is_nursery) {
                    return MemoryRegionType::GC_NURSERY;
                } else {
                    return MemoryRegionType::GC_OLD_GEN;
                }
            }
        }
    }
    
    // Check for executable memory (potential WildX)
    if (perms.find('x') != std::string::npos) {
        // If in heap range and executable, it's WildX
        if (heap_start != 0 && start_addr >= heap_start && end_addr <= heap_end) {
            return MemoryRegionType::WILDX;
        }
    }
    
    // Check for Wild heap (writable, in heap range, not GC)
    if (perms.find('w') != std::string::npos && perms.find('x') == std::string::npos) {
        if (heap_start != 0 && start_addr >= heap_start && end_addr <= heap_end) {
            return MemoryRegionType::WILD;
        }
    }
    
    return MemoryRegionType::UNKNOWN;
}

ObjHeader MemoryVisualizer::decodeObjectHeader(lldb::addr_t address) {
    // Object header is 8 bytes BEFORE the object address
    lldb::addr_t header_addr = address - 8;
    uint64_t raw_header = 0;
    
    if (!readMemorySafe(header_addr, &raw_header, sizeof(raw_header))) {
        // Return empty header on failure
        return ObjHeader{false, false, false, false, 0, 0, 0, address};
    }
    
    return unpackHeader(raw_header);
}

ObjHeader MemoryVisualizer::unpackHeader(uint64_t raw_header) {
    ObjHeader header;
    
    // Bit layout (64-bit):
    // Bits 0-1: Color (tri-color: WHITE/GRAY/BLACK)
    // Bits 2-7: Flags (pinned_bit, forwarded_bit, is_nursery, reserved)
    // Bits 8-31: Size class (24 bits)
    // Bits 32-63: Type ID (32 bits)
    
    uint8_t flags = raw_header & 0xFF;
    header.color = flags & 0x03;  // 2-bit color field
    header.pinned_bit = (flags & 0x04) != 0;
    header.forwarded_bit = (flags & 0x08) != 0;
    header.is_nursery = (flags & 0x10) != 0;
    
    header.size_class = (raw_header >> 8) & 0xFFFFFF;
    header.type_id = (raw_header >> 32) & 0xFFFFFFFF;
    
    // Calculate object size from size class
    // Size classes: 0=16, 1=32, 2=64, 3=128, 4=256, etc.
    if (header.size_class < 20) {
        header.object_size = 16 << header.size_class;
    } else {
        header.object_size = 0; // Large object, size stored elsewhere
    }
    
    return header;
}

std::vector<ObjHeader> MemoryVisualizer::findGCObjects(const MemoryRegion& region) {
    std::vector<ObjHeader> objects;
    
    // Scan region for valid object headers
    // Objects are aligned to 16-byte boundaries
    const size_t alignment = 16;
    
    for (lldb::addr_t addr = region.start_address; 
         addr < region.end_address; 
         addr += alignment) {
        
        if (isValidGCObject(addr)) {
            ObjHeader obj = decodeObjectHeader(addr);
            obj.object_address = addr;
            objects.push_back(obj);
            
            // Skip to next object
            if (obj.object_size > 0) {
                addr += obj.object_size;
            }
        }
    }
    
    return objects;
}

bool MemoryVisualizer::isValidGCObject(lldb::addr_t address) {
    // Read header at address - 8
    lldb::addr_t header_addr = address - 8;
    uint64_t raw_header = 0;
    
    if (!readMemorySafe(header_addr, &raw_header, sizeof(raw_header))) {
        return false;
    }
    
    // Basic validation: check if header looks reasonable
    uint8_t flags = raw_header & 0xFF;
    uint32_t size_class = (raw_header >> 8) & 0xFFFFFF;
    // uint32_t type_id = (raw_header >> 32) & 0xFFFFFFFF; // unused for validation
    
    // Heuristics:
    // - Size class should be reasonable (< 20 for normal objects)
    // - Type ID should be non-zero for most objects
    // - Reserved flag bits should be zero
    
    if (size_class > 30) return false; // Unreasonably large
    if ((flags & 0xF0) != 0) return false; // Reserved bits set
    
    return true;
}

GCStatistics MemoryVisualizer::getGCStatistics() {
    GCStatistics stats = {};
    
    // Try to read GC runtime statistics from well-known symbols
    // These would be exported by the Nitpick GC runtime
    
    // Query nursery stats
    lldb::SBValue nursery_size_val = m_target.FindFirstGlobalVariable("__aria_gc_nursery_size");
    if (nursery_size_val.IsValid()) {
        stats.nursery_size = nursery_size_val.GetValueAsUnsigned();
    }
    
    lldb::SBValue nursery_used_val = m_target.FindFirstGlobalVariable("__aria_gc_nursery_used");
    if (nursery_used_val.IsValid()) {
        stats.nursery_used = nursery_used_val.GetValueAsUnsigned();
    }
    
    // Query old gen stats
    lldb::SBValue old_gen_size_val = m_target.FindFirstGlobalVariable("__aria_gc_old_gen_size");
    if (old_gen_size_val.IsValid()) {
        stats.old_gen_size = old_gen_size_val.GetValueAsUnsigned();
    }
    
    lldb::SBValue old_gen_used_val = m_target.FindFirstGlobalVariable("__aria_gc_old_gen_used");
    if (old_gen_used_val.IsValid()) {
        stats.old_gen_used = old_gen_used_val.GetValueAsUnsigned();
    }
    
    // Query allocation/collection counters
    lldb::SBValue total_allocs_val = m_target.FindFirstGlobalVariable("__aria_gc_total_allocations");
    if (total_allocs_val.IsValid()) {
        stats.total_allocations = total_allocs_val.GetValueAsUnsigned();
    }
    
    lldb::SBValue total_collections_val = m_target.FindFirstGlobalVariable("__aria_gc_total_collections");
    if (total_collections_val.IsValid()) {
        stats.total_collections = total_collections_val.GetValueAsUnsigned();
    }
    
    lldb::SBValue nursery_collections_val = m_target.FindFirstGlobalVariable("__aria_gc_nursery_collections");
    if (nursery_collections_val.IsValid()) {
        stats.nursery_collections = nursery_collections_val.GetValueAsUnsigned();
    }
    
    lldb::SBValue old_collections_val = m_target.FindFirstGlobalVariable("__aria_gc_old_gen_collections");
    if (old_collections_val.IsValid()) {
        stats.old_gen_collections = old_collections_val.GetValueAsUnsigned();
    }
    
    // Query byte counters
    lldb::SBValue bytes_allocated_val = m_target.FindFirstGlobalVariable("__aria_gc_total_bytes_allocated");
    if (bytes_allocated_val.IsValid()) {
        stats.total_bytes_allocated = bytes_allocated_val.GetValueAsUnsigned();
    }
    
    lldb::SBValue bytes_freed_val = m_target.FindFirstGlobalVariable("__aria_gc_total_bytes_freed");
    if (bytes_freed_val.IsValid()) {
        stats.total_bytes_freed = bytes_freed_val.GetValueAsUnsigned();
    }
    
    // Calculate live object count by scanning memory regions
    // (This is done during region scan, would be aggregated here)
    
    // Calculate fragmentation percentage
    if (stats.nursery_size > 0 || stats.old_gen_size > 0) {
        size_t total_size = stats.nursery_size + stats.old_gen_size;
        size_t total_used = stats.nursery_used + stats.old_gen_used;
        
        if (total_size > 0) {
            double utilization = static_cast<double>(total_used) / total_size;
            // Fragmentation is inverse of utilization (simplified)
            stats.fragmentation_percent = (1.0 - utilization) * 100.0;
        }
    }
    
    return stats;
}

double MemoryVisualizer::computeFragmentation(const MemoryRegion& region) {
    // Fragmentation analysis:
    // - Internal fragmentation: Padding waste within allocations
    // - External fragmentation: Gaps between allocations
    
    if (region.type != MemoryRegionType::GC_NURSERY && 
        region.type != MemoryRegionType::GC_OLD_GEN &&
        region.type != MemoryRegionType::WILD) {
        return 0.0; // No fragmentation for non-heap regions
    }
    
    // Scan for objects and measure gaps
    auto objects = findGCObjects(region);
    
    if (objects.empty()) {
        return 0.0; // No objects, no fragmentation
    }
    
    // Calculate total object size vs region size
    size_t total_object_size = 0;
    for (const auto& obj : objects) {
        total_object_size += obj.object_size;
    }
    
    // Fragmentation ratio: unused space / total space
    size_t unused = region.size - total_object_size;
    double frag_ratio = static_cast<double>(unused) / region.size;
    
    return frag_ratio;
}

std::vector<HeapBlock> MemoryVisualizer::generateFragmentationHeatmap(
    const std::vector<MemoryRegion>& regions,
    size_t block_size) {
    
    std::vector<HeapBlock> blocks;
    
    for (const auto& region : regions) {
        // Only visualize heap regions
        if (region.type == MemoryRegionType::UNKNOWN) {
            continue;
        }
        
        // Divide region into blocks
        size_t num_blocks = (region.size + block_size - 1) / block_size;
        
        for (size_t i = 0; i < num_blocks; ++i) {
            HeapBlock block;
            block.address = region.start_address + (i * block_size);
            block.size = std::min(block_size, region.size - (i * block_size));
            block.type = region.type;
            block.is_allocated = false; // Would need object scan to determine
            block.header = nullptr;
            
            // Generate tooltip
            std::ostringstream tooltip;
            tooltip << "Address: 0x" << std::hex << block.address << "\n";
            tooltip << "Size: " << std::dec << block.size << " bytes\n";
            tooltip << "Type: ";
            switch (block.type) {
                case MemoryRegionType::GC_NURSERY: tooltip << "GC Nursery"; break;
                case MemoryRegionType::GC_OLD_GEN: tooltip << "GC Old Gen"; break;
                case MemoryRegionType::WILD: tooltip << "Wild Heap"; break;
                case MemoryRegionType::WILDX: tooltip << "WildX (Executable)"; break;
                default: tooltip << "Unknown"; break;
            }
            block.tooltip = tooltip.str();
            
            blocks.push_back(block);
        }
    }
    
    return blocks;
}

bool MemoryVisualizer::detectGCHeapBounds(lldb::addr_t& heap_start, lldb::addr_t& heap_end) {
    // Try to find GC heap boundaries from runtime symbols
    lldb::SBValue heap_start_val = m_target.FindFirstGlobalVariable("__aria_gc_heap_start");
    lldb::SBValue heap_end_val = m_target.FindFirstGlobalVariable("__aria_gc_heap_end");
    
    if (heap_start_val.IsValid() && heap_end_val.IsValid()) {
        heap_start = heap_start_val.GetValueAsUnsigned();
        heap_end = heap_end_val.GetValueAsUnsigned();
        return (heap_start != 0 && heap_end > heap_start);
    }
    
    // Fallback: Search for heap-like regions in memory map
    // (This would require heuristics based on region properties)
    
    return false;
}

bool MemoryVisualizer::readMemorySafe(lldb::addr_t address, void* buffer, size_t size) {
    lldb::SBError error;
    size_t bytes_read = m_process.ReadMemory(address, buffer, size, error);
    
    return error.Success() && bytes_read == size;
}

} // namespace debugger
} // namespace npk
