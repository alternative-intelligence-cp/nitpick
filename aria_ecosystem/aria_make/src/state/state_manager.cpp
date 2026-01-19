// state_manager.cpp - Incremental State Manager Implementation
// Part of aria_make - Aria Build System

#include "state/state_manager.hpp"

#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <mutex>

// Simple JSON support (minimal implementation to avoid dependencies)
// For production, consider nlohmann/json

namespace aria::make {

// =============================================================================
// FNV-1a Hash Constants
// =============================================================================

static constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
static constexpr uint64_t FNV_PRIME = 1099511628211ULL;

// =============================================================================
// Lifecycle
// =============================================================================

StateManager::StateManager(const fs::path& build_dir)
    : state_file_path_(build_dir / STATE_FILE_NAME) {
}

StateManager::~StateManager() = default;

StateManager::StateManager(StateManager&& other) noexcept
    : state_file_path_(std::move(other.state_file_path_))
    , toolchain_(std::move(other.toolchain_))
    , saved_toolchain_(std::move(other.saved_toolchain_))
    , records_(std::move(other.records_))
    , hash_cache_(std::move(other.hash_cache_))
    , timestamp_cache_(std::move(other.timestamp_cache_))
    , stats_(std::move(other.stats_))
    , dirty_targets_(std::move(other.dirty_targets_)) {
}

StateManager& StateManager::operator=(StateManager&& other) noexcept {
    if (this != &other) {
        std::unique_lock lock(mutex_);
        state_file_path_ = std::move(other.state_file_path_);
        toolchain_ = std::move(other.toolchain_);
        saved_toolchain_ = std::move(other.saved_toolchain_);
        records_ = std::move(other.records_);
        hash_cache_ = std::move(other.hash_cache_);
        timestamp_cache_ = std::move(other.timestamp_cache_);
        stats_ = std::move(other.stats_);
        dirty_targets_ = std::move(other.dirty_targets_);
    }
    return *this;
}

// =============================================================================
// Core Operations
// =============================================================================

bool StateManager::load() {
    std::unique_lock lock(mutex_);

    if (!fs::exists(state_file_path_)) {
        // No state file - this is fine, start fresh
        records_.clear();
        return true;
    }

    std::ifstream file(state_file_path_);
    if (!file) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return deserialize(buffer.str());
}

bool StateManager::save() {
    std::shared_lock lock(mutex_);

    // Ensure parent directory exists
    fs::path parent = state_file_path_.parent_path();
    if (!parent.empty() && !fs::exists(parent)) {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec) {
            return false;
        }
    }

    std::ofstream file(state_file_path_);
    if (!file) {
        return false;
    }

    file << serialize();
    return file.good();
}

void StateManager::clear() {
    std::unique_lock lock(mutex_);
    records_.clear();
    hash_cache_.clear();
    timestamp_cache_.clear();
    dirty_targets_.clear();
    stats_ = BuildStats{};
}

// =============================================================================
// Query Operations
// =============================================================================

DirtyReason StateManager::check_dirty(
    const std::string& target_name,
    const fs::path& output_path,
    const std::vector<std::string>& source_files,
    const std::vector<std::string>& flags) const {

    std::shared_lock lock(mutex_);

    // Rule 1: Output must exist
    if (!fs::exists(output_path)) {
        return DirtyReason::MISSING_ARTIFACT;
    }

    // Rule 2: Must have a record
    auto it = records_.find(target_name);
    if (it == records_.end()) {
        return DirtyReason::MISSING_RECORD;
    }

    const ArtifactRecord& record = it->second;

    // Rule 3: Check if already marked dirty (propagation)
    if (dirty_targets_.count(target_name) > 0) {
        return DirtyReason::DEPENDENCY_DIRTY;
    }

    // Rule 4: Toolchain must match
    if (toolchain_ != saved_toolchain_) {
        return DirtyReason::TOOLCHAIN_CHANGED;
    }

    // Rule 5: Flags must match
    uint64_t current_flags_hash = hash_flags(flags);
    if (current_flags_hash != record.command_hash) {
        return DirtyReason::FLAGS_CHANGED;
    }

    // Rule 6: Source files must match (using hybrid check)
    // Compute combined hash the same way update_record does
    std::string combined;
    for (const auto& source : source_files) {
        combined += get_cached_hash(source);
    }
    std::string current_source_hash = fnv1a_hash(combined) != 0
        ? "fnv1a:" + std::to_string(fnv1a_hash(combined))
        : "";
    if (current_source_hash != record.source_hash) {
        return DirtyReason::SOURCE_CHANGED;
    }

    // Rule 7: Direct dependencies must match
    for (const auto& dep : record.direct_dependencies) {
        if (file_changed(dep.path, dep.hash)) {
            return DirtyReason::DEPENDENCY_CHANGED;
        }
    }

    // Rule 8: Implicit dependencies must match
    for (const auto& implicit_dep : record.implicit_dependencies) {
        // For implicit deps, we just check if file changed since build
        if (!fs::exists(implicit_dep)) {
            return DirtyReason::IMPLICIT_DEP_CHANGED;
        }
        uint64_t current_ts = get_file_timestamp(implicit_dep);
        if (current_ts > record.build_timestamp) {
            return DirtyReason::IMPLICIT_DEP_CHANGED;
        }
    }

    return DirtyReason::CLEAN;
}

std::optional<ArtifactRecord> StateManager::get_record(
    const std::string& target_name) const {
    std::shared_lock lock(mutex_);

    auto it = records_.find(target_name);
    if (it != records_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool StateManager::has_state() const {
    std::shared_lock lock(mutex_);
    return !records_.empty();
}

size_t StateManager::target_count() const {
    std::shared_lock lock(mutex_);
    return records_.size();
}

// =============================================================================
// Update Operations
// =============================================================================

void StateManager::update_record(
    const std::string& target_name,
    const fs::path& output_path,
    const std::vector<std::string>& source_files,
    const std::vector<DependencyInfo>& resolved_deps,
    const std::vector<std::string>& implicit_deps,
    const std::vector<std::string>& flags,
    uint64_t build_duration_ms) {

    std::unique_lock lock(mutex_);

    ArtifactRecord record;
    record.target_name = target_name;
    record.output_path = output_path;

    // Compute source hash (combined hash of all sources)
    std::string combined;
    for (const auto& source : source_files) {
        combined += get_cached_hash(source);
    }
    record.source_hash = fnv1a_hash(combined) != 0
        ? "fnv1a:" + std::to_string(fnv1a_hash(combined))
        : "";

    record.command_hash = hash_flags(flags);
    record.direct_dependencies = resolved_deps;
    record.implicit_dependencies = implicit_deps;

    auto now = std::chrono::system_clock::now();
    record.build_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    record.build_duration_ms = build_duration_ms;

    // Update source timestamp
    if (!source_files.empty()) {
        record.source_timestamp = get_file_timestamp(source_files[0]);
    }

    records_[target_name] = std::move(record);
    dirty_targets_.erase(target_name);

    // Update statistics
    stats_.rebuilt_targets++;
    stats_.total_targets = records_.size();
}

void StateManager::invalidate(const std::string& target_name) {
    std::unique_lock lock(mutex_);
    records_.erase(target_name);
    dirty_targets_.insert(target_name);
}

void StateManager::mark_dirty(const std::string& target_name) {
    std::unique_lock lock(mutex_);
    dirty_targets_.insert(target_name);
}

// =============================================================================
// Toolchain Management
// =============================================================================

void StateManager::set_toolchain(const ToolchainInfo& toolchain) {
    std::unique_lock lock(mutex_);
    // If no state was previously loaded, also update saved_toolchain_
    // to avoid false "toolchain changed" detection
    if (saved_toolchain_.compiler_version.empty()) {
        saved_toolchain_ = toolchain;
    }
    toolchain_ = toolchain;
}

ToolchainInfo StateManager::get_toolchain() const {
    std::shared_lock lock(mutex_);
    return toolchain_;
}

bool StateManager::toolchain_changed() const {
    std::shared_lock lock(mutex_);
    return toolchain_ != saved_toolchain_;
}

// =============================================================================
// Hash Utilities
// =============================================================================

std::string StateManager::hash_file(const fs::path& path) const {
    return get_cached_hash(path);
}

std::string StateManager::hash_files(const std::vector<std::string>& paths) const {
    std::string combined;
    for (const auto& path : paths) {
        combined += get_cached_hash(path);
    }
    return "fnv1a:" + std::to_string(fnv1a_hash(combined));
}

uint64_t StateManager::hash_flags(const std::vector<std::string>& flags) {
    return fnv1a_hash(flags);
}

void StateManager::invalidate_hash_cache(const fs::path& path) {
    std::unique_lock cache_lock(cache_mutex_);
    std::string path_str = path.string();
    hash_cache_.erase(path_str);
    timestamp_cache_.erase(path_str);
}

void StateManager::clear_hash_cache() {
    std::unique_lock cache_lock(cache_mutex_);
    hash_cache_.clear();
    timestamp_cache_.clear();
}

// =============================================================================
// Statistics
// =============================================================================

BuildStats StateManager::get_stats() const {
    std::shared_lock lock(mutex_);
    return stats_;
}

void StateManager::reset_stats() {
    std::unique_lock lock(mutex_);
    stats_ = BuildStats{};
}

// =============================================================================
// Internal Helpers
// =============================================================================

std::string StateManager::get_cached_hash(const fs::path& path) const {
    std::string path_str = path.string();

    // Check cache first
    {
        std::shared_lock cache_lock(cache_mutex_);
        auto it = hash_cache_.find(path_str);
        if (it != hash_cache_.end()) {
            // Check if file hasn't changed since we cached
            auto ts_it = timestamp_cache_.find(path_str);
            if (ts_it != timestamp_cache_.end()) {
                uint64_t current_ts = get_file_timestamp(path);
                if (current_ts == ts_it->second) {
                    return it->second;
                }
            }
        }
    }

    // Need to compute hash
    std::string hash = sha256_file(path);
    uint64_t timestamp = get_file_timestamp(path);

    // Update cache
    {
        std::unique_lock cache_lock(cache_mutex_);
        hash_cache_[path_str] = hash;
        timestamp_cache_[path_str] = timestamp;
    }

    return hash;
}

bool StateManager::file_changed(const fs::path& path,
                                const std::string& expected_hash) const {
    if (!fs::exists(path)) {
        return true;  // File doesn't exist = changed
    }

    std::string current_hash = get_cached_hash(path);
    return current_hash != expected_hash;
}

// =============================================================================
// FNV-1a Implementation
// =============================================================================

uint64_t StateManager::fnv1a_hash(const std::string& str) {
    uint64_t hash = FNV_OFFSET_BASIS;
    for (char c : str) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= FNV_PRIME;
    }
    return hash;
}

uint64_t StateManager::fnv1a_hash(const std::vector<std::string>& strings) {
    uint64_t hash = FNV_OFFSET_BASIS;
    for (const auto& str : strings) {
        for (char c : str) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
            hash *= FNV_PRIME;
        }
        // Add separator
        hash ^= 0xFF;
        hash *= FNV_PRIME;
    }
    return hash;
}

// =============================================================================
// SHA-256 Implementation (Simple file hashing)
// For production, use BLAKE3 or OpenSSL
// =============================================================================

std::string StateManager::sha256_file(const fs::path& path) {
    // Simple content-based hash using FNV-1a for now
    // In production, replace with BLAKE3 or OpenSSL SHA-256

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }

    uint64_t hash = FNV_OFFSET_BASIS;
    char buffer[8192];

    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        std::streamsize bytes_read = file.gcount();
        for (std::streamsize i = 0; i < bytes_read; ++i) {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(buffer[i]));
            hash *= FNV_PRIME;
        }
    }

    // Format as hex string with prefix
    std::ostringstream oss;
    oss << "fnv1a:" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return oss.str();
}

uint64_t StateManager::get_file_timestamp(const fs::path& path) {
    std::error_code ec;
    auto ftime = fs::last_write_time(path, ec);
    if (ec) {
        return 0;
    }

    // C++17 compatible: Use file_time_type directly
    // Note: The epoch may differ from system_clock, but we only need
    // consistent comparisons within the same build session
    auto duration = ftime.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    return static_cast<uint64_t>(seconds.count());
}

// =============================================================================
// JSON Serialization (Simple implementation)
// =============================================================================

std::string StateManager::serialize() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"version\": \"" << MANIFEST_VERSION << "\",\n";

    // Toolchain
    oss << "  \"toolchain\": {\n";
    oss << "    \"compiler_version\": \"" << toolchain_.compiler_version << "\",\n";
    oss << "    \"compiler_hash\": \"" << toolchain_.compiler_hash << "\"\n";
    oss << "  },\n";

    // Targets
    oss << "  \"targets\": {\n";

    bool first_target = true;
    for (const auto& [name, record] : records_) {
        if (!first_target) oss << ",\n";
        first_target = false;

        oss << "    \"" << name << "\": {\n";
        oss << "      \"artifact_path\": \"" << record.output_path.string() << "\",\n";
        oss << "      \"source_hash\": \"" << record.source_hash << "\",\n";
        oss << "      \"command_hash\": " << record.command_hash << ",\n";
        oss << "      \"source_timestamp\": " << record.source_timestamp << ",\n";
        oss << "      \"build_timestamp\": " << record.build_timestamp << ",\n";
        oss << "      \"build_duration_ms\": " << record.build_duration_ms << ",\n";

        // Dependencies
        oss << "      \"dependencies\": [";
        bool first_dep = true;
        for (const auto& dep : record.direct_dependencies) {
            if (!first_dep) oss << ", ";
            first_dep = false;
            oss << "{\"path\": \"" << dep.path << "\", \"hash\": \"" << dep.hash << "\"}";
        }
        oss << "],\n";

        // Implicit dependencies
        oss << "      \"implicit_inputs\": [";
        bool first_impl = true;
        for (const auto& impl : record.implicit_dependencies) {
            if (!first_impl) oss << ", ";
            first_impl = false;
            oss << "\"" << impl << "\"";
        }
        oss << "]\n";

        oss << "    }";
    }

    oss << "\n  }\n";
    oss << "}\n";

    return oss.str();
}

bool StateManager::deserialize(const std::string& json_str) {
    // Simple JSON parser for our specific format
    // For production, use nlohmann/json or similar

    records_.clear();

    // Find version
    size_t version_pos = json_str.find("\"version\"");
    if (version_pos == std::string::npos) {
        return false;  // Invalid format
    }

    // Find toolchain
    size_t tc_pos = json_str.find("\"compiler_version\"");
    if (tc_pos != std::string::npos) {
        size_t start = json_str.find(':', tc_pos) + 1;
        start = json_str.find('"', start) + 1;
        size_t end = json_str.find('"', start);
        if (start != std::string::npos && end != std::string::npos) {
            saved_toolchain_.compiler_version = json_str.substr(start, end - start);
        }
    }

    tc_pos = json_str.find("\"compiler_hash\"");
    if (tc_pos != std::string::npos) {
        size_t start = json_str.find(':', tc_pos) + 1;
        start = json_str.find('"', start) + 1;
        size_t end = json_str.find('"', start);
        if (start != std::string::npos && end != std::string::npos) {
            saved_toolchain_.compiler_hash = json_str.substr(start, end - start);
        }
    }

    // Find targets section
    size_t targets_pos = json_str.find("\"targets\"");
    if (targets_pos == std::string::npos) {
        return true;  // No targets, but valid
    }

    // Parse each target (simplified parsing)
    size_t pos = targets_pos;
    while ((pos = json_str.find("\"artifact_path\"", pos)) != std::string::npos) {
        ArtifactRecord record;

        // Find target name (look backwards for key)
        size_t name_end = json_str.rfind("\":", pos);
        if (name_end == std::string::npos) break;
        size_t name_start = json_str.rfind('"', name_end - 1);
        if (name_start == std::string::npos) break;
        record.target_name = json_str.substr(name_start + 1, name_end - name_start - 1);

        // Parse artifact_path
        size_t start = json_str.find(':', pos) + 1;
        start = json_str.find('"', start) + 1;
        size_t end = json_str.find('"', start);
        if (start != std::string::npos && end != std::string::npos) {
            record.output_path = json_str.substr(start, end - start);
        }

        // Parse source_hash
        size_t sh_pos = json_str.find("\"source_hash\"", pos);
        if (sh_pos != std::string::npos && sh_pos < pos + 500) {
            start = json_str.find(':', sh_pos) + 1;
            start = json_str.find('"', start) + 1;
            end = json_str.find('"', start);
            if (start != std::string::npos && end != std::string::npos) {
                record.source_hash = json_str.substr(start, end - start);
            }
        }

        // Parse command_hash
        size_t ch_pos = json_str.find("\"command_hash\"", pos);
        if (ch_pos != std::string::npos && ch_pos < pos + 500) {
            start = json_str.find(':', ch_pos) + 1;
            while (start < json_str.size() && (json_str[start] == ' ' || json_str[start] == '\t')) {
                start++;
            }
            end = start;
            while (end < json_str.size() && std::isdigit(json_str[end])) {
                end++;
            }
            if (start < end) {
                record.command_hash = std::stoull(json_str.substr(start, end - start));
            }
        }

        // Parse timestamps
        size_t st_pos = json_str.find("\"source_timestamp\"", pos);
        if (st_pos != std::string::npos && st_pos < pos + 600) {
            start = json_str.find(':', st_pos) + 1;
            while (start < json_str.size() && !std::isdigit(json_str[start])) start++;
            end = start;
            while (end < json_str.size() && std::isdigit(json_str[end])) end++;
            if (start < end) {
                record.source_timestamp = std::stoull(json_str.substr(start, end - start));
            }
        }

        size_t bt_pos = json_str.find("\"build_timestamp\"", pos);
        if (bt_pos != std::string::npos && bt_pos < pos + 700) {
            start = json_str.find(':', bt_pos) + 1;
            while (start < json_str.size() && !std::isdigit(json_str[start])) start++;
            end = start;
            while (end < json_str.size() && std::isdigit(json_str[end])) end++;
            if (start < end) {
                record.build_timestamp = std::stoull(json_str.substr(start, end - start));
            }
        }

        if (record.is_valid()) {
            records_[record.target_name] = std::move(record);
        }

        pos += 100;  // Move forward to find next target
    }

    return true;
}

} // namespace aria::make
