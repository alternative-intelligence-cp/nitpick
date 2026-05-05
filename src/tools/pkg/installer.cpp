#include "tools/pkg/installer.h"
#include <toml.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

namespace fs = std::filesystem;

namespace {
// Shell-escape a string for safe use in system() calls.
// Wraps in single quotes and escapes embedded single quotes.
std::string shell_escape(const std::string& s) {
    std::string escaped = "'";
    for (char c : s) {
        if (c == '\'') {
            escaped += "'\\''";
        } else {
            escaped += c;
        }
    }
    escaped += "'";
    return escaped;
}
} // anonymous namespace

namespace npk {
namespace pkg {

PackageInstaller::PackageInstaller() {
    // Set packages root to ~/.aria/packages/
    const char* home = getenv("HOME");
    if (!home) {
        home = getpwuid(getuid())->pw_dir;
    }
    m_packages_root = std::string(home) + "/.aria/packages";
}

PackageInstaller::~PackageInstaller() {
}

std::string PackageInstaller::getPackagesRoot() const {
    return m_packages_root;
}

bool PackageInstaller::initializePackageDirectory() {
    try {
        // Create main packages directory
        fs::create_directories(m_packages_root);
        
        // Create subdirectories
        fs::create_directories(m_packages_root + "/registry");
        fs::create_directories(m_packages_root + "/cache");
        fs::create_directories(m_packages_root + "/installed");
        fs::create_directories(m_packages_root + "/bin");
        
        // Create empty registry index if it doesn't exist
        std::string registry_path = m_packages_root + "/registry/index.json";
        if (!fs::exists(registry_path)) {
            std::ofstream registry_file(registry_path);
            registry_file << "[]" << std::endl;
        }
        
        std::cout << "Initialized package directory at: " << m_packages_root << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize package directory: " << e.what() << std::endl;
        return false;
    }
}

bool PackageInstaller::extractPackage(const std::string& pkg_path, std::string& temp_dir) {
    // Create temporary directory
    char temp_template[] = "/tmp/aria-pkg-XXXXXX";
    char* temp_result = mkdtemp(temp_template);
    if (!temp_result) {
        std::cerr << "Failed to create temporary directory" << std::endl;
        return false;
    }
    temp_dir = temp_result;
    
    // Extract tarball using tar command
    std::string cmd = "tar -xzf " + shell_escape(pkg_path) + " -C " + shell_escape(temp_dir);
    int result = system(cmd.c_str());
    if (result != 0) {
        std::cerr << "Failed to extract package: " << pkg_path << std::endl;
        cleanupTempDir(temp_dir);
        return false;
    }
    
    return true;
}

bool PackageInstaller::parseMetadata(const std::string& toml_path, PackageMetadata& metadata) {
    try {
        auto data = toml::parse(toml_path);
        
        // Parse [package] section
        const auto& package = toml::find(data, "package");
        metadata.name = toml::find<std::string>(package, "name");
        metadata.version = toml::find<std::string>(package, "version");
        
        // Authors: accept both "authors" (array) and "author" (string)
        if (package.contains("authors")) {
            metadata.authors = toml::find<std::vector<std::string>>(package, "authors");
        } else if (package.contains("author")) {
            metadata.authors.push_back(toml::find<std::string>(package, "author"));
        }
        
        if (package.contains("license")) {
            metadata.license = toml::find<std::string>(package, "license");
        }
        
        if (package.contains("description")) {
            metadata.description = toml::find<std::string>(package, "description");
        }
        
        if (package.contains("keywords")) {
            metadata.keywords = toml::find<std::vector<std::string>>(package, "keywords");
        }
        
        if (package.contains("categories")) {
            metadata.categories = toml::find<std::vector<std::string>>(package, "categories");
        }
        
        // Parse [dependencies] section (optional)
        if (data.contains("dependencies")) {
            const auto& deps = toml::find(data, "dependencies");
            if (deps.is_table()) {
                for (const auto& [key, value] : deps.as_table()) {
                    PackageMetadata::Dependency dep;
                    dep.name = key;
                    if (value.is_string()) {
                        dep.version_spec = value.as_string();
                    } else if (value.is_table()) {
                        // Handle { path = "...", version = "..." } form
                        if (value.contains("version")) {
                            dep.version_spec = toml::find<std::string>(value, "version");
                        } else if (value.contains("path")) {
                            dep.version_spec = "path:" + toml::find<std::string>(value, "path");
                        }
                    }
                    metadata.dependencies.push_back(dep);
                }
            }
        }
        
        // Parse [dev-dependencies] section (optional)
        if (data.contains("dev-dependencies")) {
            const auto& dev_deps = toml::find(data, "dev-dependencies");
            if (dev_deps.is_table()) {
                for (const auto& [key, value] : dev_deps.as_table()) {
                    PackageMetadata::Dependency dep;
                    dep.name = key;
                    if (value.is_string()) {
                        dep.version_spec = value.as_string();
                    } else if (value.is_table()) {
                        if (value.contains("version")) {
                            dep.version_spec = toml::find<std::string>(value, "version");
                        } else if (value.contains("path")) {
                            dep.version_spec = "path:" + toml::find<std::string>(value, "path");
                        }
                    }
                    metadata.dev_dependencies.push_back(dep);
                }
            }
        }
        
        // Parse [build] section (optional)
        if (data.contains("build")) {
            const auto& build = toml::find(data, "build");
            PackageMetadata::BuildInfo build_info;
            build_info.type = build.contains("type") 
                ? toml::find<std::string>(build, "type") : "library";
            build_info.entry = build.contains("entry")
                ? toml::find<std::string>(build, "entry") : "";
            
            if (build.contains("outputs")) {
                build_info.outputs = toml::find<std::vector<std::string>>(build, "outputs");
            }
            
            metadata.build = build_info;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse metadata: " << e.what() << std::endl;
        return false;
    }
}

bool PackageInstaller::validateMetadata(const PackageMetadata& metadata) {
    // Check package name
    if (metadata.name.empty() || metadata.name.length() < 3 || metadata.name.length() > 64) {
        std::cerr << "Invalid package name: must be 3-64 characters" << std::endl;
        return false;
    }
    
    // Check name format: lowercase alphanumeric + hyphen + underscore
    for (char c : metadata.name) {
        if (!std::islower(c) && !std::isdigit(c) && c != '-' && c != '_') {
            std::cerr << "Invalid package name: must contain only lowercase, digits, hyphen, underscore" << std::endl;
            return false;
        }
    }
    
    // Check first character is a letter
    if (!std::islower(metadata.name[0])) {
        std::cerr << "Invalid package name: must start with a lowercase letter" << std::endl;
        return false;
    }
    
    // Check version format (basic semver validation)
    if (metadata.version.empty()) {
        std::cerr << "Package version is required" << std::endl;
        return false;
    }
    
    return true;
}

bool PackageInstaller::checkConflicts(const std::string& name, const std::string& version) {
    std::string install_path = m_packages_root + "/installed/" + name + "/" + version;
    
    if (fs::exists(install_path)) {
        std::cout << "Package " << name << " version " << version << " is already installed" << std::endl;
        std::cout << "To reinstall, remove it first with: aria pkg remove " << name << std::endl;
        return false;
    }
    
    return true;
}

bool PackageInstaller::copyToInstallDir(const std::string& temp_dir, const PackageMetadata& metadata) {
    try {
        std::string install_path = m_packages_root + "/installed/" + metadata.name + "/" + metadata.version;
        
        // Create installation directory
        fs::create_directories(install_path);
        
        // Copy all files from temp directory to install directory
        fs::copy(temp_dir, install_path, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        
        std::cout << "Installed package to: " << install_path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to copy package to install directory: " << e.what() << std::endl;
        return false;
    }
}

bool PackageInstaller::createBinaryLinks(const PackageMetadata& metadata, const std::string& install_path) {
    // Only create links if this is an executable package with outputs
    if (!metadata.build || metadata.build->type != "executable" || metadata.build->outputs.empty()) {
        return true; // Not an error, just nothing to do
    }
    
    try {
        std::string bin_dir = m_packages_root + "/bin";
        
        for (const auto& output : metadata.build->outputs) {
            std::string binary_path = install_path + "/" + output;
            std::string link_path = bin_dir + "/" + fs::path(output).filename().string();
            
            // Remove existing link if it exists
            if (fs::exists(link_path)) {
                fs::remove(link_path);
            }
            
            // Create symlink
            fs::create_symlink(binary_path, link_path);
            
            std::cout << "Created symlink: " << link_path << " -> " << binary_path << std::endl;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create binary links: " << e.what() << std::endl;
        return false;
    }
}

bool PackageInstaller::updateRegistry(const PackageMetadata& metadata, const std::string& install_path) {
    try {
        auto packages = loadRegistry();
        
        // Create new installed package entry
        InstalledPackage pkg;
        pkg.name = metadata.name;
        pkg.version = metadata.version;
        pkg.install_path = install_path;
        pkg.is_dependency = false; // Direct install, not a dependency
        
        // Add binary names if applicable
        if (metadata.build && metadata.build->type == "executable") {
            for (const auto& output : metadata.build->outputs) {
                pkg.binaries.push_back(fs::path(output).filename().string());
            }
        }
        
        packages.push_back(pkg);
        
        return saveRegistry(packages);
    } catch (const std::exception& e) {
        std::cerr << "Failed to update registry: " << e.what() << std::endl;
        return false;
    }
}

std::vector<InstalledPackage> PackageInstaller::loadRegistry() {
    std::vector<InstalledPackage> packages;
    
    try {
        std::string registry_path = m_packages_root + "/registry/index.json";
        std::ifstream file(registry_path);
        if (!file.is_open()) {
            return packages;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json_str = buffer.str();
        
        // Parse our own JSON format written by saveRegistry:
        // Simple state-machine parser for the known structure
        InstalledPackage current;
        bool in_object = false;
        
        std::istringstream stream(json_str);
        std::string line;
        
        while (std::getline(stream, line)) {
            // Trim whitespace
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;
            std::string trimmed = line.substr(start);
            
            if (trimmed == "{") {
                in_object = true;
                current = InstalledPackage{};
                continue;
            }
            
            if (trimmed == "}" || trimmed == "}," || trimmed == "}]") {
                if (in_object && !current.name.empty()) {
                    packages.push_back(current);
                }
                in_object = false;
                continue;
            }
            
            if (!in_object) continue;
            
            // Parse "key": "value" or "key": bool
            auto colon = trimmed.find(':');
            if (colon == std::string::npos) continue;
            
            std::string key = trimmed.substr(0, colon);
            std::string val = trimmed.substr(colon + 1);
            
            // Extract key name from quotes
            auto kq1 = key.find('"');
            auto kq2 = key.rfind('"');
            if (kq1 == std::string::npos || kq1 == kq2) continue;
            key = key.substr(kq1 + 1, kq2 - kq1 - 1);
            
            // Trim value
            size_t vs = val.find_first_not_of(" \t");
            if (vs != std::string::npos) val = val.substr(vs);
            // Remove trailing comma
            if (!val.empty() && val.back() == ',') val.pop_back();
            
            // Extract string value if quoted
            auto vq1 = val.find('"');
            auto vq2 = val.rfind('"');
            std::string str_val;
            if (vq1 != std::string::npos && vq1 != vq2) {
                str_val = val.substr(vq1 + 1, vq2 - vq1 - 1);
            }
            
            if (key == "name") current.name = str_val;
            else if (key == "version") current.version = str_val;
            else if (key == "install_path") current.install_path = str_val;
            else if (key == "is_dependency") current.is_dependency = (val.find("true") != std::string::npos);
            else if (key == "binaries") {
                // Parse inline array: ["bin1", "bin2"]
                size_t pos = 0;
                while ((pos = val.find('"', pos)) != std::string::npos) {
                    size_t end = val.find('"', pos + 1);
                    if (end != std::string::npos) {
                        current.binaries.push_back(val.substr(pos + 1, end - pos - 1));
                        pos = end + 1;
                    } else break;
                }
            }
        }
        
        return packages;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load registry: " << e.what() << std::endl;
        return packages;
    }
}

bool PackageInstaller::saveRegistry(const std::vector<InstalledPackage>& packages) {
    try {
        std::string registry_path = m_packages_root + "/registry/index.json";
        std::ofstream file(registry_path);
        
        // Simple JSON serialization (for demo purposes)
        file << "[" << std::endl;
        for (size_t i = 0; i < packages.size(); ++i) {
            const auto& pkg = packages[i];
            file << "  {" << std::endl;
            file << "    \"name\": \"" << pkg.name << "\"," << std::endl;
            file << "    \"version\": \"" << pkg.version << "\"," << std::endl;
            file << "    \"install_path\": \"" << pkg.install_path << "\"," << std::endl;
            file << "    \"is_dependency\": " << (pkg.is_dependency ? "true" : "false");
            
            if (!pkg.binaries.empty()) {
                file << "," << std::endl << "    \"binaries\": [";
                for (size_t j = 0; j < pkg.binaries.size(); ++j) {
                    file << "\"" << pkg.binaries[j] << "\"";
                    if (j < pkg.binaries.size() - 1) file << ", ";
                }
                file << "]" << std::endl;
            } else {
                file << std::endl;
            }
            
            file << "  }";
            if (i < packages.size() - 1) file << ",";
            file << std::endl;
        }
        file << "]" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to save registry: " << e.what() << std::endl;
        return false;
    }
}

void PackageInstaller::cleanupTempDir(const std::string& temp_dir) {
    try {
        if (fs::exists(temp_dir)) {
            fs::remove_all(temp_dir);
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to cleanup temporary directory: " << e.what() << std::endl;
    }
}

bool PackageInstaller::installPackage(const std::string& pkg_path) {
    std::cout << "Installing package from: " << pkg_path << std::endl;
    
    // Check if package file exists
    if (!fs::exists(pkg_path)) {
        std::cerr << "Package file not found: " << pkg_path << std::endl;
        return false;
    }
    
    // Initialize package directory if needed
    if (!fs::exists(m_packages_root)) {
        if (!initializePackageDirectory()) {
            return false;
        }
    }
    
    // Extract package to temporary directory
    std::string temp_dir;
    if (!extractPackage(pkg_path, temp_dir)) {
        return false;
    }
    
    // Parse metadata — check both temp_dir/aria-package.toml and temp_dir/<subdir>/aria-package.toml
    std::string metadata_path = temp_dir + "/aria-package.toml";
    std::string pkg_root = temp_dir;
    if (!fs::exists(metadata_path)) {
        // Look for a single subdirectory containing the toml
        for (const auto& entry : fs::directory_iterator(temp_dir)) {
            if (entry.is_directory()) {
                std::string candidate = entry.path().string() + "/aria-package.toml";
                if (fs::exists(candidate)) {
                    metadata_path = candidate;
                    pkg_root = entry.path().string();
                    break;
                }
            }
        }
    }
    if (!fs::exists(metadata_path)) {
        std::cerr << "Package is missing aria-package.toml metadata file" << std::endl;
        cleanupTempDir(temp_dir);
        return false;
    }
    
    PackageMetadata metadata;
    if (!parseMetadata(metadata_path, metadata)) {
        cleanupTempDir(temp_dir);
        return false;
    }
    
    std::cout << "Package: " << metadata.name << " v" << metadata.version << std::endl;
    
    // Validate metadata
    if (!validateMetadata(metadata)) {
        cleanupTempDir(temp_dir);
        return false;
    }
    
    // Check for conflicts
    if (!checkConflicts(metadata.name, metadata.version)) {
        cleanupTempDir(temp_dir);
        return false;
    }
    
    // Copy to installation directory
    if (!copyToInstallDir(pkg_root, metadata)) {
        cleanupTempDir(temp_dir);
        return false;
    }
    
    std::string install_path = m_packages_root + "/installed/" + metadata.name + "/" + metadata.version;
    
    // Create binary links if needed
    if (!createBinaryLinks(metadata, install_path)) {
        std::cerr << "Warning: Failed to create binary links" << std::endl;
    }
    
    // Update registry
    if (!updateRegistry(metadata, install_path)) {
        std::cerr << "Warning: Failed to update registry" << std::endl;
    }
    
    // Cleanup temporary directory
    cleanupTempDir(temp_dir);
    
    std::cout << "Successfully installed " << metadata.name << " v" << metadata.version << std::endl;
    return true;
}

bool PackageInstaller::removePackage(const std::string& name, const std::string& version) {
    try {
        std::string base_path = m_packages_root + "/installed/" + name;
        
        if (version.empty()) {
            // Remove all versions
            if (fs::exists(base_path)) {
                fs::remove_all(base_path);
                std::cout << "Removed all versions of package: " << name << std::endl;
                return true;
            }
        } else {
            // Remove specific version
            std::string version_path = base_path + "/" + version;
            if (fs::exists(version_path)) {
                fs::remove_all(version_path);
                std::cout << "Removed package: " << name << " v" << version << std::endl;
                return true;
            }
        }
        
        std::cerr << "Package not found: " << name;
        if (!version.empty()) {
            std::cerr << " v" << version;
        }
        std::cerr << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Failed to remove package: " << e.what() << std::endl;
        return false;
    }
}

std::vector<InstalledPackage> PackageInstaller::listInstalledPackages() {
    std::vector<InstalledPackage> packages;
    
    try {
        std::string installed_dir = m_packages_root + "/installed";
        
        if (!fs::exists(installed_dir)) {
            return packages;
        }
        
        // Iterate through package directories
        for (const auto& pkg_entry : fs::directory_iterator(installed_dir)) {
            if (pkg_entry.is_directory()) {
                std::string pkg_name = pkg_entry.path().filename().string();
                
                // Iterate through version directories
                for (const auto& ver_entry : fs::directory_iterator(pkg_entry.path())) {
                    if (ver_entry.is_directory()) {
                        std::string version = ver_entry.path().filename().string();
                        
                        InstalledPackage pkg;
                        pkg.name = pkg_name;
                        pkg.version = version;
                        pkg.install_path = ver_entry.path().string();
                        pkg.is_dependency = false;
                        
                        packages.push_back(pkg);
                    }
                }
            }
        }
        
        return packages;
    } catch (const std::exception& e) {
        std::cerr << "Failed to list packages: " << e.what() << std::endl;
        return packages;
    }
}

std::optional<InstalledPackage> PackageInstaller::getPackageInfo(const std::string& name) {
    auto packages = listInstalledPackages();
    
    for (const auto& pkg : packages) {
        if (pkg.name == name) {
            return pkg;
        }
    }
    
    return std::nullopt;
}

// Helper: case-insensitive substring search
static bool containsCI(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    std::string h = haystack, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

std::vector<RegistryEntry> PackageInstaller::searchRegistry(const std::string& query,
                                                             const std::string& registry_path) {
    std::vector<RegistryEntry> results;
    
    std::ifstream file(registry_path);
    if (!file.is_open()) {
        std::cerr << "Cannot open registry: " << registry_path << std::endl;
        return results;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_str = buffer.str();
    
    // Parse the repo-level registry.json format:
    // { "packages": [ { "name": ..., "latest_version": ..., "description": ..., "categories": [...], "downloads": N }, ... ] }
    RegistryEntry current;
    bool in_packages = false;
    bool in_entry = false;
    
    std::istringstream stream(json_str);
    std::string line;
    
    while (std::getline(stream, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        std::string trimmed = line.substr(start);
        
        if (trimmed.find("\"packages\"") != std::string::npos) {
            in_packages = true;
            continue;
        }
        
        if (!in_packages) continue;
        
        if (trimmed == "{") {
            in_entry = true;
            current = RegistryEntry{};
            continue;
        }
        
        if (trimmed == "}" || trimmed == "}," || trimmed == "}]") {
            if (in_entry && !current.name.empty()) {
                // Filter by query
                bool match = query.empty() 
                    || containsCI(current.name, query) 
                    || containsCI(current.description, query);
                if (!match) {
                    for (const auto& cat : current.categories) {
                        if (containsCI(cat, query)) { match = true; break; }
                    }
                }
                if (match) {
                    results.push_back(current);
                }
            }
            in_entry = false;
            continue;
        }
        
        if (!in_entry) continue;
        
        auto colon = trimmed.find(':');
        if (colon == std::string::npos) continue;
        
        std::string key = trimmed.substr(0, colon);
        std::string val = trimmed.substr(colon + 1);
        
        auto kq1 = key.find('"');
        auto kq2 = key.rfind('"');
        if (kq1 == std::string::npos || kq1 == kq2) continue;
        key = key.substr(kq1 + 1, kq2 - kq1 - 1);
        
        size_t vs = val.find_first_not_of(" \t");
        if (vs != std::string::npos) val = val.substr(vs);
        if (!val.empty() && val.back() == ',') val.pop_back();
        
        auto vq1 = val.find('"');
        auto vq2 = val.rfind('"');
        std::string str_val;
        if (vq1 != std::string::npos && vq1 != vq2) {
            str_val = val.substr(vq1 + 1, vq2 - vq1 - 1);
        }
        
        if (key == "name") current.name = str_val;
        else if (key == "latest_version") current.latest_version = str_val;
        else if (key == "description") current.description = str_val;
        else if (key == "downloads") {
            try { current.downloads = std::stoi(val); } catch (...) {}
        }
        else if (key == "categories") {
            size_t pos = 0;
            while ((pos = val.find('"', pos)) != std::string::npos) {
                size_t end = val.find('"', pos + 1);
                if (end != std::string::npos) {
                    current.categories.push_back(val.substr(pos + 1, end - pos - 1));
                    pos = end + 1;
                } else break;
            }
        }
    }
    
    return results;
}

bool PackageInstaller::packPackage(const std::string& pkg_dir, const std::string& output_dir) {
    if (!fs::exists(pkg_dir) || !fs::is_directory(pkg_dir)) {
        std::cerr << "Not a directory: " << pkg_dir << std::endl;
        return false;
    }
    
    std::string toml_path = pkg_dir + "/aria-package.toml";
    if (!fs::exists(toml_path)) {
        std::cerr << "No aria-package.toml found in: " << pkg_dir << std::endl;
        return false;
    }
    
    PackageMetadata metadata;
    if (!parseMetadata(toml_path, metadata)) {
        return false;
    }
    
    if (!validateMetadata(metadata)) {
        return false;
    }
    
    std::string filename = metadata.name + "-" + metadata.version + ".aria-pkg";
    std::string output_path = output_dir + "/" + filename;
    
    // Create tarball
    std::string abs_dir = fs::canonical(pkg_dir).string();
    std::string parent = fs::path(abs_dir).parent_path().string();
    std::string dir_name = fs::path(abs_dir).filename().string();
    
    std::string cmd = "tar -czf " + shell_escape(output_path) + " -C " + shell_escape(parent) + " " + shell_escape(dir_name);
    int result = system(cmd.c_str());
    if (result != 0) {
        std::cerr << "Failed to create package tarball" << std::endl;
        return false;
    }
    
    std::cout << "Created package: " << output_path << std::endl;
    std::cout << "  Name: " << metadata.name << std::endl;
    std::cout << "  Version: " << metadata.version << std::endl;
    std::cout << "  License: " << metadata.license << std::endl;
    
    return true;
}

bool PackageInstaller::installFromDirectory(const std::string& pkg_dir) {
    if (!fs::exists(pkg_dir) || !fs::is_directory(pkg_dir)) {
        std::cerr << "Not a directory: " << pkg_dir << std::endl;
        return false;
    }
    
    std::string toml_path = pkg_dir + "/aria-package.toml";
    if (!fs::exists(toml_path)) {
        std::cerr << "No aria-package.toml found in: " << pkg_dir << std::endl;
        return false;
    }
    
    // Initialize if needed
    if (!fs::exists(m_packages_root)) {
        if (!initializePackageDirectory()) {
            return false;
        }
    }
    
    PackageMetadata metadata;
    if (!parseMetadata(toml_path, metadata)) {
        return false;
    }
    
    std::cout << "Package: " << metadata.name << " v" << metadata.version << std::endl;
    
    if (!validateMetadata(metadata)) {
        return false;
    }
    
    if (!checkConflicts(metadata.name, metadata.version)) {
        return false;
    }
    
    // Copy directly from source directory
    if (!copyToInstallDir(pkg_dir, metadata)) {
        return false;
    }
    
    std::string install_path = m_packages_root + "/installed/" + metadata.name + "/" + metadata.version;
    
    if (!createBinaryLinks(metadata, install_path)) {
        std::cerr << "Warning: Failed to create binary links" << std::endl;
    }
    
    if (!updateRegistry(metadata, install_path)) {
        std::cerr << "Warning: Failed to update registry" << std::endl;
    }
    
    std::cout << "Successfully installed " << metadata.name << " v" << metadata.version << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Remote fetch support (v0.17.3)
// ═══════════════════════════════════════════════════════════════════════

std::string PackageInstaller::getCacheDir() const {
    const char* home = getenv("HOME");
    if (!home) {
        home = getpwuid(getuid())->pw_dir;
    }
    return std::string(home) + "/.aria/cache";
}

bool PackageInstaller::hasRemoteCache() const {
    std::string cache_repo = getCacheDir() + "/aria-packages";
    return fs::exists(cache_repo + "/registry/index.json");
}

bool PackageInstaller::updateRemoteRegistry(const std::string& remote_url) {
    std::string url = remote_url.empty() ? DEFAULT_REMOTE_URL : remote_url;
    std::string cache_dir = getCacheDir();
    std::string cache_repo = cache_dir + "/aria-packages";
    
    // Ensure cache directory exists
    try {
        fs::create_directories(cache_dir);
    } catch (const std::exception& e) {
        std::cerr << "Error creating cache directory: " << e.what() << std::endl;
        return false;
    }
    
    // Check if git is available
    if (system("command -v git >/dev/null 2>&1") != 0) {
        std::cerr << "Error: git is required for remote package fetching\n";
        std::cerr << "Install git and try again.\n";
        return false;
    }
    
    if (fs::exists(cache_repo + "/.git")) {
        // Already cloned — pull latest
        std::cout << "Updating package registry..." << std::endl;
        std::string cmd = "cd " + shell_escape(cache_repo) +
                          " && git pull --ff-only -q 2>&1";
        int ret = system(cmd.c_str());
        if (ret != 0) {
            // Pull failed (diverged?) — re-clone
            std::cerr << "Warning: pull failed, re-cloning..." << std::endl;
            fs::remove_all(cache_repo);
        } else {
            std::cout << "Package registry updated." << std::endl;
            return true;
        }
    }
    
    // Fresh clone
    std::cout << "Downloading package registry from " << url << " ..." << std::endl;
    std::string cmd = "git clone --depth=1 -q " +
                      shell_escape(url) + " " +
                      shell_escape(cache_repo) + " 2>&1";
    int ret = system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Error: failed to clone package registry\n";
        std::cerr << "Check your internet connection and try again.\n";
        return false;
    }
    
    // Verify we got a valid registry
    if (!fs::exists(cache_repo + "/registry/index.json")) {
        std::cerr << "Warning: registry.json not found in cloned repo\n";
    }
    
    // Count available packages
    int pkg_count = 0;
    if (fs::exists(cache_repo + "/packages")) {
        for (const auto& entry : fs::directory_iterator(cache_repo + "/packages")) {
            if (entry.is_directory()) {
                pkg_count++;
            }
        }
    }
    
    std::cout << "Package registry ready (" << pkg_count << " packages available)." << std::endl;
    return true;
}

bool PackageInstaller::installRemotePackage(const std::string& name,
                                             const std::string& version) {
    std::string cache_repo = getCacheDir() + "/aria-packages";
    
    if (!hasRemoteCache()) {
        std::cout << "Package registry not found. Running update first..." << std::endl;
        if (!updateRemoteRegistry()) {
            return false;
        }
    }
    
    // Look for the package in the cached repo
    std::string pkg_dir = cache_repo + "/packages/" + name;
    
    if (!fs::exists(pkg_dir)) {
        std::cerr << "Package not found: " << name << std::endl;
        std::cerr << "Run 'aria-pkg search " << name << "' to find available packages.\n";
        return false;
    }
    
    if (!fs::exists(pkg_dir + "/aria-package.toml")) {
        std::cerr << "Error: package '" << name << "' has no aria-package.toml\n";
        return false;
    }
    
    // Install from the cached directory
    std::cout << "Installing " << name << " from registry..." << std::endl;
    return installFromDirectory(pkg_dir);
}

std::vector<RegistryEntry> PackageInstaller::listRemotePackages(const std::string& query) {
    std::string cache_repo = getCacheDir() + "/aria-packages";
    std::string registry_path = cache_repo + "/registry/index.json";
    std::vector<RegistryEntry> results;
    
    if (!fs::exists(registry_path)) {
        std::cerr << "No cached registry. Run 'aria-pkg update' first.\n";
        return results;
    }
    
    // Parse the actual aria-packages registry format:
    // { "packages": { "name": { "latest": "ver", "description": "...", "versions": {...} }, ... } }
    std::ifstream file(registry_path);
    if (!file.is_open()) {
        std::cerr << "Cannot open registry: " << registry_path << std::endl;
        return results;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    
    // Simple state-machine parser for the nested object format
    bool in_packages = false;
    int brace_depth = 0;
    std::string current_name;
    std::string current_latest;
    std::string current_desc;
    int pkg_depth = -1; // depth when we entered a package entry
    
    std::istringstream stream(json);
    std::string line;
    
    while (std::getline(stream, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        std::string trimmed = line.substr(start);
        
        // Track "packages" section
        if (!in_packages && trimmed.find("\"packages\"") != std::string::npos) {
            in_packages = true;
            brace_depth = 1;  // The '{' on this line opens the packages object
            continue;
        }
        
        if (!in_packages) continue;
        
        // Count braces
        for (char c : trimmed) {
            if (c == '{') brace_depth++;
            else if (c == '}') brace_depth--;
        }
        
        // depth 1 = inside "packages" object, new key = package name
        // depth 2 = inside a package entry
        
        // Detect package name (key at depth 1 followed by '{')
        if (brace_depth >= 2 && pkg_depth < 0) {
            // This line has a key opening a new package object
            auto q1 = trimmed.find('"');
            auto q2 = trimmed.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                std::string key = trimmed.substr(q1 + 1, q2 - q1 - 1);
                // Only treat as package name if line contains '{'
                if (trimmed.find('{') != std::string::npos && 
                    key != "packages" && key != "versions") {
                    current_name = key;
                    current_latest.clear();
                    current_desc.clear();
                    pkg_depth = brace_depth;
                    continue;
                }
            }
        }
        
        // Inside a package entry — extract fields
        if (pkg_depth > 0 && brace_depth >= pkg_depth) {
            auto colon = trimmed.find(':');
            if (colon != std::string::npos) {
                std::string key_part = trimmed.substr(0, colon);
                std::string val_part = trimmed.substr(colon + 1);
                
                auto kq1 = key_part.find('"');
                auto kq2 = key_part.rfind('"');
                if (kq1 != std::string::npos && kq1 != kq2) {
                    std::string key = key_part.substr(kq1 + 1, kq2 - kq1 - 1);
                    
                    auto vq1 = val_part.find('"');
                    auto vq2 = val_part.rfind('"');
                    std::string val;
                    if (vq1 != std::string::npos && vq1 != vq2) {
                        val = val_part.substr(vq1 + 1, vq2 - vq1 - 1);
                    }
                    
                    if (key == "latest") current_latest = val;
                    else if (key == "description") current_desc = val;
                }
            }
        }
        
        // Closing a package entry
        if (pkg_depth > 0 && brace_depth < pkg_depth) {
            if (!current_name.empty()) {
                bool match = query.empty() ||
                    containsCI(current_name, query) ||
                    containsCI(current_desc, query);
                if (match) {
                    RegistryEntry entry;
                    entry.name = current_name;
                    entry.latest_version = current_latest;
                    entry.description = current_desc;
                    entry.downloads = 0;
                    results.push_back(entry);
                }
            }
            current_name.clear();
            pkg_depth = -1;
        }
        
        // Exited "packages" entirely
        if (brace_depth <= 0) {
            break;
        }
    }
    
    return results;
}

} // namespace pkg
} // namespace npk
