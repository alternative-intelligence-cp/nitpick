#ifndef ARIA_TOOLS_PKG_INSTALLER_H
#define ARIA_TOOLS_PKG_INSTALLER_H

#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace aria {
namespace pkg {

/**
 * Package metadata from aria-package.toml
 */
struct PackageMetadata {
    std::string name;
    std::string version;
    std::vector<std::string> authors;
    std::string license;
    std::string description;
    
    struct Dependency {
        std::string name;
        std::string version_spec;
    };
    
    std::vector<Dependency> dependencies;
    std::vector<Dependency> dev_dependencies;
    
    struct BuildInfo {
        std::string type;  // "library" or "executable"
        std::string entry;
        std::vector<std::string> outputs;
    };
    
    std::optional<BuildInfo> build;
};

/**
 * Installed package information
 */
struct InstalledPackage {
    std::string name;
    std::string version;
    std::string install_path;
    std::vector<std::string> binaries;
    bool is_dependency;
};

/**
 * PackageInstaller handles extraction, validation, and installation
 * of .aria-pkg packages to ~/.aria/packages/
 */
class PackageInstaller {
public:
    PackageInstaller();
    ~PackageInstaller();
    
    /**
     * Install a package from a local .aria-pkg file
     * @param pkg_path Path to the .aria-pkg file
     * @return true if installation succeeded
     */
    bool installPackage(const std::string& pkg_path);
    
    /**
     * Remove an installed package
     * @param name Package name
     * @param version Specific version (or empty for all versions)
     * @return true if removal succeeded
     */
    bool removePackage(const std::string& name, const std::string& version = "");
    
    /**
     * List all installed packages
     * @return Vector of installed package info
     */
    std::vector<InstalledPackage> listInstalledPackages();
    
    /**
     * Get information about an installed package
     * @param name Package name
     * @return Package info if found
     */
    std::optional<InstalledPackage> getPackageInfo(const std::string& name);
    
    /**
     * Initialize ~/.aria/packages/ directory structure
     * @return true if initialization succeeded
     */
    bool initializePackageDirectory();
    
    /**
     * Get the packages root directory
     * @return Path to ~/.aria/packages/
     */
    std::string getPackagesRoot() const;
    
private:
    std::string m_packages_root;
    
    /**
     * Extract a .aria-pkg tarball to a temporary directory
     * @param pkg_path Path to .aria-pkg file
     * @param temp_dir Output: path to temporary extraction directory
     * @return true if extraction succeeded
     */
    bool extractPackage(const std::string& pkg_path, std::string& temp_dir);
    
    /**
     * Parse aria-package.toml metadata
     * @param toml_path Path to aria-package.toml file
     * @param metadata Output: parsed metadata
     * @return true if parsing succeeded
     */
    bool parseMetadata(const std::string& toml_path, PackageMetadata& metadata);
    
    /**
     * Validate package metadata
     * @param metadata Package metadata to validate
     * @return true if metadata is valid
     */
    bool validateMetadata(const PackageMetadata& metadata);
    
    /**
     * Check if package version conflicts with existing installation
     * @param name Package name
     * @param version Package version
     * @return true if no conflicts
     */
    bool checkConflicts(const std::string& name, const std::string& version);
    
    /**
     * Copy extracted package to installation directory
     * @param temp_dir Temporary extraction directory
     * @param metadata Package metadata
     * @return true if copy succeeded
     */
    bool copyToInstallDir(const std::string& temp_dir, const PackageMetadata& metadata);
    
    /**
     * Create symlinks for package binaries in ~/.aria/packages/bin/
     * @param metadata Package metadata
     * @param install_path Package installation path
     * @return true if symlinks created successfully
     */
    bool createBinaryLinks(const PackageMetadata& metadata, const std::string& install_path);
    
    /**
     * Update registry index with newly installed package
     * @param metadata Package metadata
     * @param install_path Package installation path
     * @return true if registry updated successfully
     */
    bool updateRegistry(const PackageMetadata& metadata, const std::string& install_path);
    
    /**
     * Load registry index from JSON
     * @return Vector of installed packages
     */
    std::vector<InstalledPackage> loadRegistry();
    
    /**
     * Save registry index to JSON
     * @param packages Vector of installed packages
     * @return true if save succeeded
     */
    bool saveRegistry(const std::vector<InstalledPackage>& packages);
    
    /**
     * Clean up temporary directory
     * @param temp_dir Path to temporary directory
     */
    void cleanupTempDir(const std::string& temp_dir);
};

} // namespace pkg
} // namespace aria

#endif // ARIA_TOOLS_PKG_INSTALLER_H
