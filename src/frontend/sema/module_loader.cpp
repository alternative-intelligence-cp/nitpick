#include "frontend/sema/module_loader.h"
#include "frontend/lexer/lexer.h"
#include "frontend/parser/parser.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace aria {
namespace sema {

ModuleLoader::ModuleLoader(const std::string& rootPath)
    : resolver(rootPath), rootPath(rootPath) {
}

LoadedModule* ModuleLoader::loadModule(const std::string& logicalPath, 
                                        const std::string& fromPath) {
    // Use resolver to convert logical path to canonical filesystem path
    std::vector<std::string> pathComponents;
    bool isFilePath = false;
    
    // Parse the logical path
    if (logicalPath.find('/') != std::string::npos || 
        logicalPath.find('\\') != std::string::npos ||
        (logicalPath.length() > 0 && logicalPath[0] == '.')) {
        // File path
        pathComponents.push_back(logicalPath);
        isFilePath = true;
    } else {
        // Logical path (e.g., "std.io")
        std::istringstream iss(logicalPath);
        std::string component;
        while (std::getline(iss, component, '.')) {
            pathComponents.push_back(component);
        }
    }
    
    std::string canonicalPath = resolver.resolveModulePath(pathComponents, isFilePath, fromPath);
    
    if (canonicalPath.empty()) {
        addError("Could not resolve module '" + logicalPath + "'", fromPath);
        return nullptr;
    }
    
    return loadModuleFile(canonicalPath);
}

LoadedModule* ModuleLoader::loadModule(const UseStmt* useStmt, const std::string& fromPath) {
    if (!useStmt) {
        addError("Internal error: null use statement in module loader", fromPath);
        return nullptr;
    }
    
    std::string canonicalPath = resolver.resolveImport(useStmt, fromPath);
    
    if (canonicalPath.empty()) {
        // Error already added by resolver
        for (const auto& err : resolver.getErrors()) {
            errors.push_back(err);
        }
        return nullptr;
    }
    
    return loadModuleFile(canonicalPath);
}

LoadedModule* ModuleLoader::getModule(const std::string& canonicalPath) const {
    auto it = moduleCache.find(canonicalPath);
    return (it != moduleCache.end()) ? it->second.get() : nullptr;
}

bool ModuleLoader::isLoading(const std::string& canonicalPath) const {
    return std::find(loadingStack.begin(), loadingStack.end(), canonicalPath) 
           != loadingStack.end();
}

LoadedModule* ModuleLoader::loadModuleFile(const std::string& canonicalPath) {
    // Step 1: Check cache - return if already loaded
    auto it = moduleCache.find(canonicalPath);
    if (it != moduleCache.end()) {
        LoadedModule* mod = it->second.get();
        if (mod->state == ModuleState::FULLY_LOADED || mod->state == ModuleState::CHECKED) {
            return mod;
        }
        // If it's in another state, we might have a circular dependency
        // or it's still being processed
    }
    
    // Step 2: Check loading stack - error if circular dependency
    if (isLoading(canonicalPath)) {
        std::ostringstream oss;
        oss << "Circular dependency detected:\n";
        for (size_t i = 0; i < loadingStack.size(); ++i) {
            oss << "  " << (i + 1) << ". " << loadingStack[i] << "\n";
        }
        oss << "  " << (loadingStack.size() + 1) << ". " << canonicalPath << " (circular)";
        addError(oss.str(), canonicalPath);
        return nullptr;
    }
    
    // Step 3: Initialize loading
    auto loadedModule = std::make_unique<LoadedModule>(canonicalPath);
    loadedModule->state = ModuleState::LOADING;
    
    // Insert into cache and push onto stack
    LoadedModule* modulePtr = loadedModule.get();
    moduleCache[canonicalPath] = std::move(loadedModule);
    loadingStack.push_back(canonicalPath);
    
    // Step 4: Parse the file
    modulePtr->ast = parseFile(canonicalPath);
    if (!modulePtr->ast) {
        addError("Failed to parse module '" + canonicalPath + "'. Check for syntax errors.", canonicalPath);
        loadingStack.pop_back();
        modulePtr->state = ModuleState::NOT_LOADED;
        return nullptr;
    }
    modulePtr->state = ModuleState::PARSED;
    
    // Step 5: Discover imports
    std::vector<UseStmt*> imports = discoverImports(modulePtr->ast.get());
    
    // Step 6: Recursively load dependencies
    for (UseStmt* import : imports) {
        if (!processImport(import, canonicalPath)) {
            // Error already logged
            loadingStack.pop_back();
            return nullptr;
        }
    }
    modulePtr->state = ModuleState::RESOLVED;
    
    // Step 7: Create module info (symbol table will be populated during type checking)
    std::string moduleName = fs::path(canonicalPath).stem().string();
    modulePtr->moduleInfo = std::make_unique<Module>(moduleName, canonicalPath);
    
    // Step 8: Mark as fully loaded
    modulePtr->state = ModuleState::FULLY_LOADED;
    loadOrder.push_back(canonicalPath);  // Add AFTER deps loaded — ensures topological order for IR gen
    loadingStack.pop_back();
    
    return modulePtr;
}

std::unique_ptr<ProgramNode> ModuleLoader::parseFile(const std::string& filePath) {
    // Read file
    std::ifstream file(filePath);
    if (!file.is_open()) {
        addError("Could not open file: " + filePath);
        return nullptr;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();
    
    // Lex and parse (with correct APIs)
    try {
        frontend::Lexer lexer(source);
        auto tokens = lexer.tokenize();  // Get tokens
        aria::Parser parser(tokens);
        
        auto programAst = parser.parse();  // Returns shared_ptr<ASTNode>
        
        if (parser.hasErrors()) {
            for (const std::string& err : parser.getErrors()) {
                addError("Parse error in " + filePath + ": " + err);
            }
            return nullptr;
        }
        
        // Parser returns shared_ptr<ASTNode>, we need unique_ptr<ProgramNode>.
        // Extract the ProgramNode and move its declarations to avoid copies.
        ProgramNode* rawPtr = dynamic_cast<ProgramNode*>(programAst.get());
        if (!rawPtr) {
            addError("Parse result is not a ProgramNode for " + filePath);
            return nullptr;
        }
        
        return std::unique_ptr<ProgramNode>(new ProgramNode(std::move(rawPtr->declarations)));
    } catch (const std::exception& e) {
        addError("Exception parsing " + filePath + ": " + e.what());
        return nullptr;
    }
}

std::vector<UseStmt*> ModuleLoader::discoverImports(ProgramNode* program) {
    std::vector<UseStmt*> imports;
    
    if (!program) return imports;
    
    // Scan top-level declarations for use statements
    for (auto& stmt : program->declarations) {
        if (auto useStmt = dynamic_cast<UseStmt*>(stmt.get())) {
            imports.push_back(useStmt);
        }
    }
    
    return imports;
}

bool ModuleLoader::processImport(UseStmt* useStmt, const std::string& fromPath) {
    if (!useStmt) return false;
    
    // Load the imported module
    LoadedModule* importedModule = loadModule(useStmt, fromPath);
    
    if (!importedModule) {
        // Error already logged
        return false;
    }
    
    // Track dependency
    auto it = moduleCache.find(fromPath);
    if (it != moduleCache.end()) {
        it->second->dependencies.push_back(importedModule->canonicalPath);
    }
    
    return true;
}

void ModuleLoader::addError(const std::string& message) {
    errors.push_back(message);
}

void ModuleLoader::addError(const std::string& message, const std::string& modulePath) {
    std::ostringstream oss;
    oss << "[" << modulePath << "] " << message;
    errors.push_back(oss.str());
}

std::string ModuleLoader::dumpDependencyGraph() const {
    std::ostringstream oss;
    oss << "Module Dependency Graph:\n";
    oss << "========================\n\n";
    
    for (const auto& [path, module] : moduleCache) {
        oss << "Module: " << path << "\n";
        oss << "  State: ";
        switch (module->state) {
            case ModuleState::NOT_LOADED: oss << "NOT_LOADED"; break;
            case ModuleState::LOADING: oss << "LOADING"; break;
            case ModuleState::PARSED: oss << "PARSED"; break;
            case ModuleState::RESOLVED: oss << "RESOLVED"; break;
            case ModuleState::CHECKED: oss << "CHECKED"; break;
            case ModuleState::FULLY_LOADED: oss << "FULLY_LOADED"; break;
        }
        oss << "\n";
        
        if (!module->dependencies.empty()) {
            oss << "  Dependencies:\n";
            for (const auto& dep : module->dependencies) {
                oss << "    - " << dep << "\n";
            }
        }
        oss << "\n";
    }
    
    return oss.str();
}

} // namespace sema
} // namespace aria
