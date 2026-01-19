#ifndef ARIA_MAKE_ARTIFACT_RECORD_HPP
#define ARIA_MAKE_ARTIFACT_RECORD_HPP

// artifact_record.hpp - Data structures for build state tracking
// Part of aria_make - Aria Build System
//
// Based on StateManager specification from aria_ecosystem research

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

namespace aria::make {

namespace fs = std::filesystem;

// Represents a dependency with its hash at build time
struct DependencyInfo {
    std::string path;     // Relative or absolute path to dependency
    std::string hash;     // Content hash at time of build (BLAKE3)

    DependencyInfo() = default;
    DependencyInfo(const std::string& p, const std::string& h)
        : path(p), hash(h) {}

    bool operator==(const DependencyInfo& other) const {
        return path == other.path && hash == other.hash;
    }
};

// Represents the state of a single build artifact
struct ArtifactRecord {
    std::string target_name;      // e.g., "src/main.aria"
    fs::path output_path;         // e.g., "build/main.o"

    // Integrity Metrics
    std::string source_hash;      // BLAKE3 hash of source content
    uint64_t command_hash;        // FNV-1a hash of compiler flags

    // Provenance Tracking
    std::vector<DependencyInfo> direct_dependencies;   // Explicit deps (use statements)
    std::vector<std::string> implicit_dependencies;    // Comptime deps (embed_file, etc.)

    // Temporal Data (Optimization - for hybrid check)
    uint64_t source_timestamp;    // Last modified time of source
    uint64_t build_timestamp;     // When artifact was built

    // Build Metrics (for telemetry)
    uint64_t build_duration_ms;   // How long the build took

    ArtifactRecord()
        : command_hash(0)
        , source_timestamp(0)
        , build_timestamp(0)
        , build_duration_ms(0) {}

    bool is_valid() const {
        return !target_name.empty() && !source_hash.empty();
    }
};

// Represents the toolchain identity
struct ToolchainInfo {
    std::string compiler_version;  // e.g., "v0.0.7"
    std::string compiler_hash;     // Hash of compiler binary (optional)

    ToolchainInfo() = default;
    ToolchainInfo(const std::string& version, const std::string& hash = "")
        : compiler_version(version), compiler_hash(hash) {}

    bool operator==(const ToolchainInfo& other) const {
        return compiler_version == other.compiler_version
            && compiler_hash == other.compiler_hash;
    }

    bool operator!=(const ToolchainInfo& other) const {
        return !(*this == other);
    }
};

// Reasons why a rebuild is needed
enum class DirtyReason {
    CLEAN,                    // Not dirty - up to date
    MISSING_ARTIFACT,         // Output file doesn't exist
    MISSING_RECORD,           // No previous build record
    SOURCE_CHANGED,           // Source hash mismatch
    DEPENDENCY_CHANGED,       // A dependency's hash changed
    IMPLICIT_DEP_CHANGED,     // An implicit dependency changed
    FLAGS_CHANGED,            // Compilation flags changed
    TOOLCHAIN_CHANGED,        // Compiler version changed
    DEPENDENCY_DIRTY          // A dependency is being rebuilt
};

// Convert DirtyReason to string for telemetry/logging
inline const char* dirty_reason_to_string(DirtyReason reason) {
    switch (reason) {
        case DirtyReason::CLEAN:                return "clean";
        case DirtyReason::MISSING_ARTIFACT:     return "missing_artifact";
        case DirtyReason::MISSING_RECORD:       return "missing_record";
        case DirtyReason::SOURCE_CHANGED:       return "source_changed";
        case DirtyReason::DEPENDENCY_CHANGED:   return "dependency_changed";
        case DirtyReason::IMPLICIT_DEP_CHANGED: return "implicit_dep_changed";
        case DirtyReason::FLAGS_CHANGED:        return "flags_changed";
        case DirtyReason::TOOLCHAIN_CHANGED:    return "toolchain_changed";
        case DirtyReason::DEPENDENCY_DIRTY:     return "dependency_dirty";
        default:                                return "unknown";
    }
}

// Build statistics for telemetry
struct BuildStats {
    size_t total_targets;
    size_t rebuilt_targets;
    size_t cached_targets;
    size_t failed_targets;
    uint64_t total_time_ms;
    uint64_t hash_time_ms;

    BuildStats()
        : total_targets(0)
        , rebuilt_targets(0)
        , cached_targets(0)
        , failed_targets(0)
        , total_time_ms(0)
        , hash_time_ms(0) {}

    double cache_hit_rate() const {
        if (total_targets == 0) return 0.0;
        return static_cast<double>(cached_targets) / total_targets;
    }
};

} // namespace aria::make

#endif // ARIA_MAKE_ARTIFACT_RECORD_HPP
