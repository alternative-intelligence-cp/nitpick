#include "frontend/sema/module_resolver.h"
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace fs = std::filesystem;

namespace aria {
namespace sema {

ModuleResolver::ModuleResolver(const std::string& rootPath)
    : rootPath(normalizePath(rootPath)) {
    // Add root path as first search location
    searchPaths.push_back(this->rootPath);
    
    // Add standard library paths
    // /usr/lib/aria is the default system-wide installation
    #ifdef __linux__
    searchPaths.push_back("/usr/lib/aria");
    searchPaths.push_back("/usr/local/lib/aria");
    #endif
    
    // Add paths from ARIA_PATH environment variable
    auto ariaPathDirs = readAriaPath();
    searchPaths.insert(searchPaths.end(), ariaPathDirs.begin(), ariaPathDirs.end());
}

std::string ModuleResolver::resolveImport(const UseStmt* useStmt, 
                                          const std::string& currentModulePath) {
    if (!useStmt) {
        addError("Null use statement");
        return "";
    }
    
    // Resolve the module path
    std::string resolvedPath = resolveModulePath(useStmt->path, 
                                                  useStmt->isFilePath,
                                                  currentModulePath);
    
    if (resolvedPath.empty()) {
        std::ostringstream oss;
        oss << "Could not resolve module '";
        if (useStmt->isFilePath) {
            oss << useStmt->path[0];
        } else {
            for (size_t i = 0; i < useStmt->path.size(); ++i) {
                if (i > 0) oss << ".";
                oss << useStmt->path[i];
            }
        }
        oss << "'. Searched in: ";
        auto paths = getSearchPaths();
        for (size_t i = 0; i < paths.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << paths[i];
        }
        addError(oss.str());
        return "";
    }
    
    // Check for circular dependencies (research_028 Section 3.3)
    if (isCurrentlyLoading(resolvedPath)) {
        std::ostringstream oss;
        oss << "Circular dependency detected: ";
        for (size_t i = 0; i < loadingStack.size(); ++i) {
            if (i > 0) oss << " -> ";
            oss << loadingStack[i];
        }
        oss << " -> " << resolvedPath;
        addError(oss.str());
        return "";
    }
    
    return resolvedPath;
}

std::string ModuleResolver::resolveModulePath(const std::vector<std::string>& path,
                                               bool isFilePath,
                                               const std::string& currentModulePath) {
    if (path.empty()) {
        addError("Empty module path");
        return "";
    }
    
    // File path imports (use "./file.aria", use "../other/mod.aria")
    // research_028 Section 3.1.5
    if (isFilePath) {
        std::string filePath = path[0]; // File paths are stored as single string in path[0]
        
        // Relative to current module
        std::string currentDir = getDirectory(currentModulePath);
        std::string resolved = normalizePath(filePath, currentDir);
        
        if (isValidAriaFile(resolved)) {
            return resolved;
        }
        
        addError("File path '" + filePath + "' does not exist or is not a .aria file");
        return "";
    }
    
    // Logical path imports (use std.io, use math.calc)
    // research_028 Section 3.2
    
    // Try each search path in order
    for (const auto& searchPath : getSearchPaths()) {
        std::string found = tryFindModule(searchPath, path);
        if (!found.empty()) {
            return found;
        }
    }
    
    // Not found in any search path
    return "";
}

bool ModuleResolver::isCurrentlyLoading(const std::string& modulePath) const {
    return loadingSet.find(normalizePath(modulePath)) != loadingSet.end();
}

void ModuleResolver::beginLoading(const std::string& modulePath) {
    std::string normalized = normalizePath(modulePath);
    loadingStack.push_back(normalized);
    loadingSet.insert(normalized);
}

void ModuleResolver::endLoading(const std::string& modulePath) {
    std::string normalized = normalizePath(modulePath);
    
    if (!loadingStack.empty() && loadingStack.back() == normalized) {
        loadingStack.pop_back();
        loadingSet.erase(normalized);
    }
}

void ModuleResolver::addSearchPath(const std::string& path) {
    std::string normalized = normalizePath(path);
    if (std::find(searchPaths.begin(), searchPaths.end(), normalized) == searchPaths.end()) {
        searchPaths.push_back(normalized);
    }
}

std::vector<std::string> ModuleResolver::getSearchPaths() const {
    return searchPaths;
}

bool ModuleResolver::isValidAriaFile(const std::string& path) {
    if (path.empty()) return false;
    
    try {
        fs::path p(path);
        return fs::exists(p) && fs::is_regular_file(p) && p.extension() == ".aria";
    } catch (...) {
        return false;
    }
}

std::string ModuleResolver::logicalToFilePath(const std::vector<std::string>& components,
                                               const std::string& baseDir) {
    return tryFindModule(baseDir, components);
}

std::string ModuleResolver::normalizePath(const std::string& path, 
                                          const std::string& relativeTo) {
    if (path.empty()) return "";
    
    try {
        fs::path p(path);
        
        // If path is relative and we have a base, resolve it
        if (p.is_relative() && !relativeTo.empty()) {
            fs::path base(relativeTo);
            if (fs::is_regular_file(base)) {
                base = base.parent_path();
            }
            p = base / p;
        }
        
        // Normalize (resolve .., ., etc.)
        return fs::absolute(p).lexically_normal().string();
    } catch (...) {
        return path;
    }
}

std::string ModuleResolver::getDirectory(const std::string& filePath) {
    if (filePath.empty()) return ".";
    
    try {
        fs::path p(filePath);
        if (fs::is_directory(p)) {
            return p.string();
        }
        return p.parent_path().string();
    } catch (...) {
        // Fallback to string manipulation
        size_t pos = filePath.find_last_of("/\\");
        if (pos != std::string::npos) {
            return filePath.substr(0, pos);
        }
        return ".";
    }
}

bool ModuleResolver::isAbsolutePath(const std::string& path) {
    if (path.empty()) return false;
    
    // Unix absolute path
    if (path[0] == '/') return true;
    
    // Windows absolute path (C:\ or similar)
    if (path.length() >= 2 && path[1] == ':') return true;
    
    return false;
}

bool ModuleResolver::isRelativePath(const std::string& path) {
    if (path.empty()) return false;
    
    // Starts with ./ or ../
    if (path[0] == '.') {
        if (path.length() == 1) return true;  // Just "."
        if (path[1] == '/' || path[1] == '\\') return true;  // "./"
        if (path[1] == '.') {
            if (path.length() == 2) return true;  // Just ".."
            if (path.length() > 2 && (path[2] == '/' || path[2] == '\\')) return true;  // "../"
        }
    }
    
    return false;
}

std::string ModuleResolver::tryFindModule(const std::string& baseDir,
                                          const std::vector<std::string>& components) {
    if (components.empty()) return "";
    
    // Try pattern 1: <base>/path/to/module.aria
    // Example: std.io -> /usr/lib/aria/std/io.aria
    std::string filePath = buildPath(baseDir, components, ".aria");
    if (isValidAriaFile(filePath)) {
        return filePath;
    }
    
    // Try pattern 2: <base>/path/to/module/mod.aria
    // Example: std.io -> /usr/lib/aria/std/io/mod.aria
    // research_028 Section 3.2 step 3
    std::vector<std::string> modComponents = components;
    modComponents.push_back("mod");
    std::string modPath = buildPath(baseDir, modComponents, ".aria");
    if (isValidAriaFile(modPath)) {
        return modPath;
    }
    
    return "";
}

std::string ModuleResolver::buildPath(const std::string& baseDir,
                                      const std::vector<std::string>& components,
                                      const std::string& extension) {
    if (components.empty()) return baseDir;
    
    try {
        fs::path result(baseDir);
        for (const auto& comp : components) {
            result /= comp;
        }
        
        // Add extension if not already present
        if (!extension.empty() && result.extension() != extension) {
            result += extension;
        }
        
        return result.string();
    } catch (...) {
        // Fallback to string concatenation
        std::string result = baseDir;
        for (const auto& comp : components) {
            if (!result.empty() && result.back() != '/' && result.back() != '\\') {
                result += '/';
            }
            result += comp;
        }
        if (!extension.empty()) {
            result += extension;
        }
        return result;
    }
}

void ModuleResolver::addError(const std::string& message) {
    errors.push_back(message);
}

std::vector<std::string> ModuleResolver::readAriaPath() {
    std::vector<std::string> paths;
    
    const char* ariaPathEnv = std::getenv("ARIA_PATH");
    if (!ariaPathEnv) {
        return paths;
    }
    
    std::string ariaPath(ariaPathEnv);
    
    // Split by : on Unix or ; on Windows
    #ifdef _WIN32
    const char delimiter = ';';
    #else
    const char delimiter = ':';
    #endif
    
    std::istringstream iss(ariaPath);
    std::string path;
    while (std::getline(iss, path, delimiter)) {
        if (!path.empty()) {
            paths.push_back(normalizePath(path));
        }
    }
    
    return paths;
}

} // namespace sema
} // namespace aria
