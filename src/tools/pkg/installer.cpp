#include "tools/pkg/installer.h"
#include <toml.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

namespace fs = std::filesystem;

namespace aria {
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
    std::string cmd = "tar -xzf \"" + pkg_path + "\" -C \"" + temp_dir + "\"";
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
        metadata.authors = toml::find<std::vector<std::string>>(package, "authors");
        metadata.license = toml::find<std::string>(package, "license");
        
        if (package.contains("description")) {
            metadata.description = toml::find<std::string>(package, "description");
        }
        
        // Parse [dependencies] section (optional)
        if (data.contains("dependencies")) {
            const auto& deps = toml::find(data, "dependencies");
            for (const auto& [key, value] : deps.as_table()) {
                PackageMetadata::Dependency dep;
                dep.name = key;
                dep.version_spec = value.as_string();
                metadata.dependencies.push_back(dep);
            }
        }
        
        // Parse [dev-dependencies] section (optional)
        if (data.contains("dev-dependencies")) {
            const auto& dev_deps = toml::find(data, "dev-dependencies");
            for (const auto& [key, value] : dev_deps.as_table()) {
                PackageMetadata::Dependency dep;
                dep.name = key;
                dep.version_spec = value.as_string();
                metadata.dev_dependencies.push_back(dep);
            }
        }
        
        // Parse [build] section (optional)
        if (data.contains("build")) {
            const auto& build = toml::find(data, "build");
            PackageMetadata::BuildInfo build_info;
            build_info.type = toml::find<std::string>(build, "type");
            build_info.entry = toml::find<std::string>(build, "entry");
            
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
    
    // Check authors
    if (metadata.authors.empty()) {
        std::cerr << "Package must have at least one author" << std::endl;
        return false;
    }
    
    // Check license
    if (metadata.license.empty()) {
        std::cerr << "Package license is required" << std::endl;
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
            return packages; // Return empty vector if file doesn't exist
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json_str = buffer.str();
        
        // Simple JSON parsing (for demo purposes, in production use a JSON library)
        // For now, just return empty vector - we'll implement proper JSON parsing later
        // or integrate a JSON library like nlohmann/json
        
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
    
    // Parse metadata
    std::string metadata_path = temp_dir + "/aria-package.toml";
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
    if (!copyToInstallDir(temp_dir, metadata)) {
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

} // namespace pkg
} // namespace aria
