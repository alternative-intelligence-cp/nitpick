# Parser & Configuration Components

This document provides detailed specifications for the parsing and configuration subsystems.

---

## ConfigParser - ABC File Parser

### Purpose
Parse `build.aria` files (Aria Build Configuration format) into structured AST.

### Grammar (EBNF)

```ebnf
BuildFile       ::= Section*
Section         ::= ProjectSection | VariablesSection | TargetsSection

ProjectSection  ::= "project" ":" ObjectLiteral
VariablesSection::= "variables" ":" ObjectLiteral
TargetsSection  ::= "targets" ":" ArrayLiteral

ObjectLiteral   ::= "{" (Member ("," Member)* ","?)? "}"
Member          ::= Key ":" Value
Key             ::= Identifier | StringLiteral

ArrayLiteral    ::= "[" (Value ("," Value)* ","?)? "]"

Value           ::= StringLiteral
                  | InterpolatedString
                  | ObjectLiteral
                  | ArrayLiteral
                  | BooleanLiteral
                  | IntegerLiteral

StringLiteral      ::= '"' [^"]* '"'
Identifier         ::= [a-zA-Z_][a-zA-Z0-9_]*
InterpolatedString ::= StringLiteral (containing "&{...}")
```

### ABC vs JSON Differences

| Feature | JSON | ABC |
|---------|------|-----|
| Keys | Must be quoted (`"name":`) | Can be unquoted (`name:`) |
| Comments | Not allowed | `//` line comments |
| Trailing commas | Error | Allowed `{a: 1,}` |
| Interpolation | No | `&{variable}` syntax |
| Top-level | Any value | Must be object with specific keys |

### Class Structure

```cpp
// AST Nodes
class ASTNode {
public:
    enum class Type {
        BUILD_FILE,
        PROJECT,
        VARIABLES,
        TARGETS,
        OBJECT,
        ARRAY,
        STRING,
        INTEGER,
        BOOLEAN
    };
    
    virtual ~ASTNode() = default;
    virtual Type getType() const = 0;
};

class BuildFileNode : public ASTNode {
public:
    std::unique_ptr<ObjectNode> project;
    std::unique_ptr<ObjectNode> variables;
    std::unique_ptr<ArrayNode> targets;
    
    Type getType() const override { return Type::BUILD_FILE; }
};

class ObjectNode : public ASTNode {
public:
    std::map<std::string, std::unique_ptr<ASTNode>> members;
    Type getType() const override { return Type::OBJECT; }
};

class ArrayNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> elements;
    Type getType() const override { return Type::ARRAY; }
};

class StringNode : public ASTNode {
public:
    std::string value;
    bool needs_interpolation;
    Type getType() const override { return Type::STRING; }
};

// Parser
class ConfigParser {
public:
    explicit ConfigParser(const std::string& source);
    
    std::unique_ptr<BuildFileNode> parse();
    const std::vector<ParseError>& getErrors() const;
    
private:
    aria::frontend::Lexer lexer_;
    aria::frontend::Token current_token_;
    bool panic_mode_;
    std::vector<ParseError> errors_;
    
    // Top-level parsing
    void parseProjectSection(BuildFileNode* root);
    void parseVariablesSection(BuildFileNode* root);
    void parseTargetsSection(BuildFileNode* root);
    
    // Recursive descent
    std::unique_ptr<ObjectNode> parseObject();
    std::unique_ptr<ArrayNode> parseArray();
    std::unique_ptr<ASTNode> parseValue();
    std::string parseKey();
    
    // Utilities
    void advance();
    bool check(aria::TokenType type);
    bool match(aria::TokenType type);
    void expect(aria::TokenType type, const std::string& message);
    void synchronize();  // Panic mode recovery
    void error(const std::string& message);
};

struct ParseError {
    size_t line;
    size_t column;
    std::string message;
};
```

### Parsing Algorithm

**Predictive LL(1) Recursive Descent:**

1. **Start:** Parse top-level sections
2. **parseObject():**
   - Expect `{`
   - Loop:
     - Parse key (IDENTIFIER or STRING)
     - Expect `:`
     - Parse value (recursive call to parseValue())
     - If `,` continue, else break
   - Expect `}`

3. **parseArray():**
   - Expect `[`
   - Loop:
     - Parse value
     - If `,` continue, else break
   - Expect `]`

4. **parseValue():**
   - Check token type:
     - `{` → parseObject()
     - `[` → parseArray()
     - STRING → StringNode
     - NUMBER → IntegerNode
     - `true`/`false` → BooleanNode

### Error Recovery (Panic Mode)

When error encountered:
1. Report error with line/column
2. Enter panic mode (skip tokens)
3. Synchronize at:
   - `,` (next member/element)
   - `}` or `]` (end of container)
   - Next top-level keyword (`project`, `variables`, `targets`)
4. Exit panic mode, resume parsing

### Example Parsing Flow

**Input:**
```javascript
project: {
    name: "MyApp",
    version: "1.0"
}

variables: {
    src: "source"
}
```

**AST:**
```
BuildFileNode
├─ project: ObjectNode
│  ├─ "name" → StringNode("MyApp")
│  └─ "version" → StringNode("1.0")
└─ variables: ObjectNode
   └─ "src" → StringNode("source")
```

### Lexer Adapter

Since we reuse Aria's lexer, we need to map tokens:

```cpp
aria::TokenType toABCToken(aria::TokenType ariaToken) {
    switch (ariaToken) {
        case aria::TokenType::IDENTIFIER:
            return ABCToken::IDENTIFIER;
        case aria::TokenType::STRING_LITERAL:
            return ABCToken::STRING;
        case aria::TokenType::INTEGER_LITERAL:
            return ABCToken::INTEGER;
        case aria::TokenType::KEYWORD_TRUE:
        case aria::TokenType::KEYWORD_FALSE:
            return ABCToken::BOOLEAN;
        case aria::TokenType::COLON:
            return ABCToken::COLON;
        case aria::TokenType::COMMA:
            return ABCToken::COMMA;
        case aria::TokenType::LEFT_BRACE:
            return ABCToken::LEFT_BRACE;
        case aria::TokenType::RIGHT_BRACE:
            return ABCToken::RIGHT_BRACE;
        case aria::TokenType::LEFT_BRACKET:
            return ABCToken::LEFT_BRACKET;
        case aria::TokenType::RIGHT_BRACKET:
            return ABCToken::RIGHT_BRACKET;
        default:
            return ABCToken::UNKNOWN;
    }
}
```

### Performance Targets

- **Parsing:** < 10ms for 1000-line config
- **Memory:** Proportional to AST size (no redundant allocations)
- **Error Reporting:** Precise line/column (not "near line X")

---

## Variable Interpolator

### Purpose
Resolve `&{variable}` references recursively with cycle detection.

### Syntax

```
Simple:      "&{src}/main.aria"
Nested:      "&{base}/&{subdir}"
Environment: "&{ENV.HOME}/.config"
Literal:     "Not a var: \&{escaped}"
```

### Resolution Algorithm

**Tri-Color DFS with Memoization:**

```cpp
enum Color { WHITE, GRAY, BLACK };

std::string resolve(const std::string& input) {
    std::string result;
    size_t pos = 0;
    
    while (pos < input.size()) {
        size_t start = input.find("&{", pos);
        if (start == std::string::npos) {
            // No more variables
            result += input.substr(pos);
            break;
        }
        
        // Append literal part
        result += input.substr(pos, start - pos);
        
        // Find variable end
        size_t end = input.find('}', start + 2);
        if (end == std::string::npos) {
            throw InterpolationError("Unclosed variable: " + input.substr(start));
        }
        
        std::string varName = input.substr(start + 2, end - start - 2);
        result += resolveVariable(varName);
        
        pos = end + 1;
    }
    
    return result;
}

std::string resolveVariable(const std::string& key) {
    // Check cache (BLACK nodes)
    if (cache_.count(key)) {
        return cache_[key];
    }
    
    // Check for cycle (GRAY nodes)
    if (recursion_set_.count(key)) {
        throw CycleError("Circular variable: " + reconstructCycle(key));
    }
    
    // Mark GRAY (entering)
    recursion_set_.insert(key);
    recursion_stack_.push_back(key);
    
    // Resolve
    std::string value;
    if (key.starts_with("ENV.")) {
        value = getEnvVar(key.substr(4));
    } else if (!variables_.count(key)) {
        throw UndefinedVariableError(key);
    } else {
        value = resolve(variables_.at(key));  // Recursive!
    }
    
    // Mark BLACK (completed)
    recursion_set_.erase(key);
    recursion_stack_.pop_back();
    cache_[key] = value;
    
    return value;
}
```

### Cycle Detection

**Example cycle:**
```javascript
variables: {
    a: "&{b}",
    b: "&{c}",
    c: "&{a}"  // Cycle!
}
```

**Detection:**
1. Resolve `a` → Mark `a` GRAY
2. Resolve `b` → Mark `b` GRAY
3. Resolve `c` → Mark `c` GRAY
4. Resolve `a` → **GRAY node encountered!**
5. Reconstruct path: `recursion_stack_` = `["a", "b", "c"]`
6. Error: `"Circular dependency: a → b → c → a"`

### Environment Variables

```cpp
std::string getEnvVar(const std::string& name) {
#ifdef _WIN32
    char* buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, name.c_str()) == 0 && buf) {
        std::string result(buf);
        free(buf);
        return result;
    }
#else
    const char* val = std::getenv(name.c_str());
    if (val) {
        return std::string(val);
    }
#endif
    throw UndefinedVariableError("ENV." + name);
}
```

### Escaping

Literal `&{` can be escaped with `\&{`:

```cpp
if (pos > 0 && input[pos - 1] == '\\') {
    // Escaped, not a variable
    result += "&{";
    continue;
}
```

### Scoping Hierarchy

1. **Local (Target-specific):** Defined in target object
2. **Global (Variables section):** Defined in `variables:`
3. **Environment:** System environment variables

**Resolution order:** Local → Global → Environment → Error

### Performance Optimization

**Memoization cache prevents redundant resolution:**

```javascript
variables: {
    base: "/usr/local",
    inc: "&{base}/include",  // Resolved once
    lib: "&{base}/lib",      // Resolved once
    
    flags: "-I&{inc} -L&{lib}"  // Uses cached values
}
```

Without cache: `base` resolved 3 times  
With cache: `base` resolved 1 time

### Class Structure

```cpp
class Interpolator {
public:
    using VariableMap = std::map<std::string, std::string>;
    
    explicit Interpolator(const VariableMap& variables);
    
    std::string resolve(const std::string& input);
    void clearCache();  // For testing
    
private:
    const VariableMap& variables_;
    
    // Tri-color marking
    std::unordered_set<std::string> recursion_set_;  // GRAY
    std::vector<std::string> recursion_stack_;       // Path reconstruction
    std::unordered_map<std::string, std::string> cache_;  // BLACK
    
    std::string resolveVariable(const std::string& key);
    std::string getEnvVar(const std::string& name);
    std::string reconstructCycle(const std::string& start);
};

class InterpolationError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

class CycleError : public InterpolationError {
    using InterpolationError::InterpolationError;
};

class UndefinedVariableError : public InterpolationError {
public:
    explicit UndefinedVariableError(const std::string& key)
        : InterpolationError("Undefined variable: " + key) {}
};
```

### Edge Cases

| Input | Output | Notes |
|-------|--------|-------|
| `"no vars"` | `"no vars"` | Passthrough |
| `"&{a}&{b}"` | Concatenated | Multiple vars |
| `"\&{escaped}"` | `"&{escaped}"` | Backslash escape |
| `"&{a}&{a}"` | Same value 2x | Cache hit |
| `"&{missing}"` | Error | Undefined variable |
| `"&{}"` | Error | Empty variable name |
| `"&{a"` | Error | Unclosed brace |
| `"&{ENV.PATH}"` | System PATH | Environment var |

---

## Import Alias Configuration and Module Resolution

### Purpose

Provide a logical-to-physical path mapping system that decouples module import statements from filesystem layout, enabling large-scale refactoring, monorepo organization, and hermetic builds.

**Problem Solved:**
- **Physical Path Fragility:** `use "../../../../shared/infra/logger.aria"` breaks when files move
- **Cognitive Overload:** Developers waste time counting `../` sequences
- **Non-Hermetic Builds:** Relying on environment variables (`ARIA_PATH`) introduces non-determinism

**Solution:**
- Define logical aliases in `aria.json`: `"infra": "src/shared/infrastructure"`
- Use clean imports: `use infra.logging.logger;`
- Single-point-of-change: Update `aria.json` when refactoring, not hundreds of source files

### Configuration Schema Extension

**File:** `aria.json` (project root)

```json
{
  "project": {
    "name": "my_app",
    "version": "1.0.0",
    "authors": ["Dev <dev@example.com>"]
  },
  "build": {
    "source_path": ["src", "lib"],
    "import_aliases": {
      "utils": "src/shared/utilities",
      "infra": "src/shared/infrastructure",
      "core": "src/core",
      "@stdlib": "/usr/lib/aria/std"
    },
    "entry_point": "src/main.aria"
  }
}
```

**Field Specifications:**

| Field | Type | Purpose | Example |
|-------|------|---------|----------|
| `source_path` | `Array<String>` | Ordered list of root directories for source discovery | `["src", "lib"]` |
| `import_aliases` | `Map<String, String>` | Logical prefix → Physical path mappings | `{"utils": "src/shared/utilities"}` |
| `entry_point` | `String` | Project root file (establishes resolution context) | `"src/main.aria"` |

**Semantic Rules:**

1. **source_path Order:** First match wins. Allows shadowing (local `src/` overrides global `lib/`).
2. **Alias Keys:** Must be valid Aria identifiers (alphanumeric + underscore).
3. **Alias Values:** Relative paths from project root (directory containing `aria.json`).
4. **Priority:** Aliases checked before `source_path` entries.

### The Aria Module Resolution Strategy (AMRS)

Formal algorithm for transforming logical imports into filesystem paths. Must be implemented identically across:
- **ariac** (compiler)
- **aria_make** (build system)
- **aria-ls** (language server)

**Input:**
- `import_string`: Logical identifier from `use` statement (e.g., `"utils.logger"`)
- `requesting_file`: Absolute path of file containing import
- `config`: Parsed `aria.json` configuration

**Output:**
- Absolute path to `.aria` file or `mod.aria` directory module

**Algorithm Phases:**

#### Phase 1: Alias Resolution

```
1. Split import_string by first '.' → (prefix, remainder)
   Example: "utils.logger" → ("utils", "logger")

2. Lookup prefix in config.import_aliases:
   - HIT: Replace prefix with alias value
     Example: "utils" → "src/shared/utilities"
     New path: "src/shared/utilities/logger"
     Anchor: Project root
   - MISS: Treat as standard path (proceed to Phase 2)
```

#### Phase 2: Root Anchoring

```
If NOT aliased:
  - Relative import (starts with './' or '../'):
      Anchor = directory of requesting_file
  - Logical import (e.g., "std.io"):
      Anchors = config.source_path entries + system paths
```

#### Phase 3: Probe Loop (File vs. Directory Modules)

```
For each anchor directory A:
  Construct base path P = A / resolved_import_path
  
  Probe A: File Module
    Check: P + ".aria"
    If exists → RETURN
  
  Probe B: Directory Module
    Check: P + "/mod.aria"
    If exists → RETURN
  
  Probe C: Native Module (FFI)
    Check: P + platform_extension (.so / .dll / .dylib)
    If exists → RETURN

If all probes fail → ERROR: "Module not found"
```

**Pseudocode:**

```cpp
std::optional<std::filesystem::path> resolve(
    const std::string& import_str,
    const std::filesystem::path& requesting_file,
    const Config& config
) {
    namespace fs = std::filesystem;
    
    // 1. Alias Expansion
    std::string logical_path = import_str;
    size_t dot_pos = import_str.find('.');
    
    if (dot_pos != std::string::npos) {
        std::string prefix = import_str.substr(0, dot_pos);
        
        if (config.aliases.count(prefix)) {
            // Replace prefix with alias
            std::string alias_path = config.aliases.at(prefix);
            std::string remainder = import_str.substr(dot_pos + 1);
            logical_path = alias_path + "/" + remainder;
        }
    }
    
    // 2. Determine search roots
    std::vector<fs::path> roots;
    
    if (logical_path.starts_with("./") || logical_path.starts_with("../")) {
        // Relative import
        roots.push_back(requesting_file.parent_path());
    } else {
        // Logical import: search all source roots
        for (const auto& src_path : config.source_path) {
            roots.push_back(config.project_root / src_path);
        }
        
        // Add system paths (e.g., /usr/lib/aria)
        if (auto aria_path = std::getenv("ARIA_PATH")) {
            roots.push_back(aria_path);
        }
    }
    
    // 3. Probe Loop
    for (const auto& root : roots) {
        fs::path base = root / logical_path;
        
        // Probe A: File module (utils.aria)
        fs::path file_path = base;
        file_path += ".aria";
        if (fs::exists(file_path) && fs::is_regular_file(file_path)) {
            return fs::canonical(file_path);
        }
        
        // Probe B: Directory module (utils/mod.aria)
        fs::path dir_path = base / "mod.aria";
        if (fs::exists(dir_path) && fs::is_regular_file(dir_path)) {
            return fs::canonical(dir_path);
        }
        
        // Probe C: Native module (utils.so / utils.dll)
#ifdef _WIN32
        fs::path native_path = base;
        native_path += ".dll";
#elif __APPLE__
        fs::path native_path = base;
        native_path += ".dylib";
#else
        fs::path native_path = base;
        native_path += ".so";
#endif
        if (fs::exists(native_path) && fs::is_regular_file(native_path)) {
            return fs::canonical(native_path);
        }
    }
    
    // Not found
    return std::nullopt;
}
```

### Module Resolver Class

```cpp
// File: include/resolve/module_resolver.h

namespace aria::resolve {

struct Config {
    std::filesystem::path project_root;
    std::vector<std::string> source_path;
    std::map<std::string, std::string> aliases;
};

class ModuleResolver {
public:
    explicit ModuleResolver(const Config& config);
    
    // Resolve logical import to filesystem path
    std::optional<std::filesystem::path> resolve(
        const std::string& import_str,
        const std::filesystem::path& requesting_file
    ) const;
    
    // Check if path exists in cache (optimization)
    bool isCached(const std::filesystem::path& path) const;
    
    // Invalidate cache (for LSP file watchers)
    void invalidateCache();
    
private:
    Config config_;
    
    // Performance cache
    mutable std::unordered_set<std::string> file_cache_;
    mutable std::mutex cache_mutex_;
    
    // Helpers
    std::vector<std::filesystem::path> getSearchRoots(
        const std::string& logical_path,
        const std::filesystem::path& requesting_file
    ) const;
    
    std::optional<std::filesystem::path> probeModule(
        const std::filesystem::path& base_path
    ) const;
};

} // namespace aria::resolve
```

### Integration with AriaBuild (Dependency Scanner)

AriaBuild uses a lightweight `DependencyScanner` to extract `use` statements without invoking full compiler:

```cpp
// File: src/build/dependency_scanner.cpp

namespace aria::build {

class DependencyScanner {
public:
    explicit DependencyScanner(const resolve::Config& config)
        : resolver_(config) {}
    
    std::vector<std::filesystem::path> scan(const std::filesystem::path& file) {
        std::string source = read_file(file);
        std::vector<std::filesystem::path> dependencies;
        
        // Simple regex or lexer to find 'use' statements
        std::regex use_pattern(R"(use\s+([a-zA-Z0-9_.]+);)");
        
        auto begin = std::sregex_iterator(source.begin(), source.end(), use_pattern);
        auto end = std::sregex_iterator();
        
        for (auto it = begin; it != end; ++it) {
            std::string import_str = (*it)[1].str();
            
            // Resolve using AMRS
            auto resolved = resolver_.resolve(import_str, file);
            
            if (resolved) {
                dependencies.push_back(*resolved);
            } else {
                // Configuration error: fail fast
                throw std::runtime_error(
                    "Module not found: " + import_str + 
                    " (imported by " + file.string() + ")"
                );
            }
        }
        
        return dependencies;
    }
    
private:
    resolve::ModuleResolver resolver_;
};

} // namespace aria::build
```

**Benefit:** Build fails immediately with clear error if alias misconfigured, before expensive compilation phase.

### Integration with Compiler (ariac)

Compiler uses same resolver for semantic analysis:

```cpp
// File: src/frontend/module_loader.cpp

namespace aria::frontend {

class ModuleLoader {
public:
    explicit ModuleLoader(const resolve::Config& config)
        : resolver_(config) {}
    
    std::unique_ptr<AST> loadModule(
        const std::string& import_str,
        const std::filesystem::path& requesting_file
    ) {
        auto resolved_path = resolver_.resolve(import_str, requesting_file);
        
        if (!resolved_path) {
            // Generate helpful error with suggestions
            std::string msg = "Module '" + import_str + "' not found.\n";
            
            // Extract prefix for diagnostic
            size_t dot = import_str.find('.');
            if (dot != std::string::npos) {
                std::string prefix = import_str.substr(0, dot);
                if (config_.aliases.count(prefix)) {
                    msg += "Alias '" + prefix + "' resolves to '" + 
                           config_.aliases.at(prefix) + "'.\n";
                    msg += "Searched for: " + /* list attempted paths */ + "\n";
                }
            }
            
            throw ModuleNotFoundError(msg);
        }
        
        // Parse module AST
        return parseFile(*resolved_path);
    }
    
private:
    resolve::ModuleResolver resolver_;
};

} // namespace aria::frontend
```

### Integration with Language Server (AriaLS)

LSP must share exact resolution logic for "Go to Definition" and autocomplete:

```cpp
// File: src/lsp/aria_ls.cpp

namespace aria::lsp {

class AriaLS {
public:
    void onDefinitionRequest(const LSPRequest& req) {
        // Extract import string at cursor position
        std::string import_str = extractImportAt(req.file, req.line, req.col);
        
        // Resolve using shared AMRS
        auto resolved = resolver_.resolve(import_str, req.file);
        
        if (resolved) {
            // Send "jump to" location
            sendResponse({
                .uri = file_uri(*resolved),
                .line = 1,
                .column = 1
            });
        } else {
            sendError("Module not found");
        }
    }
    
    void onCompletionRequest(const LSPRequest& req) {
        std::string partial = extractPartialImport(req);
        
        // If "use utils." → suggest files in utils/ directory
        size_t dot = partial.rfind('.');
        if (dot != std::string::npos) {
            std::string prefix = partial.substr(0, dot);
            auto base = resolver_.resolve(prefix, req.file);
            
            if (base && fs::is_directory(*base)) {
                // List directory, suggest file names
                for (auto& entry : fs::directory_iterator(*base)) {
                    if (entry.path().extension() == ".aria") {
                        completions.push_back(entry.path().stem().string());
                    }
                }
            }
        }
        
        sendCompletions(completions);
    }
    
private:
    resolve::ModuleResolver resolver_;
};

} // namespace aria::lsp
```

**Critical:** LSP watches `aria.json` and re-initializes resolver on changes to reflect alias updates immediately.

### Configuration Data Structures

```cpp
// File: include/build/config.h

namespace aria::build {

struct BuildConfig {
    // Project metadata
    std::string project_name;
    std::string version;
    std::vector<std::string> authors;
    
    // Module resolution
    std::vector<std::string> source_path = {"src"};
    std::map<std::string, std::string> import_aliases;
    std::string entry_point = "src/main.aria";
    
    // Build targets (from targets section)
    std::vector<Target> targets;
};

void from_json(const nlohmann::json& j, BuildConfig& config) {
    if (j.contains("build")) {
        auto& build = j["build"];
        
        if (build.contains("source_path")) {
            config.source_path = build["source_path"].get<std::vector<std::string>>();
        }
        
        if (build.contains("import_aliases")) {
            config.import_aliases = build["import_aliases"].get<std::map<std::string, std::string>>();
        }
        
        if (build.contains("entry_point")) {
            config.entry_point = build["entry_point"].get<std::string>();
        }
    }
}

} // namespace aria::build
```

### Performance: StatCache Optimization

Filesystem `stat()` calls are expensive. Cache file existence during globbing phase:

```cpp
class ModuleResolver {
public:
    void buildCache(const std::filesystem::path& root) {
        std::lock_guard lock(cache_mutex_);
        file_cache_.clear();
        
        // Scan all .aria files once at startup
        for (auto& entry : fs::recursive_directory_iterator(root)) {
            if (entry.is_regular_file() && entry.path().extension() == ".aria") {
                file_cache_.insert(fs::canonical(entry.path()).string());
            }
        }
    }
    
    bool isCached(const std::filesystem::path& path) const {
        std::lock_guard lock(cache_mutex_);
        return file_cache_.count(path.string()) > 0;
    }
};
```

**Invalidation:** LSP watches filesystem (inotify/FSEvents/ReadDirectoryChangesW) and updates cache on file create/delete.

### Edge Cases and Security

#### Case 1: Symlink Handling

```cpp
// Always canonicalize to prevent cycles
std::filesystem::path resolved = fs::canonical(candidate);

// Security: Ensure resolved path is within project boundary
if (!is_subpath(resolved, config.project_root)) {
    throw SecurityException("Module resolution escaped project root");
}
```

#### Case 2: Case Sensitivity (Cross-Platform)

```cpp
// Windows: Case-insensitive filesystem but case-preserving
// Must ensure import "Utils.Logger" fails if file is "utils/logger.aria"

if constexpr (is_windows) {
    // On Windows, iterate directory and strcmp filenames
    for (auto& entry : fs::directory_iterator(parent)) {
        if (entry.path().filename().string() == requested_name) {
            // Exact case match required
            return entry.path();
        }
    }
    return std::nullopt;  // Case mismatch
}
```

#### Case 3: Alias Shadowing

If alias `net` defined AND physical directory `src/net/` exists:

**Priority:** Alias wins (explicit configuration overrides implicit structure).

**Rationale:** Allows monkey-patching (e.g., replace `net` with `test/mocks/net` during testing).

### Glob Integration Example

```json
{
  "build": {
    "import_aliases": {
      "utils": "src/shared/utilities"
    }
  },
  "targets": [
    {
      "name": "app",
      "sources": ["&{utils}/**/*.aria"]  // Variable interpolation + globbing
    }
  ]
}
```

**Expansion:**
1. Interpolator resolves `&{utils}` → `"src/shared/utilities"`
2. Glob engine expands `src/shared/utilities/**/*.aria`
3. Result: All `.aria` files in utilities directory

### Testing Strategy

```cpp
TEST(ModuleResolver, AliasResolution) {
    Config config;
    config.project_root = "/project";
    config.aliases["utils"] = "src/shared/utilities";
    
    // Create test file structure
    TempDir tmp("/project");
    tmp.write("src/shared/utilities/logger.aria", "// Logger");
    
    ModuleResolver resolver(config);
    
    auto resolved = resolver.resolve("utils.logger", "/project/src/main.aria");
    
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(*resolved, "/project/src/shared/utilities/logger.aria");
}

TEST(ModuleResolver, SourcePathPrecedence) {
    Config config;
    config.project_root = "/project";
    config.source_path = {"src", "lib"};
    
    TempDir tmp("/project");
    tmp.write("src/math.aria", "// Local math");
    tmp.write("lib/math.aria", "// Library math");
    
    ModuleResolver resolver(config);
    
    auto resolved = resolver.resolve("math", "/project/src/main.aria");
    
    // Should resolve to src/math.aria (first in source_path)
    EXPECT_EQ(*resolved, "/project/src/math.aria");
}
```

### Implementation Checklist

- [ ] Add `source_path` and `import_aliases` to `aria.json` schema
- [ ] Implement `ModuleResolver` class with AMRS algorithm
- [ ] Add alias expansion logic (prefix matching)
- [ ] Implement probe loop (file / directory / native modules)
- [ ] Add StatCache optimization
- [ ] Integrate with `DependencyScanner` in AriaBuild
- [ ] Integrate with `ModuleLoader` in compiler
- [ ] Integrate with AriaLS for "Go to Definition"
- [ ] Add autocomplete for alias prefixes in LSP
- [ ] Handle case sensitivity on Windows
- [ ] Add security check (sandbox boundary)
- [ ] Add unit tests for all resolution scenarios
- [ ] Document in user manual with examples

**Estimated Implementation Time:** 4-6 days
- Core resolver: 1-2 days
- AriaBuild integration: 1 day
- Compiler integration: 1 day
- LSP integration: 1 day
- Testing + edge cases: 1-2 days

---

**Next:** [20_FILE_DISCOVERY.md](./20_FILE_DISCOVERY.md) - Globbing Engine specification
