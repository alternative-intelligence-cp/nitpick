#include "tools/pkg/installer.h"
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <filesystem>

using namespace aria::pkg;

void printUsage(const char* program_name) {
    std::cout << "Aria Package Manager v0.2.4\n\n";
    std::cout << "Usage: " << program_name << " <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  install <path>       Install a package from a .aria-pkg file or directory\n";
    std::cout << "  remove <name>        Remove an installed package (all versions)\n";
    std::cout << "  list                 List all installed packages\n";
    std::cout << "  info <name>          Show information about an installed package\n";
    std::cout << "  search [query]       Search the package registry (or list all)\n";
    std::cout << "  pack [dir]           Pack a directory into a .aria-pkg tarball\n";
    std::cout << "  init                 Initialize ~/.aria/packages/ directory structure\n";
    std::cout << "  help                 Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " install ./math-utils-1.0.0.aria-pkg\n";
    std::cout << "  " << program_name << " install ./my-package/\n";
    std::cout << "  " << program_name << " search math\n";
    std::cout << "  " << program_name << " pack ./my-package/\n";
    std::cout << "  " << program_name << " list\n";
    std::cout << "  " << program_name << " info math-utils\n";
    std::cout << "  " << program_name << " remove math-utils\n\n";
}

int cmdInstall(PackageInstaller& installer, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Error: install command requires a package path\n";
        std::cerr << "Usage: aria-pkg install <path>\n";
        return 1;
    }
    
    std::string pkg_path = args[1];
    
    // Check if it's a directory (install from source) or a .aria-pkg file
    namespace fs = std::filesystem;
    if (fs::is_directory(pkg_path)) {
        if (installer.installFromDirectory(pkg_path)) {
            return 0;
        } else {
            return 1;
        }
    }
    
    if (installer.installPackage(pkg_path)) {
        return 0;
    } else {
        return 1;
    }
}

int cmdRemove(PackageInstaller& installer, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Error: remove command requires a package name\n";
        std::cerr << "Usage: aria-pkg remove <name>\n";
        return 1;
    }
    
    std::string name = args[1];
    
    if (installer.removePackage(name)) {
        return 0;
    } else {
        return 1;
    }
}

int cmdList(PackageInstaller& installer, const std::vector<std::string>& args) {
    auto packages = installer.listInstalledPackages();
    
    if (packages.empty()) {
        std::cout << "No packages installed.\n";
        std::cout << "Install a package with: aria-pkg install <path>\n";
        return 0;
    }
    
    std::cout << "Installed packages:\n\n";
    std::cout << std::left << std::setw(30) << "Package" 
              << std::setw(15) << "Version"
              << "Path" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    for (const auto& pkg : packages) {
        std::cout << std::left << std::setw(30) << pkg.name
                  << std::setw(15) << pkg.version
                  << pkg.install_path << std::endl;
    }
    
    std::cout << "\nTotal: " << packages.size() << " package(s)\n";
    return 0;
}

int cmdInfo(PackageInstaller& installer, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Error: info command requires a package name\n";
        std::cerr << "Usage: aria-pkg info <name>\n";
        return 1;
    }
    
    std::string name = args[1];
    auto pkg_info = installer.getPackageInfo(name);
    
    if (!pkg_info) {
        std::cerr << "Package not found: " << name << std::endl;
        return 1;
    }
    
    std::cout << "Package: " << pkg_info->name << std::endl;
    std::cout << "Version: " << pkg_info->version << std::endl;
    std::cout << "Install Path: " << pkg_info->install_path << std::endl;
    
    if (!pkg_info->binaries.empty()) {
        std::cout << "Binaries:" << std::endl;
        for (const auto& bin : pkg_info->binaries) {
            std::cout << "  - " << bin << std::endl;
        }
    }
    
    std::cout << "Dependency: " << (pkg_info->is_dependency ? "yes" : "no") << std::endl;
    
    return 0;
}

int cmdInit(PackageInstaller& installer, const std::vector<std::string>& args) {
    if (installer.initializePackageDirectory()) {
        std::cout << "\nPackage directory structure created successfully.\n";
        std::cout << "You can now install packages with: aria-pkg install <path>\n";
        return 0;
    } else {
        return 1;
    }
}

int cmdSearch(PackageInstaller& installer, const std::vector<std::string>& args) {
    std::string query = (args.size() >= 2) ? args[1] : "";
    
    // Find registry.json — check common locations
    std::string registry_path;
    const char* aria_registry = getenv("ARIA_REGISTRY");
    if (aria_registry) {
        registry_path = aria_registry;
    } else {
        // Check relative to executable or common install locations
        std::vector<std::string> candidates = {
            "aria_ecosystem/aria_package_repository/registry.json",
            "../aria_ecosystem/aria_package_repository/registry.json",
            std::string(getenv("HOME") ? getenv("HOME") : "") + "/.aria/registry/registry.json",
        };
        for (const auto& path : candidates) {
            if (std::filesystem::exists(path)) {
                registry_path = path;
                break;
            }
        }
    }
    
    if (registry_path.empty()) {
        std::cerr << "Cannot find package registry.\n";
        std::cerr << "Set ARIA_REGISTRY environment variable to the path of registry.json\n";
        return 1;
    }
    
    auto results = installer.searchRegistry(query, registry_path);
    
    if (results.empty()) {
        if (query.empty()) {
            std::cout << "No packages found in registry.\n";
        } else {
            std::cout << "No packages matching '" << query << "'.\n";
        }
        return 0;
    }
    
    std::cout << "Packages" << (query.empty() ? "" : " matching '" + query + "'") << ":\n\n";
    std::cout << std::left << std::setw(25) << "Name"
              << std::setw(10) << "Version"
              << "Description" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    for (const auto& entry : results) {
        std::string desc = entry.description;
        // Truncate long descriptions
        if (desc.length() > 45) {
            desc = desc.substr(0, 42) + "...";
        }
        std::cout << std::left << std::setw(25) << entry.name
                  << std::setw(10) << entry.latest_version
                  << desc << std::endl;
    }
    
    std::cout << "\nFound " << results.size() << " package(s).\n";
    return 0;
}

int cmdPack(PackageInstaller& installer, const std::vector<std::string>& args) {
    std::string pkg_dir = (args.size() >= 2) ? args[1] : ".";
    
    if (installer.packPackage(pkg_dir)) {
        return 0;
    } else {
        return 1;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    
    // Build args vector
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    
    PackageInstaller installer;
    
    // Dispatch commands
    if (command == "install") {
        return cmdInstall(installer, args);
    } else if (command == "remove") {
        return cmdRemove(installer, args);
    } else if (command == "list") {
        return cmdList(installer, args);
    } else if (command == "info") {
        return cmdInfo(installer, args);
    } else if (command == "search") {
        return cmdSearch(installer, args);
    } else if (command == "pack") {
        return cmdPack(installer, args);
    } else if (command == "init") {
        return cmdInit(installer, args);
    } else if (command == "help" || command == "--help" || command == "-h") {
        printUsage(argv[0]);
        return 0;
    } else {
        std::cerr << "Unknown command: " << command << "\n\n";
        printUsage(argv[0]);
        return 1;
    }
    
    return 0;
}
