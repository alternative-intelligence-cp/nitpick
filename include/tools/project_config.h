#ifndef ARIA_TOOLS_PROJECT_CONFIG_H
#define ARIA_TOOLS_PROJECT_CONFIG_H

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <toml.hpp>

namespace aria {
namespace tools {

/// Represents a dependency in aria.toml
struct Dependency {
    std::string name;
    std::string version;           // Empty for path/git dependencies
    std::optional<std::string> path;
    std::optional<std::string> git;
    std::optional<std::string> branch;
    std::optional<std::string> tag;
    std::optional<std::string> rev;
    std::vector<std::string> features;
    bool optional = false;
};

/// Build target type
enum class TargetType {
    Executable,
    Library,
    Static,
    Shared
};

/// Optimization level
enum class OptimizationLevel {
    Debug,
    Release,
    Size
};

/// Build configuration section [build]
struct BuildConfig {
    TargetType target = TargetType::Executable;
    OptimizationLevel optimization = OptimizationLevel::Debug;
    std::string output = "build/";
    std::vector<std::string> sources = {"src/**/*.aria"};
    std::vector<std::string> exclude;
    std::string main = "src/main.aria";
};

/// Profile configuration [profile.*]
struct Profile {
    int opt_level = 0;           // 0-3
    bool debug_info = true;
    bool lto = false;
    bool strip = false;
    bool overflow_checks = true;
    std::string panic = "unwind"; // or "abort"
};

/// Library configuration [lib]
struct LibConfig {
    std::string name;
    std::vector<std::string> crate_type = {"lib"};
    std::string path = "src/lib.aria";
};

/// Binary configuration [[bin]]
struct BinConfig {
    std::string name;
    std::string path = "src/main.aria";
};

/// Package metadata [package]
struct PackageInfo {
    std::string name;
    std::string version;
    std::vector<std::string> authors;
    std::string edition = "2025";
    std::string license;
    std::string description;
    std::string homepage;
    std::string repository;
    std::string readme = "README.md";
    std::vector<std::string> keywords;
    std::vector<std::string> categories;
    bool publish = true;
};

/// Complete project configuration from aria.toml
struct ProjectConfig {
    // Required sections
    PackageInfo package;
    
    // Optional sections
    BuildConfig build;
    std::map<std::string, Dependency> dependencies;
    std::map<std::string, Dependency> dev_dependencies;
    std::map<std::string, Dependency> build_dependencies;
    std::map<std::string, std::vector<std::string>> features;
    std::map<std::string, Profile> profiles;
    
    std::optional<LibConfig> lib;
    std::vector<BinConfig> bins;
    
    // Workspace
    std::vector<std::string> workspace_members;
    std::vector<std::string> workspace_exclude;
    
    // Path to aria.toml file
    std::string project_path;
    
    // Helper methods
    bool has_feature(const std::string& feature) const;
    std::vector<std::string> get_default_features() const;
    Profile get_profile(const std::string& profile_name) const;
};

/// Parser for aria.toml files
class ProjectConfigParser {
public:
    /// Parse aria.toml from file path
    /// Throws std::runtime_error on parse errors or validation failures
    static ProjectConfig parse_file(const std::string& toml_path);
    
    /// Parse aria.toml from string content
    static ProjectConfig parse_string(const std::string& toml_content, 
                                     const std::string& base_path = ".");
    
    /// Validate project configuration
    /// Returns empty string if valid, error message otherwise
    static std::string validate(const ProjectConfig& config);

private:
    static void parse_package(ProjectConfig& config, const toml::value& data);
    static void parse_dependencies(std::map<std::string, Dependency>& deps, 
                                   const toml::value& data);
    static void parse_build(ProjectConfig& config, const toml::value& data);
    static void parse_features(ProjectConfig& config, const toml::value& data);
    static void parse_profiles(ProjectConfig& config, const toml::value& data);
    static void parse_lib(ProjectConfig& config, const toml::value& data);
    static void parse_bins(ProjectConfig& config, const toml::value& data);
    static void parse_workspace(ProjectConfig& config, const toml::value& data);
    
    static std::string validate_package_name(const std::string& name);
    static std::string validate_version(const std::string& version);
};

} // namespace tools
} // namespace aria

#endif // ARIA_TOOLS_PROJECT_CONFIG_H
