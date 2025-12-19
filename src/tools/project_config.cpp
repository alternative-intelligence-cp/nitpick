#include "tools/project_config.h"
#include <toml.hpp>
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>

namespace aria {
namespace tools {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

TargetType parse_target_type(const std::string& type) {
    if (type == "executable") return TargetType::Executable;
    if (type == "library") return TargetType::Library;
    if (type == "static") return TargetType::Static;
    if (type == "shared") return TargetType::Shared;
    throw std::runtime_error("Invalid target type: " + type);
}

OptimizationLevel parse_optimization(const std::string& opt) {
    if (opt == "debug") return OptimizationLevel::Debug;
    if (opt == "release") return OptimizationLevel::Release;
    if (opt == "size") return OptimizationLevel::Size;
    throw std::runtime_error("Invalid optimization level: " + opt);
}

} // anonymous namespace

// ============================================================================
// ProjectConfig Helper Methods
// ============================================================================

bool ProjectConfig::has_feature(const std::string& feature) const {
    return features.find(feature) != features.end();
}

std::vector<std::string> ProjectConfig::get_default_features() const {
    auto it = features.find("default");
    if (it != features.end()) {
        return it->second;
    }
    return {};
}

Profile ProjectConfig::get_profile(const std::string& profile_name) const {
    auto it = profiles.find(profile_name);
    if (it != profiles.end()) {
        return it->second;
    }
    
    // Return default profile
    Profile default_profile;
    if (profile_name == "release") {
        default_profile.opt_level = 3;
        default_profile.debug_info = false;
        default_profile.overflow_checks = false;
    }
    return default_profile;
}

// ============================================================================
// ProjectConfigParser Implementation
// ============================================================================

ProjectConfig ProjectConfigParser::parse_file(const std::string& toml_path) {
    try {
        // Check if file exists
        if (!std::filesystem::exists(toml_path)) {
            throw std::runtime_error("aria.toml not found: " + toml_path);
        }
        
        // Read and parse TOML
        const auto data = toml::parse(toml_path);
        
        ProjectConfig config;
        config.project_path = std::filesystem::canonical(toml_path).parent_path().string();
        
        // Parse required [package] section
        if (!data.contains("package")) {
            throw std::runtime_error("Missing required [package] section in aria.toml");
        }
        parse_package(config, data.at("package"));
        
        // Parse optional sections
        if (data.contains("dependencies")) {
            parse_dependencies(config.dependencies, data.at("dependencies"));
        }
        if (data.contains("dev-dependencies")) {
            parse_dependencies(config.dev_dependencies, data.at("dev-dependencies"));
        }
        if (data.contains("build-dependencies")) {
            parse_dependencies(config.build_dependencies, data.at("build-dependencies"));
        }
        if (data.contains("build")) {
            parse_build(config, data.at("build"));
        }
        if (data.contains("features")) {
            parse_features(config, data.at("features"));
        }
        if (data.contains("lib")) {
            parse_lib(config, data.at("lib"));
        }
        if (data.contains("bin")) {
            parse_bins(config, data.at("bin"));
        }
        if (data.contains("workspace")) {
            parse_workspace(config, data.at("workspace"));
        }
        
        // Parse profiles
        for (const auto& [key, value] : data.as_table()) {
            if (key.find("profile.") == 0) {
                std::string profile_name = key.substr(8); // Remove "profile." prefix
                parse_profiles(config, value);
            }
        }
        
        // Validate configuration
        std::string error = validate(config);
        if (!error.empty()) {
            throw std::runtime_error("Validation error: " + error);
        }
        
        return config;
        
    } catch (const toml::syntax_error& e) {
        throw std::runtime_error(std::string("TOML parse error: ") + e.what());
    }
}

ProjectConfig ProjectConfigParser::parse_string(const std::string& toml_content,
                                               const std::string& base_path) {
    try {
        std::istringstream iss(toml_content);
        const auto data = toml::parse(iss, "aria.toml");
        
        ProjectConfig config;
        config.project_path = base_path;
        
        // Parse sections (same as parse_file)
        if (!data.contains("package")) {
            throw std::runtime_error("Missing required [package] section");
        }
        parse_package(config, data.at("package"));
        
        if (data.contains("dependencies")) {
            parse_dependencies(config.dependencies, data.at("dependencies"));
        }
        if (data.contains("build")) {
            parse_build(config, data.at("build"));
        }
        if (data.contains("features")) {
            parse_features(config, data.at("features"));
        }
        
        std::string error = validate(config);
        if (!error.empty()) {
            throw std::runtime_error("Validation error: " + error);
        }
        
        return config;
        
    } catch (const toml::syntax_error& e) {
        throw std::runtime_error(std::string("TOML parse error: ") + e.what());
    }
}

void ProjectConfigParser::parse_package(ProjectConfig& config, const toml::value& data) {
    // Required fields
    config.package.name = toml::find<std::string>(data, "name");
    config.package.version = toml::find<std::string>(data, "version");
    
    // Optional fields
    config.package.authors = toml::find_or<std::vector<std::string>>(data, "authors", {});
    config.package.edition = toml::find_or<std::string>(data, "edition", "2025");
    config.package.license = toml::find_or<std::string>(data, "license", "");
    config.package.description = toml::find_or<std::string>(data, "description", "");
    config.package.homepage = toml::find_or<std::string>(data, "homepage", "");
    config.package.repository = toml::find_or<std::string>(data, "repository", "");
    config.package.readme = toml::find_or<std::string>(data, "readme", "README.md");
    config.package.keywords = toml::find_or<std::vector<std::string>>(data, "keywords", {});
    config.package.categories = toml::find_or<std::vector<std::string>>(data, "categories", {});
    config.package.publish = toml::find_or<bool>(data, "publish", true);
}

void ProjectConfigParser::parse_dependencies(std::map<std::string, Dependency>& deps,
                                             const toml::value& data) {
    for (const auto& [name, value] : data.as_table()) {
        Dependency dep;
        dep.name = name;
        
        if (value.is_string()) {
            // Simple version: std = "0.1"
            dep.version = value.as_string();
        } else if (value.is_table()) {
            // Detailed dependency
            dep.version = toml::find_or<std::string>(value, "version", "");
            
            if (value.contains("path")) {
                dep.path = toml::find<std::string>(value, "path");
            }
            if (value.contains("git")) {
                dep.git = toml::find<std::string>(value, "git");
                if (value.contains("branch")) {
                    dep.branch = toml::find<std::string>(value, "branch");
                }
                if (value.contains("tag")) {
                    dep.tag = toml::find<std::string>(value, "tag");
                }
                if (value.contains("rev")) {
                    dep.rev = toml::find<std::string>(value, "rev");
                }
            }
            
            dep.features = toml::find_or<std::vector<std::string>>(value, "features", {});
            dep.optional = toml::find_or<bool>(value, "optional", false);
        }
        
        deps[name] = dep;
    }
}

void ProjectConfigParser::parse_build(ProjectConfig& config, const toml::value& data) {
    if (data.contains("target")) {
        config.build.target = parse_target_type(toml::find<std::string>(data, "target"));
    }
    if (data.contains("optimization")) {
        config.build.optimization = parse_optimization(toml::find<std::string>(data, "optimization"));
    }
    
    config.build.output = toml::find_or<std::string>(data, "output", "build/");
    config.build.sources = toml::find_or<std::vector<std::string>>(data, "sources", {"src/**/*.aria"});
    config.build.exclude = toml::find_or<std::vector<std::string>>(data, "exclude", {});
    config.build.main = toml::find_or<std::string>(data, "main", "src/main.aria");
}

void ProjectConfigParser::parse_features(ProjectConfig& config, const toml::value& data) {
    for (const auto& [name, value] : data.as_table()) {
        std::vector<std::string> feature_list;
        for (const auto& v : value.as_array()) {
            feature_list.push_back(v.as_string());
        }
        config.features[name] = feature_list;
    }
}

void ProjectConfigParser::parse_profiles(ProjectConfig& config, const toml::value& data) {
    // This will be called for each profile.* section
    // Implementation depends on toml11 API for getting section names
    // Simplified for now
}

void ProjectConfigParser::parse_lib(ProjectConfig& config, const toml::value& data) {
    LibConfig lib;
    lib.name = toml::find_or<std::string>(data, "name", config.package.name);
    lib.crate_type = toml::find_or<std::vector<std::string>>(data, "crate-type", {"lib"});
    lib.path = toml::find_or<std::string>(data, "path", "src/lib.aria");
    config.lib = lib;
}

void ProjectConfigParser::parse_bins(ProjectConfig& config, const toml::value& data) {
    if (data.is_table()) {
        // Single [[bin]]
        BinConfig bin;
        bin.name = toml::find_or<std::string>(data, "name", config.package.name);
        bin.path = toml::find_or<std::string>(data, "path", "src/main.aria");
        config.bins.push_back(bin);
    } else if (data.is_array()) {
        // Multiple [[bin]]
        for (const auto& item : data.as_array()) {
            BinConfig bin;
            bin.name = toml::find_or<std::string>(item, "name", config.package.name);
            bin.path = toml::find_or<std::string>(item, "path", "src/main.aria");
            config.bins.push_back(bin);
        }
    }
}

void ProjectConfigParser::parse_workspace(ProjectConfig& config, const toml::value& data) {
    config.workspace_members = toml::find_or<std::vector<std::string>>(data, "members", {});
    config.workspace_exclude = toml::find_or<std::vector<std::string>>(data, "exclude", {});
}

// ============================================================================
// Validation
// ============================================================================

std::string ProjectConfigParser::validate(const ProjectConfig& config) {
    // Validate package name
    std::string error = validate_package_name(config.package.name);
    if (!error.empty()) return error;
    
    // Validate version
    error = validate_version(config.package.version);
    if (!error.empty()) return error;
    
    // Validate description length
    if (config.package.description.length() > 256) {
        return "Description must be 256 characters or less";
    }
    
    // Validate keywords
    if (config.package.keywords.size() > 5) {
        return "Maximum 5 keywords allowed";
    }
    for (const auto& keyword : config.package.keywords) {
        if (keyword.length() > 20) {
            return "Keyword '" + keyword + "' exceeds 20 character limit";
        }
    }
    
    return ""; // Valid
}

std::string ProjectConfigParser::validate_package_name(const std::string& name) {
    if (name.empty()) {
        return "Package name cannot be empty";
    }
    if (name.length() > 64) {
        return "Package name must be 64 characters or less";
    }
    
    // Must start with letter
    if (!std::isalpha(name[0])) {
        return "Package name must start with a letter";
    }
    
    // Only alphanumeric, hyphens, underscores
    std::regex pattern("^[a-zA-Z][a-zA-Z0-9_-]*$");
    if (!std::regex_match(name, pattern)) {
        return "Package name can only contain letters, numbers, hyphens, and underscores";
    }
    
    // Check reserved names
    const std::vector<std::string> reserved = {"std", "core", "aria"};
    if (std::find(reserved.begin(), reserved.end(), name) != reserved.end()) {
        return "Package name '" + name + "' is reserved";
    }
    
    return ""; // Valid
}

std::string ProjectConfigParser::validate_version(const std::string& version) {
    // Simplified semver validation (major.minor.patch)
    std::regex pattern(R"(^(\d+)\.(\d+)\.(\d+)(-[a-zA-Z0-9.-]+)?(\+[a-zA-Z0-9.-]+)?$)");
    if (!std::regex_match(version, pattern)) {
        return "Invalid version format. Expected semantic version (e.g., '1.0.0')";
    }
    return ""; // Valid
}

} // namespace tools
} // namespace aria
