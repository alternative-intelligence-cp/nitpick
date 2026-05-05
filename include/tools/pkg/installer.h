#ifndef ARIA_TOOLS_PKG_INSTALLER_H
#define ARIA_TOOLS_PKG_INSTALLER_H

#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace npk {
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
    std::vector<std::string> keywords;
    std::vector<std::string> categories;
    
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
 * Registry entry from the remote/repo-level registry.json
 */
struct RegistryEntry {
    std::string name;
    std::string latest_version;
    std::string description;
    std::vector<std::string> categories;
    int downloads;
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
     */
    bool installPackage(const std::string& pkg_path);
    
    /**
     * Remove an installed package
     */
    bool removePackage(const std::string& name, const std::string& version = "");
    
    /**
     * List all installed packages
     */
    std::vector<InstalledPackage> listInstalledPackages();
    
    /**
     * Get information about an installed package
     */
    std::optional<InstalledPackage> getPackageInfo(const std::string& name);
    
    /**
     * Initialize ~/.aria/packages/ directory structure
     */
    bool initializePackageDirectory();
    
    /**
     * Get the packages root directory
     */
    std::string getPackagesRoot() const;
    
    /**
     * Search the package repository registry for packages matching a query.
     * Searches name, description, keywords, and categories.
     * @param query Search string (empty = list all)
     * @param registry_path Path to registry.json
     * @return Matching registry entries
     */
    std::vector<RegistryEntry> searchRegistry(const std::string& query,
                                               const std::string& registry_path);
    
    /**
     * Create a .aria-pkg tarball from a package directory.
     * The directory must contain an aria-package.toml.
     * @param pkg_dir Path to the package source directory
     * @param output_dir Directory to write the .aria-pkg file (default: current dir)
     * @return true if packing succeeded
     */
    bool packPackage(const std::string& pkg_dir, const std::string& output_dir = ".");
    
    /**
     * Install a package from a source directory (without packing first).
     * Reads aria-package.toml, validates, and copies to install dir.
     * @param pkg_dir Path to the package source directory
     * @return true if installation succeeded
     */
    bool installFromDirectory(const std::string& pkg_dir);
    
    /**
     * Update the local cache of the remote package registry.
     * Clones or pulls the aria-packages repo to ~/.aria/cache/aria-packages/
     * @param remote_url Override the default GitHub repo URL
     * @return true if update succeeded
     */
    bool updateRemoteRegistry(const std::string& remote_url = "");
    
    /**
     * Install a package by name from the remote registry cache.
     * Requires updateRemoteRegistry() to have been run at least once.
     * @param name Package name (e.g., "aria-http")
     * @param version Specific version (empty = latest)
     * @return true if installation succeeded
     */
    bool installRemotePackage(const std::string& name, const std::string& version = "");
    
    /**
     * List packages available in the remote registry cache.
     * @param query Search filter (empty = list all)
     * @return Matching registry entries
     */
    std::vector<RegistryEntry> listRemotePackages(const std::string& query = "");
    
    /**
     * Get the local cache directory path
     */
    std::string getCacheDir() const;
    
    /**
     * Check if the remote registry cache exists
     */
    bool hasRemoteCache() const;
    
private:
    std::string m_packages_root;
    static constexpr const char* DEFAULT_REMOTE_URL = 
        "https://github.com/alternative-intelligence-cp/aria-packages.git";
    
    bool extractPackage(const std::string& pkg_path, std::string& temp_dir);
    bool parseMetadata(const std::string& toml_path, PackageMetadata& metadata);
    bool validateMetadata(const PackageMetadata& metadata);
    bool checkConflicts(const std::string& name, const std::string& version);
    bool copyToInstallDir(const std::string& temp_dir, const PackageMetadata& metadata);
    bool createBinaryLinks(const PackageMetadata& metadata, const std::string& install_path);
    bool updateRegistry(const PackageMetadata& metadata, const std::string& install_path);
    std::vector<InstalledPackage> loadRegistry();
    bool saveRegistry(const std::vector<InstalledPackage>& packages);
    void cleanupTempDir(const std::string& temp_dir);
};

} // namespace pkg
} // namespace npk

#endif // ARIA_TOOLS_PKG_INSTALLER_H
