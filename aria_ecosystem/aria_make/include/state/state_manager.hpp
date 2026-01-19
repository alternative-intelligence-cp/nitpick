#ifndef ARIA_MAKE_STATE_MANAGER_HPP
#define ARIA_MAKE_STATE_MANAGER_HPP

// state_manager.hpp - Incremental State Manager for aria_make
// Part of aria_make - Aria Build System
//
// Implements content-addressable build state tracking using:
// - BLAKE3 for content hashing (fast, parallel-capable)
// - FNV-1a for command/flag hashing (fast for short strings)
// - JSON manifest for persistence
// - Hybrid timestamp+hash checking for performance
//
// Thread-safe: Uses shared_mutex for concurrent read access

#include "artifact_record.hpp"

#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <optional>
#include <functional>

namespace aria::make {

namespace fs = std::filesystem;

// Forward declaration for JSON (we'll use nlohmann/json or simple implementation)
class StateManager {
public:
    // State file name (placed in build directory)
    static constexpr const char* STATE_FILE_NAME = ".aria_build_state";
    static constexpr const char* MANIFEST_VERSION = "1.0";

    // Lifecycle
    explicit StateManager(const fs::path& build_dir);
    ~StateManager();

    // Prevent copying (contains mutex)
    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;

    // Allow moving
    StateManager(StateManager&&) noexcept;
    StateManager& operator=(StateManager&&) noexcept;

    // =========================================================================
    // Core Operations
    // =========================================================================

    // Load state from disk. Returns true on success, false if no state exists.
    // Creates empty state if file doesn't exist (not an error).
    bool load();

    // Save state to disk. Returns true on success.
    bool save();

    // Clear all state (for clean builds)
    void clear();

    // =========================================================================
    // Query Operations (Thread-Safe: shared_lock)
    // =========================================================================

    // Check if a target needs rebuilding
    // Returns the reason why it's dirty, or CLEAN if up-to-date
    DirtyReason check_dirty(
        const std::string& target_name,
        const fs::path& output_path,
        const std::vector<std::string>& source_files,
        const std::vector<std::string>& flags) const;

    // Convenience: Returns true if target is dirty
    bool is_dirty(
        const std::string& target_name,
        const fs::path& output_path,
        const std::vector<std::string>& source_files,
        const std::vector<std::string>& flags) const {
        return check_dirty(target_name, output_path, source_files, flags)
            != DirtyReason::CLEAN;
    }

    // Get the record for a target (if exists)
    std::optional<ArtifactRecord> get_record(const std::string& target_name) const;

    // Check if we have any state
    bool has_state() const;

    // Get number of tracked targets
    size_t target_count() const;

    // =========================================================================
    // Update Operations (Thread-Safe: unique_lock)
    // =========================================================================

    // Record a successful build
    void update_record(
        const std::string& target_name,
        const fs::path& output_path,
        const std::vector<std::string>& source_files,
        const std::vector<DependencyInfo>& resolved_deps,
        const std::vector<std::string>& implicit_deps,
        const std::vector<std::string>& flags,
        uint64_t build_duration_ms = 0);

    // Remove a record (forces rebuild next time)
    void invalidate(const std::string& target_name);

    // Mark a target as dirty (propagates to dependents)
    void mark_dirty(const std::string& target_name);

    // =========================================================================
    // Toolchain Management
    // =========================================================================

    // Set the current toolchain info
    void set_toolchain(const ToolchainInfo& toolchain);

    // Get the current toolchain info
    ToolchainInfo get_toolchain() const;

    // Check if toolchain has changed since last build
    bool toolchain_changed() const;

    // =========================================================================
    // Hash Utilities (Public for testing/external use)
    // =========================================================================

    // Compute content hash of a file (BLAKE3 or fallback)
    std::string hash_file(const fs::path& path) const;

    // Compute hash of multiple files
    std::string hash_files(const std::vector<std::string>& paths) const;

    // Compute hash of command-line flags (FNV-1a)
    static uint64_t hash_flags(const std::vector<std::string>& flags);

    // Invalidate hash cache for a specific file (use when file is known to have changed)
    void invalidate_hash_cache(const fs::path& path);

    // Clear all hash caches
    void clear_hash_cache();

    // =========================================================================
    // Statistics
    // =========================================================================

    // Get build statistics (for telemetry)
    BuildStats get_stats() const;

    // Reset statistics
    void reset_stats();

private:
    // State file path
    fs::path state_file_path_;

    // Toolchain identity
    ToolchainInfo toolchain_;
    ToolchainInfo saved_toolchain_;  // From loaded state

    // In-memory state
    std::unordered_map<std::string, ArtifactRecord> records_;

    // Hash cache (avoid re-hashing same file multiple times)
    mutable std::unordered_map<std::string, std::string> hash_cache_;
    mutable std::unordered_map<std::string, uint64_t> timestamp_cache_;

    // Build statistics
    mutable BuildStats stats_;

    // Dirty tracking (propagation)
    mutable std::unordered_set<std::string> dirty_targets_;

    // Synchronization
    mutable std::shared_mutex mutex_;
    mutable std::shared_mutex cache_mutex_;

    // =========================================================================
    // Internal Helpers
    // =========================================================================

    // Get file hash with caching (uses hybrid check)
    std::string get_cached_hash(const fs::path& path) const;

    // Check if file has changed using hybrid timestamp+hash
    bool file_changed(const fs::path& path, const std::string& expected_hash) const;

    // Serialize state to JSON string
    std::string serialize() const;

    // Deserialize state from JSON string
    bool deserialize(const std::string& json_str);

    // FNV-1a hash implementation
    static uint64_t fnv1a_hash(const std::string& str);
    static uint64_t fnv1a_hash(const std::vector<std::string>& strings);

    // Simple SHA-256 hash (fallback if BLAKE3 not available)
    static std::string sha256_file(const fs::path& path);

    // Get file modification timestamp
    static uint64_t get_file_timestamp(const fs::path& path);
};

} // namespace aria::make

#endif // ARIA_MAKE_STATE_MANAGER_HPP
