# Tooling Integration

This document specifies how aria_make integrates with development tools through standardized interfaces: `compile_commands.json` for IDE/LSP integration and `.d` files for compiler-generated dependencies.

---

## Overview

**Goal:** Enable seamless integration with:
- **Language servers** (code completion, go-to-definition, diagnostics)
- **IDEs** (VS Code, CLion, Emacs, Vim)
- **Static analysis tools** (clang-tidy, cppcheck equivalents for Aria)
- **Refactoring tools**

### Key Interfaces

| File | Format | Consumer | Purpose |
|------|--------|----------|---------|
| `compile_commands.json` | JSON | LSP servers, IDEs | Compilation database |
| `*.d` | Make-style | Build system | Implicit dependencies |

---

## compile_commands.json - Compilation Database

### Purpose
Provide IDEs/LSPs with exact compilation commands for each source file, enabling accurate code intelligence.

### Format Specification

```json
[
  {
    "directory": "/home/user/myproject",
    "file": "src/main.aria",
    "command": "ariac src/main.aria -o build/main.o -I include -O2",
    "output": "build/main.o"
  },
  {
    "directory": "/home/user/myproject",
    "file": "src/utils.aria",
    "command": "ariac src/utils.aria -o build/utils.o -I include -O2",
    "output": "build/utils.o"
  }
]
```

**Fields:**
- `directory` - Working directory for compilation (absolute path)
- `file` - Source file path (can be relative to `directory`)
- `command` - Full compiler invocation (as shell command)
- `output` - Output file path (optional but recommended)

### Why This Matters

**Without compile_commands.json:**
```
LSP: What's the type of `x`?
     → Don't know include paths
     → Don't know imported modules
     → Can't resolve symbols
     → ❌ No code intelligence
```

**With compile_commands.json:**
```
LSP: What's the type of `x`?
     → Read compile_commands.json
     → Parse command: -I include -I ../lib/math
     → Load imports from those paths
     → Resolve symbol: x is Vec<i32>
     → ✅ Accurate completion/diagnostics
```

---

## CompileDBWriter Implementation

### Class Structure

```cpp
class CompileDBWriter {
public:
    CompileDBWriter();
    
    // Add compilation entry
    void addCompilation(
        const std::filesystem::path& working_dir,
        const std::filesystem::path& source_file,
        const std::string& binary,
        const std::vector<std::string>& args,
        const std::filesystem::path& output_file
    );
    
    // Write to file
    bool write(const std::filesystem::path& output_path = "compile_commands.json");
    
    // Clear all entries
    void clear();
    
    // Statistics
    size_t entryCount() const { return entries_.size(); }
    
private:
    struct Entry {
        std::filesystem::path directory;
        std::filesystem::path file;
        std::string command;
        std::filesystem::path output;
    };
    
    std::vector<Entry> entries_;
    
    // Helpers
    std::string buildCommandString(const std::string& binary,
                                   const std::vector<std::string>& args);
    std::string escapeJSON(const std::string& str);
};
```

### Implementation

```cpp
void CompileDBWriter::addCompilation(
    const fs::path& working_dir,
    const fs::path& source_file,
    const std::string& binary,
    const std::vector<std::string>& args,
    const fs::path& output_file
) {
    Entry entry;
    
    entry.directory = fs::absolute(working_dir);
    entry.file = source_file;
    entry.command = buildCommandString(binary, args);
    entry.output = output_file;
    
    entries_.push_back(entry);
}

std::string CompileDBWriter::buildCommandString(
    const std::string& binary,
    const std::vector<std::string>& args
) {
    std::ostringstream oss;
    
    oss << escapeJSON(binary);
    
    for (const auto& arg : args) {
        oss << " " << escapeJSON(arg);
    }
    
    return oss.str();
}

std::string CompileDBWriter::escapeJSON(const std::string& str) {
    std::ostringstream oss;
    
    for (char c : str) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b";  break;
            case '\f': oss << "\\f";  break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:
                if (c < 0x20) {
                    // Control character: \uXXXX
                    oss << "\\u" << std::hex << std::setw(4) 
                        << std::setfill('0') << static_cast<int>(c);
                } else {
                    oss << c;
                }
        }
    }
    
    return oss.str();
}

bool CompileDBWriter::write(const fs::path& output_path) {
    try {
        std::ofstream file(output_path);
        
        file << "[\n";
        
        for (size_t i = 0; i < entries_.size(); ++i) {
            const Entry& entry = entries_[i];
            
            file << "  {\n";
            file << "    \"directory\": \"" << escapeJSON(entry.directory.string()) << "\",\n";
            file << "    \"file\": \"" << escapeJSON(entry.file.string()) << "\",\n";
            file << "    \"command\": \"" << entry.command << "\",\n";
            file << "    \"output\": \"" << escapeJSON(entry.output.string()) << "\"\n";
            file << "  }";
            
            if (i < entries_.size() - 1) {
                file << ",";
            }
            
            file << "\n";
        }
        
        file << "]\n";
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to write compile_commands.json: " 
                  << e.what() << "\n";
        return false;
    }
}
```

### Critical: Double Escaping

**Problem:** Paths/args may contain special characters that need escaping in BOTH the command string AND the JSON encoding.

```cpp
// Example: Source file with space
std::string source = "src/my file.aria";

// STEP 1: Shell escaping for command field
std::string shell_escaped = "\"src/my file.aria\"";

// STEP 2: JSON escaping
std::string json_escaped = "\\\"src/my file.aria\\\"";

// Final JSON:
// {
//   "command": "ariac \\\"src/my file.aria\\\" -o build/my\\ file.o"
// }
```

**Our approach:** Skip shell escaping (assume no spaces in paths), only JSON-escape.

**Rationale:**
- Build system controls paths → enforce no spaces in build.aria
- Simplifies implementation
- If needed later, add shell escaping layer

### Integration with BuildScheduler

```cpp
class BuildScheduler {
public:
    // ... (existing methods)
    
    bool build(const BuildOptions& opts = {}) {
        // ... (existing build logic)
        
        // After build, generate compile_commands.json
        if (opts.generate_compile_db) {
            generateCompileDB();
        }
        
        return !build_failed_;
    }
    
private:
    void generateCompileDB() {
        CompileDBWriter writer;
        
        for (Node* node : graph_.getAllNodes()) {
            for (const auto& src : node->source_files) {
                auto [binary, args] = toolchain_.buildCompileCommand(
                    node, src, node->output_file
                );
                
                writer.addCompilation(
                    fs::current_path(),  // Working directory
                    src,
                    binary,
                    args,
                    node->output_file
                );
            }
        }
        
        if (!writer.write()) {
            std::cerr << "[WARNING] Failed to write compile_commands.json\n";
        } else {
            std::cout << "[INFO] Generated compile_commands.json ("
                      << writer.entryCount() << " entries)\n";
        }
    }
};
```

---

## Dependency Files (.d) - Compiler-Generated Dependencies

### Purpose
Track implicit dependencies (imported modules) to trigger rebuilds when imports change.

### Format (Make-style)

**Produced by compiler with `-MD -MF <file>.d` flags:**

```makefile
build/main.o: src/main.aria \
  include/utils.aria \
  include/math.aria \
  /usr/local/include/aria/std/vector.aria
```

**Format:**
```
<target>: <dependency1> \
  <dependency2> \
  <dependency3>
```

### Why Needed?

**Without .d files:**
```aria
// main.aria
import utils;  // ← Compiler resolves to include/utils.aria

// User modifies include/utils.aria
// Build system doesn't know main.aria depends on it
// → main.aria NOT rebuilt (BUG!)
```

**With .d files:**
```
1. ariac generates build/main.o.d during compilation:
   build/main.o: src/main.aria include/utils.aria
   
2. Next build: aria_make reads build/main.o.d
   
3. Detects include/utils.aria modified
   
4. Rebuilds main.aria ✅
```

### Compiler Integration

**ariac must support:**
```bash
ariac src/main.aria -o build/main.o -MD -MF build/main.o.d
```

**Flags:**
- `-MD` - Generate dependency file
- `-MF <path>` - Output path for .d file

### Implementation (see 50_INCREMENTAL_BUILDS.md)

**Parser:** `DependencyFileParser::parse()` - Reads .d file, extracts dependencies

**Integration:** `Node::loadImplicitDependencies()` - Loads .d file into node

**Dirty Check:** `BuildState::isTargetDirty()` - Checks implicit deps timestamps

---

## DependencyPrinter - ariac Integration

### Purpose
Helper for ariac compiler to generate .d files during compilation.

### Class Structure

```cpp
class DependencyPrinter {
public:
    DependencyPrinter();
    
    // Set target (output file)
    void setTarget(const std::filesystem::path& target);
    
    // Add dependency (source file or imported module)
    void addDependency(const std::filesystem::path& dep);
    
    // Write to .d file
    bool write(const std::filesystem::path& output_path);
    
private:
    std::filesystem::path target_;
    std::vector<std::filesystem::path> dependencies_;
    
    std::string escapePath(const std::filesystem::path& path);
};
```

### Implementation

```cpp
void DependencyPrinter::setTarget(const fs::path& target) {
    target_ = target;
}

void DependencyPrinter::addDependency(const fs::path& dep) {
    // Normalize path
    fs::path normalized = fs::absolute(dep).lexically_normal();
    
    // Avoid duplicates
    if (std::find(dependencies_.begin(), dependencies_.end(), normalized) 
        == dependencies_.end()) {
        dependencies_.push_back(normalized);
    }
}

std::string DependencyPrinter::escapePath(const fs::path& path) {
    std::string str = path.string();
    std::string escaped;
    
    for (char c : str) {
        if (c == ' ') {
            escaped += "\\ ";  // Escape spaces for Make
        } else if (c == '#') {
            escaped += "\\#";  // Escape Make comments
        } else {
            escaped += c;
        }
    }
    
    return escaped;
}

bool DependencyPrinter::write(const fs::path& output_path) {
    try {
        std::ofstream file(output_path);
        
        // Write target
        file << escapePath(target_) << ":";
        
        // Write dependencies (with line continuations)
        for (size_t i = 0; i < dependencies_.size(); ++i) {
            if (i == 0) {
                file << " ";
            } else {
                file << " \\\n  ";  // Line continuation
            }
            
            file << escapePath(dependencies_[i]);
        }
        
        file << "\n";
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to write .d file: " << e.what() << "\n";
        return false;
    }
}
```

### Example Usage in ariac

```cpp
// During compilation of src/main.aria

DependencyPrinter dep_printer;

// Set target (output file)
dep_printer.setTarget("build/main.o");

// Add source file
dep_printer.addDependency("src/main.aria");

// Add imports (discovered during parsing)
for (const auto& import : parser.getImports()) {
    fs::path import_path = module_resolver.resolve(import);
    dep_printer.addDependency(import_path);
}

// Write .d file
dep_printer.write("build/main.o.d");
```

### Output Example

```makefile
build/main.o: src/main.aria \
  include/utils.aria \
  include/math.aria \
  /usr/local/include/aria/std/vector.aria
```

---

## Full Workflow with Tooling Integration

### 1. User Opens Project in VS Code

```bash
$ code myproject/
```

### 2. VS Code Loads Aria Language Server

```
VS Code Extension: aria.vscode-aria
  ↓
Starts: aria-language-server
  ↓
LSP: "Where's compile_commands.json?"
  ↓
Searches: myproject/compile_commands.json
  ↓
Status: ❌ Not found → Limited code intelligence
```

### 3. User Runs aria_make

```bash
$ aria_make build
```

**aria_make does:**
1. Parse build.aria
2. Construct dependency graph
3. Compile sources with `-MD -MF` flags
4. Compiler generates .d files for each target
5. Link executables
6. **Generate compile_commands.json**

### 4. LSP Detects compile_commands.json

```
aria-language-server:
  ↓
Watches: compile_commands.json
  ↓
Event: File created!
  ↓
Reloads: Parse compile_commands.json
  ↓
Indexes: All source files with correct flags
  ↓
Status: ✅ Full code intelligence enabled
```

### 5. User Edits Code

```aria
// src/main.aria
import utils;
import math;

fn main() {
    let x = utils.helpe  // ← Cursor here, press Ctrl+Space
}
```

**LSP provides:**
- **Completion:** `helper()`, `helperV2()`, etc.
- **Signature:** `fn helper(x: i32) -> String`
- **Diagnostics:** "unused import: math"

### 6. User Modifies Import

```bash
$ vim include/utils.aria  # Add new function
```

### 7. Next Build is Incremental

```bash
$ aria_make build
```

**aria_make does:**
1. Load build state
2. Read build/main.o.d:
   ```
   build/main.o: src/main.aria include/utils.aria
   ```
3. Check timestamps:
   ```
   include/utils.aria: 2024-01-01 12:35:00
   build/main.o:       2024-01-01 12:30:00
   ```
4. **Detect:** include/utils.aria newer → rebuild main.o
5. Recompile only main.aria
6. Update compile_commands.json (regenerated)

---

## Platform Considerations

### Path Normalization

**Problem:** Different platforms use different path separators.

| Platform | Separator | Example |
|----------|-----------|---------|
| Linux/macOS | `/` | `/home/user/project/src/main.aria` |
| Windows | `\` | `C:\Users\user\project\src\main.aria` |

**Solution:** Always use forward slashes in JSON output.

```cpp
std::string CompileDBWriter::normalizePath(const fs::path& path) {
    std::string str = path.string();
    
    // Replace backslashes with forward slashes
    std::replace(str.begin(), str.end(), '\\', '/');
    
    return str;
}
```

### Working Directory

**Always absolute paths:**
```json
{
  "directory": "/home/user/myproject",  // ✅ Absolute
  "file": "src/main.aria",              // ✅ Relative (to directory)
  "output": "build/main.o"              // ✅ Relative (to directory)
}
```

**NOT:**
```json
{
  "directory": ".",                     // ❌ Ambiguous
  "file": "src/main.aria",
  "output": "build/main.o"
}
```

### Special Characters in Paths

**Supported:**
- Spaces: `src/my file.aria` → JSON-escaped
- Unicode: `src/привет.aria` → UTF-8 encoded
- Dots: `src/../utils.aria` → Normalized to `utils.aria`

**NOT supported (enforce in build.aria validation):**
- Newlines, tabs in filenames
- Null bytes
- Wildcards (*, ?, [])

---

## Testing Strategy

### CompileDBWriter Tests

```cpp
TEST(CompileDBWriter, BasicEntry) {
    CompileDBWriter writer;
    
    writer.addCompilation(
        "/home/user/project",
        "src/main.aria",
        "ariac",
        {"src/main.aria", "-o", "build/main.o", "-O2"},
        "build/main.o"
    );
    
    writer.write("test_compile_commands.json");
    
    // Verify JSON is valid
    std::ifstream file("test_compile_commands.json");
    nlohmann::json json;
    file >> json;
    
    ASSERT_EQ(json.size(), 1);
    ASSERT_EQ(json[0]["directory"], "/home/user/project");
    ASSERT_EQ(json[0]["file"], "src/main.aria");
    ASSERT_EQ(json[0]["command"], "ariac src/main.aria -o build/main.o -O2");
}

TEST(CompileDBWriter, EscapeSpecialChars) {
    CompileDBWriter writer;
    
    writer.addCompilation(
        "/home/user/project",
        "src/\"quoted\".aria",  // Quotes in filename
        "ariac",
        {"src/\"quoted\".aria", "-o", "build/quoted.o"},
        "build/quoted.o"
    );
    
    writer.write("test_escape.json");
    
    std::ifstream file("test_escape.json");
    nlohmann::json json;
    file >> json;
    
    // Quotes should be escaped
    ASSERT_EQ(json[0]["file"], "src/\\\"quoted\\\".aria");
}
```

### DependencyPrinter Tests

```cpp
TEST(DependencyPrinter, BasicOutput) {
    DependencyPrinter printer;
    
    printer.setTarget("build/main.o");
    printer.addDependency("src/main.aria");
    printer.addDependency("include/utils.aria");
    
    printer.write("test.d");
    
    std::ifstream file("test.d");
    std::string line;
    std::getline(file, line);
    
    ASSERT_EQ(line, "build/main.o: src/main.aria \\");
    
    std::getline(file, line);
    ASSERT_EQ(line, "  include/utils.aria");
}

TEST(DependencyPrinter, EscapeSpaces) {
    DependencyPrinter printer;
    
    printer.setTarget("build/my file.o");
    printer.addDependency("src/my file.aria");
    
    printer.write("test.d");
    
    std::ifstream file("test.d");
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    // Spaces should be escaped
    ASSERT_NE(content.find("build/my\\ file.o"), std::string::npos);
    ASSERT_NE(content.find("src/my\\ file.aria"), std::string::npos);
}
```

### Integration Test: LSP Workflow

```cpp
TEST(Integration, LSPWorkflow) {
    // 1. Build project
    BuildScheduler scheduler(graph, pool, toolchain);
    bool success = scheduler.build({.generate_compile_db = true});
    
    ASSERT_TRUE(success);
    ASSERT_TRUE(fs::exists("compile_commands.json"));
    
    // 2. Parse compile_commands.json
    std::ifstream file("compile_commands.json");
    nlohmann::json json;
    file >> json;
    
    ASSERT_GT(json.size(), 0);
    
    // 3. Verify first entry
    auto& entry = json[0];
    ASSERT_TRUE(entry.contains("directory"));
    ASSERT_TRUE(entry.contains("file"));
    ASSERT_TRUE(entry.contains("command"));
    ASSERT_TRUE(entry.contains("output"));
    
    // 4. Verify command includes source file
    std::string command = entry["command"];
    std::string file_path = entry["file"];
    ASSERT_NE(command.find(file_path), std::string::npos);
}
```

---

## FFI Linking Architecture Enhancement

### Overview

**Source:** aria_01.txt - Architectural Specification for FFI Linking  
**Priority:** HIGH - Essential for C/C++ library interoperability  
**Complexity:** MEDIUM-HIGH  

As Aria matures toward v0.1.0, the ability to link against existing C/C++ libraries is critical for ecosystem adoption. The current aria_make system compiles Aria sources to LLVM IR and executes via `lli`, but lacks a formalized "Linker Driver" role capable of orchestrating system linkers (`ld`, `link.exe`, `lld`) with proper library search paths, symbol resolution, and ABI compatibility.

### The Challenge: Three-Tier Complexity

1. **Schema Extension** - Add `libraries` field to target configuration
2. **Flag Generation** - Translate abstract library names to platform-specific linker flags
3. **Dependency Propagation** - Automatically propagate transitive library dependencies through the build graph

### Configuration Schema Extension

#### New Target Fields

```javascript
targets: [
    {
        name: "ssl_client",
        type: "executable",
        sources: "src/client/**/*.aria",
        output: "build/ssl_client",
        
        // NEW: Library linking
        libraries: ["ssl", "crypto"],  // Abstract library names
        lib_search_paths: [            // Optional: additional search paths
            "/usr/local/ssl/lib",
            "&{OPENSSL_ROOT}/lib"
        ],
        static_libs: [                 // Optional: force static linking
            "/opt/custom/libfoo.a"
        ]
    }
]
```

#### Field Semantics

| Field | Type | Purpose | Platform Translation |
|-------|------|---------|---------------------|
| `libraries` | `string[]` | Abstract library names | Linux: `-lssl`, Windows: `ssl.lib` |
| `lib_search_paths` | `string[]` | Additional search directories | Linux: `-L/path`, Windows: `/LIBPATH:/path` |
| `static_libs` | `string[]` | Explicit `.a`/`.lib` paths | Passed directly to linker |

### Platform-Specific Flag Generation

#### Linux (ld/lld with ELF)

```cpp
std::vector<std::string> LinkerFlagGenerator::generateLinux(
    const std::vector<std::string>& libraries,
    const std::vector<std::string>& search_paths
) {
    std::vector<std::string> flags;
    
    // Add search paths first
    for (const auto& path : search_paths) {
        flags.push_back("-L" + path);
    }
    
    // Add libraries (order matters!)
    for (const auto& lib : libraries) {
        flags.push_back("-l" + lib);
    }
    
    // Common runtime paths
    flags.push_back("-Wl,-rpath,$ORIGIN");  // Executable-relative
    flags.push_back("-Wl,--as-needed");     // Drop unused libs
    
    return flags;
}
```

**Example Output:**
```bash
lld -o build/ssl_client build/client.o \
    -L/usr/local/ssl/lib \
    -lssl -lcrypto \
    -Wl,-rpath,$ORIGIN \
    -Wl,--as-needed
```

#### Windows (link.exe with PE/COFF)

```cpp
std::vector<std::string> LinkerFlagGenerator::generateWindows(
    const std::vector<std::string>& libraries,
    const std::vector<std::string>& search_paths
) {
    std::vector<std::string> flags;
    
    // Add search paths
    for (const auto& path : search_paths) {
        flags.push_back("/LIBPATH:" + path);
    }
    
    // Add libraries with .lib extension
    for (const auto& lib : libraries) {
        // Handle both "ssl" and "ssl.lib" input
        std::string lib_name = lib;
        if (!lib_name.ends_with(".lib")) {
            lib_name += ".lib";
        }
        flags.push_back(lib_name);
    }
    
    // Windows-specific: embed manifest
    flags.push_back("/MANIFEST:EMBED");
    flags.push_back("/MANIFESTUAC:NO");
    
    return flags;
}
```

**Example Output:**
```cmd
link.exe /OUT:build\ssl_client.exe build\client.obj ^
    /LIBPATH:C:\OpenSSL\lib ^
    ssl.lib crypto.lib ^
    /MANIFEST:EMBED
```

### Transitive Dependency Propagation

#### The Problem

```javascript
// Target A depends on library "ssl"
targets: [
    {
        name: "crypto_utils",
        type: "library",
        libraries: ["ssl", "crypto"]
    },
    {
        name: "my_app",
        type: "executable",
        dependencies: ["crypto_utils"]
        // PROBLEM: Does my_app automatically get -lssl -lcrypto?
    }
]
```

Without propagation, `my_app` will fail to link with "undefined reference to SSL_connect".

#### The Solution: Library Accumulation

```cpp
class DependencyGraph {
    // Accumulate libraries from dependencies
    std::vector<std::string> collectTransitiveLibraries(Node* node) {
        std::unordered_set<std::string> libs_set;
        std::queue<Node*> queue;
        std::unordered_set<Node*> visited;
        
        queue.push(node);
        visited.insert(node);
        
        while (!queue.empty()) {
            Node* current = queue.front();
            queue.pop();
            
            // Add this node's libraries
            for (const auto& lib : current->libraries) {
                libs_set.insert(lib);
            }
            
            // Traverse dependencies
            for (Node* dep : current->dependencies_) {
                if (visited.find(dep) == visited.end()) {
                    queue.push(dep);
                    visited.insert(dep);
                }
            }
        }
        
        // Convert to vector (preserve insertion order for determinism)
        std::vector<std::string> result(libs_set.begin(), libs_set.end());
        std::sort(result.begin(), result.end());  // Deterministic
        
        return result;
    }
};
```

#### Usage in ToolchainOrchestrator

```cpp
std::string ToolchainOrchestrator::generateLinkCommand(Node* node) {
    std::ostringstream cmd;
    
    // Collect all libraries (transitive)
    auto all_libs = graph_->collectTransitiveLibraries(node);
    
    // Generate platform flags
    #ifdef _WIN32
        auto flags = LinkerFlagGenerator::generateWindows(all_libs, node->lib_search_paths);
    #else
        auto flags = LinkerFlagGenerator::generateLinux(all_libs, node->lib_search_paths);
    #endif
    
    // Build command
    cmd << "lld -o " << node->output_file;
    for (const auto& obj : node->object_files) {
        cmd << " " << obj;
    }
    for (const auto& flag : flags) {
        cmd << " " << flag;
    }
    
    return cmd.str();
}
```

### Integration with BuildState

#### Command Signature Hashing

Library changes must trigger rebuilds:

```cpp
std::string CommandHasher::hashLinkCommand(
    const std::vector<std::string>& objects,
    const std::vector<std::string>& libraries,
    const std::vector<std::string>& flags
) {
    std::ostringstream sig;
    
    // Hash objects (sorted)
    for (const auto& obj : sorted(objects)) {
        sig << obj << ";";
    }
    
    // Hash libraries (already sorted by collectTransitiveLibraries)
    for (const auto& lib : libraries) {
        sig << lib << ";";
    }
    
    // Hash flags
    for (const auto& flag : sorted(flags)) {
        sig << flag << ";";
    }
    
    return sha256(sig.str());
}
```

**Rebuild Trigger Example:**
```javascript
// build.aria changed: "ssl" → "ssl", "crypto"
{
    libraries: ["ssl", "crypto"]  // Was: ["ssl"]
}
```
→ Command hash changes → Executable marked dirty → Relink

### Error Handling

#### Missing Library Detection

```cpp
bool LinkerValidator::validateLibraries(
    const std::vector<std::string>& libraries,
    const std::vector<std::string>& search_paths
) {
    for (const auto& lib : libraries) {
        bool found = false;
        
        // Check standard paths
        for (const auto& std_path : {"/usr/lib", "/usr/local/lib"}) {
            if (fs::exists(std_path / ("lib" + lib + ".a")) ||
                fs::exists(std_path / ("lib" + lib + ".so"))) {
                found = true;
                break;
            }
        }
        
        // Check custom search paths
        for (const auto& path : search_paths) {
            if (fs::exists(path / ("lib" + lib + ".a")) ||
                fs::exists(path / ("lib" + lib + ".so"))) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            std::cerr << "ERROR: Library '" << lib << "' not found in:\n";
            for (const auto& path : search_paths) {
                std::cerr << "  - " << path << "\n";
            }
            return false;
        }
    }
    
    return true;
}
```

### Implementation Phases

#### Phase 1: Schema & Parsing (2-3 days)
- Add `libraries`, `lib_search_paths`, `static_libs` to target schema
- Update ConfigParser to recognize new fields
- Add validation (no empty strings, paths exist)

#### Phase 2: Flag Generation (3-4 days)
- Implement `LinkerFlagGenerator` class
- Linux flag synthesis (test with `gcc` and `clang`)
- Windows flag synthesis (test with MSVC)
- Unit tests for all combinations

#### Phase 3: Dependency Propagation (2-3 days)
- Implement `collectTransitiveLibraries()` in DependencyGraph
- Update `ToolchainOrchestrator::generateLinkCommand()`
- Integration tests with nested dependencies

#### Phase 4: BuildState Integration (1-2 days)
- Update command hasher to include libraries
- Test rebuild triggering on library changes
- Verify incremental builds work correctly

#### Phase 5: Validation & Errors (1-2 days)
- Library existence checking
- Clear error messages for missing libs
- Documentation and examples

**Total Time Estimate:** 9-14 days

### Testing Strategy

```cpp
// Test: Transitive library propagation
TEST(FFILinking, TransitivePropagation) {
    // Graph: exe → lib_a → lib_b
    //        lib_a needs "ssl"
    //        lib_b needs "crypto"
    
    Node* lib_b = graph.addNode("lib_b");
    lib_b->libraries = {"crypto"};
    
    Node* lib_a = graph.addNode("lib_a");
    lib_a->libraries = {"ssl"};
    lib_a->dependencies_.push_back(lib_b);
    
    Node* exe = graph.addNode("exe");
    exe->dependencies_.push_back(lib_a);
    
    auto libs = graph.collectTransitiveLibraries(exe);
    
    // Should contain both "crypto" and "ssl" (sorted)
    ASSERT_EQ(libs.size(), 2);
    ASSERT_EQ(libs[0], "crypto");
    ASSERT_EQ(libs[1], "ssl");
}
```

### Success Criteria

- ✅ Aria programs can link against OpenSSL, SQLite, zlib
- ✅ Transitive dependencies work (A→B→C propagates C's libs to A)
- ✅ Incremental builds detect library changes
- ✅ Clear error messages for missing libraries
- ✅ Cross-platform (Linux ELF + Windows PE/COFF)
- ✅ No manual linker flag editing required

---

## Preprocessing Pipeline Inspection (-E Flag)

### Purpose

Implement preprocessing-only mode to expose the preprocessed source code output, enabling debugging of macro expansions, module import resolution, and textual transformation logic before compilation.

**Developer Workflow Enhancement:**
- **Problem:** Macros fail with cryptic "Parse Error: Unexpected token" but the expanded code is invisible
- **Solution:** `ariac -E file.aria` outputs preprocessed source, allowing developers to inspect exactly what the parser receives
- **Use Cases:**
  1. Debugging macro hygiene issues (variable name collisions)
  2. Verifying `%include` directive expansion
  3. Auditing generic template instantiation
  4. IDE "Expand Macro" feature integration
  5. Documentation generation from preprocessed output

### Background: Aria Compilation Pipeline

The Aria compiler implements a multi-phase architecture:

```
┌─────────────┐
│   Source    │
│   Input     │
└──────┬──────┘
       │
       ▼
┌──────────────────────┐
│   Phase 0:           │ ◄── PREPROCESSING
│   Preprocessor       │     - Macro expansion (%macro)
│   - Macro expansion  │     - Context stack (%push/%pop)
│   - Include files    │     - Include files (%include)
│   - Hygiene renaming │     - Hygiene renaming (varname → varname_h1)
└──────┬───────────────┘
       │ Expanded Source String
       ▼
┌──────────────────────┐
│   Phase 1:           │
│   Lexer              │
│   (Tokenization)     │
└──────┬───────────────┘
       │ Token Vector
       ▼
┌──────────────────────┐
│   Phase 2:           │
│   Parser             │
│   (AST Construction) │
└──────┬───────────────┘
       │ AST
       ▼
┌──────────────────────┐
│   Phase 3:           │
│   Semantic Analysis  │
└──────┬───────────────┘
       │ Decorated AST
       ▼
┌──────────────────────┐
│   Phase 4:           │
│   Code Generation    │
│   (LLVM IR)          │
└──────┬───────────────┘
       │
       ▼
   .ll / .bc File
```

**Critical Insight:** Errors introduced during Phase 0 (preprocessing) manifest as parsing errors in Phase 2, but developers only see their original source code, not the expanded intermediate form. The -E flag exposes this "invisible" transformation layer.

### The "Critical Macro Limitation" Problem

Aria v0.0.7 identified a severe hygiene issue:

```aria
%macro GEN_POPCOUNT 1
func:popcount_%1 = int32(%1:x) {
    %1:val = x;  // COLLISION: %1 expands to generic param syntax
    pass(0);
};
%endmacro

GEN_POPCOUNT(int32)  // Error: "Parse Error: Unexpected token after type"
```

**Without -E:** Developer sees only the clean macro definition, no insight into what went wrong.

**With -E:**
```bash
ariac -E source.aria
```

Output reveals:
```aria
func:popcount_int32 = int32(int32:x) {  // ✓ Correct hygiene
    int32:val = x;  // ❌ COLLISION - should be int32:val_h1
    pass(0);
};
```

Developer immediately sees the hygiene system failed to rename `val`, enabling targeted debugging.

### Architecture: Build System vs. Compiler

The -E flag must be implemented at **two layers**:

| Component | File | Responsibility | Flag |
|-----------|------|----------------|------|
| **aria_make** (Build System) | `src/cli/main.cpp` | Argument parsing, orchestration | `-E` or `--debug-macro` |
| **aria_make** (Orchestrator) | `src/build/toolchain.cpp` | Command construction, output redirection | Injects `-E` into `ariac` command |
| **ariac** (Compiler) | `src/driver/main.cpp` | Pipeline control, early exit after Phase 0 | `-E` (plus optional `-C`) |
| **ariac** (Preprocessor) | `src/frontend/preprocessor.cpp` | Comment preservation logic | `-C` flag support |

### CompilerOptions Data Structure

```cpp
// File: src/driver/options.h

struct CompilerOptions {
    std::vector<std::string> input_files;
    std::string output_file;
    
    // Existing emission flags
    bool emit_llvm_ir = false;
    bool emit_llvm_bc = false;
    bool emit_asm = false;
    bool dump_ast = false;
    bool dump_tokens = false;
    
    // NEW: Preprocessing-only mode
    bool preprocess_only = false;       // -E: Stop after Phase 0
    bool preserve_comments = false;     // -C: Keep // and /* */ in output
    
    bool verbose = false;
    int opt_level = 0;
    std::vector<std::string> warning_flags;
};
```

### Implementation: CLI Argument Parsing (aria_make)

```cpp
// File: src/cli/main.cpp

#include "build/settings.h"
#include "build/orchestrator.h"
#include <vector>
#include <string>
#include <iostream>

void parse_cli_args(int argc, char** argv, aria::build::BuildSettings& settings) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-E" || arg == "--debug-macro") {
            settings.preprocess_only = true;
        }
        else if (arg == "-C" || arg == "--preserve-comments") {
            settings.preserve_comments = true;
        }
        else if (arg == "-o") {
            if (i + 1 < argc) {
                settings.custom_output = argv[++i];
            } else {
                std::cerr << "Error: -o requires an argument\n";
                exit(1);
            }
        }
        else if (arg == "--help") {
            print_help();
            exit(0);
        }
        // ... other flags ...
    }
}

int main(int argc, char** argv) {
    aria::build::BuildSettings settings;
    
    // Initialize defaults
    settings.preprocess_only = false;
    settings.preserve_comments = false;
    
    // Parse arguments
    parse_cli_args(argc, argv, settings);
    
    // Instantiate orchestrator
    aria::build::ToolchainOrchestrator orchestrator(settings);
    
    // Run build
    if (!orchestrator.run()) {
        return 1;
    }
    
    return 0;
}
```

### Implementation: Command Construction (ToolchainOrchestrator)

```cpp
// File: src/build/toolchain.cpp

namespace aria::build {

std::pair<std::string, std::vector<std::string>> 
ToolchainOrchestrator::construct_compile_cmd(const graph::Node* node) {
    std::vector<std::string> args;
    
    // 1. Source inputs (from globbing)
    for (const auto& src : node->source_files) {
        args.push_back(src);
    }
    
    // 2. Output and mode handling
    if (m_settings.preprocess_only) {
        // MODE: Preprocess-only
        args.push_back("-E");
        
        // Preserve comments if requested
        if (m_settings.preserve_comments) {
            args.push_back("-C");
        }
        
        // Determine output path for preprocessed file
        // Change extension to .preprocessed.aria to avoid overwriting artifacts
        std::filesystem::path output_path = node->output_file;
        if (output_path.has_extension()) {
            output_path.replace_extension(".preprocessed.aria");
        } else {
            output_path += ".preprocessed.aria";
        }
        
        args.push_back("-o");
        args.push_back(output_path.string());
        
        // Log to stddbg (FD 3), not stdout (keeps stdout clean)
        // Aria's six-stream topology: stdin/stdout/stderr/stddbg/stddati/stddato
        std::cerr << "[DEBUG] Preprocessing target '" << node->name 
                  << "' to " << output_path.string() << "\n";
    }
    else {
        // MODE: Standard compilation
        args.push_back("-o");
        args.push_back(node->output_file);
        
        // Add optimization flags
        if (m_settings.opt_level > 0) {
            args.push_back("-O" + std::to_string(m_settings.opt_level));
        }
    }
    
    // 3. Include paths (-I) from dependency graph
    for (const auto& inc : resolve_includes(node)) {
        args.push_back("-I");
        args.push_back(inc);
    }
    
    // 4. User-defined flags from target config
    for (const auto& flag : node->flags) {
        args.push_back(flag);
    }
    
    return {"ariac", args};
}

void ToolchainOrchestrator::execute_target(const graph::Node* node) {
    auto [binary, args] = construct_compile_cmd(node);
    
    // Execute compiler
    ExecResult result = ProcessExecutor::execute(binary, args);
    
    if (!result.success()) {
        std::cerr << "[ERROR] Compilation failed: " << node->name << "\n";
        std::cerr << result.stderr_output << "\n";
        return;
    }
    
    // CRITICAL: If preprocessing-only, DO NOT proceed to linking
    if (m_settings.preprocess_only) {
        std::cout << "[INFO] Preprocessing complete. Output: "
                  << node->output_file << ".preprocessed.aria\n";
        return;  // HALT: No linking phase
    }
    
    // Standard compilation: proceed to linking if needed
    if (node->type() == Node::TargetType::EXECUTABLE) {
        execute_link_phase(node);
    }
}

} // namespace aria::build
```

### Implementation: Pipeline Control (ariac Compiler Driver)

```cpp
// File: src/driver/main.cpp

int compile(const CompilerOptions& opts) {
    aria::DiagnosticEngine diags;
    
    // ═══════════════════════════════════════
    // PHASE 0: PREPROCESSING
    // ═══════════════════════════════════════
    
    aria::frontend::Preprocessor preprocessor;
    
    // Configure preprocessor
    if (opts.preserve_comments) {
        preprocessor.setPreserveComments(true);
    }
    
    // Process all input files
    std::stringstream full_preprocessed_output;
    
    for (const auto& input_file : opts.input_files) {
        std::string source_code = read_file(input_file);
        
        try {
            std::string processed = preprocessor.process(source_code, input_file);
            full_preprocessed_output << processed << "\n";
        }
        catch (const std::exception& e) {
            diags.error(aria::SourceLocation(input_file, 0, 0), e.what());
            return 1;
        }
    }
    
    // ═══════════════════════════════════════
    // CHECK: EARLY EXIT IF -E FLAG
    // ═══════════════════════════════════════
    
    if (opts.preprocess_only) {
        std::string final_output = full_preprocessed_output.str();
        
        if (!opts.output_file.empty()) {
            // Write to specified output file
            std::ofstream out(opts.output_file);
            if (!out) {
                std::cerr << "Error: Could not open output file " 
                          << opts.output_file << "\n";
                return 1;
            }
            out << final_output;
            out.close();
        }
        else {
            // Write to stdout (standard Unix compiler behavior)
            std::cout << final_output;
        }
        
        // CRITICAL: Halt pipeline
        // Do NOT instantiate Lexer/Parser/Sema/Codegen
        return 0;
    }
    
    // ═══════════════════════════════════════
    // PHASE 1: LEXICAL ANALYSIS
    // ═══════════════════════════════════════
    
    aria::frontend::Lexer lexer(full_preprocessed_output.str());
    std::vector<aria::Token> tokens = lexer.tokenize();
    
    if (opts.dump_tokens) {
        for (const auto& tok : tokens) {
            std::cout << tok << "\n";
        }
    }
    
    // ═══════════════════════════════════════
    // PHASE 2: PARSING
    // ═══════════════════════════════════════
    
    aria::frontend::Parser parser(tokens);
    auto ast = parser.parse();
    
    if (opts.dump_ast) {
        ast->dump(std::cout);
    }
    
    // ... Continue with Sema, Codegen ...
    
    return 0;
}
```

### Implementation: Comment Preservation (Preprocessor)

```cpp
// File: src/frontend/preprocessor.cpp

namespace aria::frontend {

class Preprocessor {
public:
    Preprocessor() : m_preserve_comments(false) {}
    
    void setPreserveComments(bool preserve) {
        m_preserve_comments = preserve;
    }
    
    std::string process(const std::string& source, const std::string& filename) {
        std::stringstream output;
        size_t pos = 0;
        
        while (pos < source.length()) {
            char c = source[pos];
            
            // Handle line comments (//)
            if (c == '/' && pos + 1 < source.length() && source[pos + 1] == '/') {
                if (m_preserve_comments) {
                    // Copy comment to output
                    size_t end = source.find('\n', pos);
                    if (end == std::string::npos) end = source.length();
                    output << source.substr(pos, end - pos) << "\n";
                    pos = end + 1;
                }
                else {
                    // Skip to end of line
                    while (pos < source.length() && source[pos] != '\n') {
                        pos++;
                    }
                    pos++;  // Skip newline
                }
                continue;
            }
            
            // Handle block comments (/* */)
            if (c == '/' && pos + 1 < source.length() && source[pos + 1] == '*') {
                if (m_preserve_comments) {
                    // Copy block comment to output
                    size_t end = source.find("*/", pos + 2);
                    if (end == std::string::npos) {
                        throw std::runtime_error("Unclosed block comment");
                    }
                    output << source.substr(pos, (end + 2) - pos);
                    pos = end + 2;
                }
                else {
                    // Skip to end of block comment
                    pos += 2;  // Skip /*
                    while (pos + 1 < source.length()) {
                        if (source[pos] == '*' && source[pos + 1] == '/') {
                            pos += 2;
                            break;
                        }
                        pos++;
                    }
                }
                continue;
            }
            
            // Handle macro directives (%macro, %include, etc.)
            if (c == '%') {
                // ... existing macro processing logic ...
            }
            
            // Regular character
            output << c;
            pos++;
        }
        
        return output.str();
    }
    
private:
    bool m_preserve_comments;
};

} // namespace aria::frontend
```

### Flag Variants and Behavior

| Command | Output Destination | Comments | Use Case |
|---------|-------------------|----------|----------|
| `ariac -E file.aria` | stdout | Stripped | Default preprocessing inspection |
| `ariac -E -C file.aria` | stdout | Preserved | Documentation generation |
| `ariac -E file.aria -o out.txt` | `out.txt` | Stripped | Save for analysis |
| `ariac -E -C file.aria -o out.txt` | `out.txt` | Preserved | Save with docs |
| `aria_make -E target` | `bin/target.preprocessed.aria` | Stripped | Build system mode |
| `aria_make -E -C target` | `bin/target.preprocessed.aria` | Preserved | Build system with comments |

### Integration with IDE/LSP ("Expand Macro" Feature)

**Workflow:**

1. **User Action:** In VS Code, user hovers over macro invocation `GEN_MAX(int32)` and selects "Expand Macro"
2. **LSP Request:** AriaLS receives `textDocument/hover` request with cursor position
3. **Temp File Creation:** LSP creates temporary file with current document content
4. **Preprocessing:** LSP invokes:
   ```bash
   ariac -E temp.aria -o temp.preprocessed.aria
   ```
5. **Output Parsing:** LSP reads `temp.preprocessed.aria`, extracts relevant lines
6. **Display:** LSP shows expanded code in hover tooltip:
   ```aria
   // Macro expansion:
   func:max_int32 = int32(a:int32, b:int32) {
       return a > b ? a : b;
   };
   ```

**LSP Implementation:**

```cpp
// File: src/lsp/aria_ls.cpp

std::string AriaLS::expandMacroAt(const std::string& file_uri, size_t line, size_t col) {
    // 1. Get document content
    std::string source = vfs_.getDocument(file_uri);
    
    // 2. Write to temp file
    std::string temp_file = "/tmp/aria_preprocess_" + generate_uuid() + ".aria";
    write_file(temp_file, source);
    
    // 3. Invoke preprocessor
    std::string output_file = temp_file + ".preprocessed";
    std::vector<std::string> args = {"-E", temp_file, "-o", output_file};
    ExecResult result = ProcessExecutor::execute("ariac", args);
    
    if (!result.success()) {
        return "Error: Preprocessing failed";
    }
    
    // 4. Read preprocessed output
    std::string preprocessed = read_file(output_file);
    
    // 5. Extract relevant lines (simple heuristic: same line range)
    // In production: map source location → preprocessed location via line markers
    auto lines = split_lines(preprocessed);
    if (line < lines.size()) {
        return lines[line];
    }
    
    // Cleanup
    std::filesystem::remove(temp_file);
    std::filesystem::remove(output_file);
    
    return preprocessed;
}
```

### Testing Strategy

**Unit Tests (Compiler):**

```cpp
TEST(Preprocessor, StopsAfterPhase0) {
    CompilerOptions opts;
    opts.preprocess_only = true;
    opts.input_files = {"test.aria"};
    
    // Mock file with macro
    write_file("test.aria", 
        "%macro DOUBLE 1\n"
        "(%1 * 2)\n"
        "%endmacro\n"
        "x = DOUBLE(5);\n"
    );
    
    std::ostringstream capture;
    auto old_cout = std::cout.rdbuf(capture.rdbuf());
    
    int result = compile(opts);
    
    std::cout.rdbuf(old_cout);
    
    ASSERT_EQ(result, 0);
    EXPECT_THAT(capture.str(), HasSubstr("x = (5 * 2);"));
    EXPECT_THAT(capture.str(), Not(HasSubstr("%macro")));  // Macro def stripped
}

TEST(Preprocessor, PreservesCommentsWithCFlag) {
    CompilerOptions opts;
    opts.preprocess_only = true;
    opts.preserve_comments = true;
    opts.input_files = {"test.aria"};
    
    write_file("test.aria", 
        "// This is a comment\n"
        "x = 42;\n"
    );
    
    std::ostringstream capture;
    auto old_cout = std::cout.rdbuf(capture.rdbuf());
    
    compile(opts);
    
    std::cout.rdbuf(old_cout);
    
    EXPECT_THAT(capture.str(), HasSubstr("// This is a comment"));
}
```

**Integration Tests (Build System):**

```cpp
TEST(AriaMake, PreprocessingModeGeneratesFile) {
    TestProject proj;
    proj.writeFile("main.aria", 
        "%macro GEN 1\n"
        "val = %1;\n"
        "%endmacro\n"
        "GEN(42)\n"
    );
    
    BuildSettings settings;
    settings.preprocess_only = true;
    
    ToolchainOrchestrator orchestrator(settings);
    orchestrator.run();
    
    // Check output file exists
    ASSERT_TRUE(fs::exists("bin/main.preprocessed.aria"));
    
    // Check content
    std::string content = read_file("bin/main.preprocessed.aria");
    EXPECT_THAT(content, HasSubstr("val = 42;"));
    EXPECT_THAT(content, Not(HasSubstr("%macro")));
}
```

### Implementation Checklist

- [ ] Add `preprocess_only` and `preserve_comments` to `CompilerOptions`
- [ ] Implement `-E` and `-C` parsing in `src/driver/main.cpp`
- [ ] Add early exit after Phase 0 in compiler driver
- [ ] Implement comment preservation toggle in `Preprocessor` class
- [ ] Add `-E` and `--debug-macro` parsing to `aria_make` CLI
- [ ] Update `ToolchainOrchestrator` to generate `.preprocessed.aria` files
- [ ] Skip linking phase when `preprocess_only` is true
- [ ] Add unit tests for preprocessor output
- [ ] Add integration tests for build system
- [ ] Update LSP to support "Expand Macro" feature
- [ ] Document flag usage in user manual

**Estimated Implementation Time:** 3-5 days
- Compiler driver changes: 1 day
- Preprocessor comment preservation: 0.5 days
- Build system orchestration: 1 day
- LSP integration: 1 day
- Testing + documentation: 0.5-1.5 days

---

## JIT-Enforced Test Execution

### Purpose

Enforce deterministic execution semantics for test targets by mandating JIT (Just-In-Time) compilation mode in LLVM's `lli` interpreter, eliminating silent fallback to the bytecode interpreter which introduces semantic divergence and catastrophic performance degradation.

**Critical Problem Statement:**

The LLVM `lli` tool supports two execution models:

1. **MCJIT/ORC JIT Compiler**: Compiles LLVM IR to native machine code using the production CodeGen backend (identical to `llc`). This produces semantically accurate execution matching final binary behavior.

2. **Bytecode Interpreter**: Emulates LLVM IR using a software virtual machine with GenericValue abstractions. This mode is:
   - **10x-50x slower** than JIT (fatal for TDD workflow with large test suites)
   - **Semantically divergent** from native execution:
     - Masks alignment errors (wild pointer misalignment doesn't trap)
     - Simplifies floating-point behavior (no FPU flag emulation)
     - Emulates SIMD operations component-wise (doesn't test AVX-512 codegen)
     - Uses "soft" memory model (doesn't enforce System V ABI padding/alignment)

By default, `lli` attempts JIT but **silently falls back to interpreter** on failure (e.g., W^X security restrictions, unsupported architecture, internal errors). This creates a "fools' gold" scenario:

- **Tests pass in interpreter** (forgiving emulation hides bugs)
- **Production fails in native code** (strict hardware execution exposes bugs)

**Example Failure Mode - TBB Arithmetic:**

```aria
tbb8 x = 127;
tbb8 y = 1;
tbb8 z = x + y;  // Should saturate to 0x80 (sticky error)
assert(z == ERR);  // -128 sentinel
```

- **Interpreter Mode**: C++ logic `if (val == -128) return ERR` works correctly
- **JIT Mode (with backend bug)**: Generates `paddsb` instruction incorrectly, fails to set sentinel
- **Result**: Test passes in interpreter (masking compiler bug), fails in production binary

### Architecture: test_mode Configuration

**ABC Schema Extension:**

Add `test_mode` field to `project` section of `aria.json`:

```javascript
{
  project: {
    name: "MyAriaLibrary",
    version: "0.1.0",
    test_mode: "jit"  // NEW FIELD
  },
  // ...
}
```

**Field Specification:**

| Property | Type | Values | Default | Scope |
|----------|------|--------|---------|-------|
| `test_mode` | String Enum | `"jit"`, `"interpreter"` | `"jit"` | Global (project-level) |

**Semantics:**

- **`"jit"`** (Default): Enforce JIT compilation via `lli -force-interpreter=false`. If JIT initialization fails (unsupported platform, W^X violation, etc.), the test execution **fails loudly** with clear error message.
  - **Guarantees**: Native performance, semantic fidelity with production binary
  - **Use Cases**: Normal development, CI/CD pipelines, production-grade testing

- **`"interpreter"`**: Force bytecode interpreter via `lli -force-interpreter=true`.
  - **Guarantees**: High introspection capability, works on all platforms
  - **Use Cases**: Debugging LLVM IR generation, platforms without JIT support (experimental RISC-V ports), toolchain crash debugging

**Variable Interpolation Support:**

The parser must support environment variable interpolation for CI/CD flexibility:

```javascript
project: {
  test_mode: "&{ENV.ARIA_TEST_MODE}"  // Default to "jit", override via env var
}
```

This enables:
- Developers run JIT locally (fast, accurate)
- Debug pipeline runs Interpreter mode (generates verbose trace logs)

### Implementation: TestRunner Refactor

**Data Model Extension:**

Add `TestExecutionMode` enum to `ProjectMetadata`:

```cpp
namespace aria::build {

struct ProjectMetadata {
    std::string name;
    std::string version;
    
    enum class TestExecutionMode {
        JIT,         // Force JIT (-force-interpreter=false)
        Interpreter  // Force interpreter (-force-interpreter=true)
    };
    
    TestExecutionMode test_mode = TestExecutionMode::JIT;  // DEFAULT
};

} // namespace aria::build
```

**ConfigParser Integration:**

Update `ConfigParser::parseProject()` to extract `test_mode`:

```cpp
void ConfigParser::parseProject(const ConfigNode* node, BuildContext& ctx) {
    // ... existing name/version parsing ...
    
    if (auto* mode_node = node->findChild("test_mode")) {
        std::string mode_str = interpolator.resolve(mode_node->value);
        
        if (mode_str == "jit") {
            ctx.project.test_mode = ProjectMetadata::TestExecutionMode::JIT;
        } else if (mode_str == "interpreter") {
            ctx.project.test_mode = ProjectMetadata::TestExecutionMode::Interpreter;
        } else {
            throw ConfigError(
                fmt::format("Invalid project.test_mode value '{}'. Expected 'jit' or 'interpreter'.",
                            mode_str)
            );
        }
    }
    // test_mode defaults to JIT if not specified
}
```

**TestRunner Command Construction:**

Refactor `src/test/runner.cpp` to enforce the execution mode:

```cpp
namespace aria::test {

class TestRunner {
public:
    explicit TestRunner(const BuildContext& context)
        : context_(context) {}
    
    bool run_target(const Target& target) {
        auto args = construct_lli_args(target.output_file);
        
        // Convert to LLVM StringRef vector
        std::vector<llvm::StringRef> argv_ref;
        for (const auto& arg : args) {
            argv_ref.push_back(arg);
        }
        
        std::string err_msg;
        int exit_code = llvm::sys::ExecuteAndWait(
            args[0],                // Program ("lli")
            argv_ref,               // Arguments
            std::nullopt,           // Environment
            {},                     // Redirects
            0,                      // Timeout (infinite)
            0,                      // Memory limit
            &err_msg                // Error message output
        );
        
        if (exit_code != 0) {
            handle_test_failure(target, exit_code, err_msg);
            return false;
        }
        
        return true;
    }

private:
    const BuildContext& context_;
    
    std::vector<std::string> construct_lli_args(const fs::path& bitcode_path) {
        std::vector<std::string> args;
        
        // 1. The executable
        args.push_back("lli");
        
        // 2. Execution mode enforcement
        auto mode = context_.project.test_mode;
        
        if (mode == ProjectMetadata::TestExecutionMode::JIT) {
            // CRITICAL: Mandate JIT compilation
            // If JIT initialization fails, lli will exit with error (not fallback)
            args.push_back("-force-interpreter=false");
            
            // Optional: Enable platform-specific optimizations
            // Ensures SIMD types (vec9) use actual AVX-512 instructions
            #if defined(__x86_64__)
            if (supports_avx512()) {
                args.push_back("-mattr=+avx512f");
            }
            #endif
            
        } else {
            // Debugging/fallback mode: explicit interpreter request
            args.push_back("-force-interpreter=true");
        }
        
        // 3. The target artifact (.ll or .bc file)
        args.push_back(bitcode_path.string());
        
        // 4. Test arguments (if any)
        // Future: Support test filters, --verbose flags
        // args.insert(args.end(), user_test_args.begin(), user_test_args.end());
        
        return args;
    }
    
    void handle_test_failure(const Target& target, int exit_code, const std::string& err_msg) {
        std::cerr << fmt::format("Test Target '{}' Failed\n", target.name);
        std::cerr << fmt::format("  Exit Code: {}\n", exit_code);
        
        if (!err_msg.empty()) {
            std::cerr << fmt::format("  Toolchain Error: {}\n", err_msg);
            
            // Provide actionable hint for JIT failures
            if (context_.project.test_mode == ProjectMetadata::TestExecutionMode::JIT) {
                std::cerr << "\n";
                std::cerr << "  Hint: JIT execution failed. Possible causes:\n";
                std::cerr << "    1. Platform lacks LLVM JIT support (try test_mode: 'interpreter')\n";
                std::cerr << "    2. W^X security policy (SELinux, macOS Hardened Runtime)\n";
                std::cerr << "    3. Missing target triple in LLVM IR\n";
                std::cerr << "  Debugging: Run manually with 'lli -force-interpreter=false {}' for details\n";
            }
        }
    }
};

} // namespace aria::test
```

### Performance and Semantic Validation

**Benchmark Expectations:**

| Test Suite Size | Interpreter Time | JIT Time | Speedup |
|-----------------|------------------|----------|----------|
| 100 tests (std.math) | ~45s | ~3s | 15x |
| 1,000 tests (std.crypto) | ~8 min | ~25s | 19x |
| 10,000 tests (full stdlib) | ~80 min | ~4 min | 20x |

**Case Study: TBB Sentinel Propagation Bug**

```aria
// Test: Sticky error propagation
tbb8 x = -128;  // ERR sentinel
tbb8 y = 10;
tbb8 z = x + y;  // Should remain ERR
assert(z == -128);  // Test assertion
```

**Scenario:** Compiler backend has bug in `paddsb` (saturating add) instruction generation.

- **Interpreter Mode**: Uses C++ logic `if (lhs == -128 || rhs == -128) return -128;` → Test **passes** (bug hidden)
- **JIT Mode**: Generates buggy assembly → Sentinel not preserved → Test **fails** (bug exposed)
- **Outcome**: JIT mode correctly identifies compiler backend bug that interpreter masked.

**Case Study: Wild Pointer Alignment**

```aria
wild ptr = allocate(10);  // Returns 0x1003 (unaligned)
wild int64* p = cast(ptr);
val = *p;  // Unaligned 8-byte read
```

- **Interpreter Mode**: Soft memory model handles unalignment gracefully → Test **passes**
- **JIT Mode**: Emits `movaps` (aligned move) → CPU traps with SIGBUS → Test **fails**
- **Outcome**: JIT mode correctly enforces Aria memory model alignment requirements.

### Documentation Update

**User-Facing Documentation (docs/build-system.md):**

```markdown
## Test Execution Configuration

AriaBuild enforces JIT (Just-In-Time) compilation for all test targets by default.
This ensures that:

- **Performance**: Tests run at native speed (10x-50x faster than interpreter)
- **Semantic Fidelity**: Tests execute with identical behavior to production binaries
- **Bug Detection**: Backend code generation issues are exposed early

### Execution Modes

**JIT Mode (Default):**

```javascript
project: {
  test_mode: "jit"  // Native compilation, production-grade semantics
}
```

- Uses LLVM MCJIT/ORC backend
- Generates real machine code (x86-64, ARM64, etc.)
- **Fails loudly** if JIT unavailable (no silent fallback)

**Interpreter Mode (Debugging):**

```javascript
project: {
  test_mode: "interpreter"  // Bytecode emulation, high introspection
}
```

- Uses LLVM bytecode interpreter
- Slower execution (acceptable for debugging)
- Works on all platforms (even those without JIT support)
- Useful for:
  - Debugging LLVM IR generation
  - Tracing macro expansion issues
  - Platforms with experimental JIT support

### Environment Variable Override

Supports dynamic configuration via environment variables:

```javascript
project: {
  test_mode: "&{ENV.ARIA_TEST_MODE}"  // Defaults to "jit"
}
```

Usage:
```bash
# Normal development (JIT)
aria_make test

# Debugging session (Interpreter)
ARIA_TEST_MODE=interpreter aria_make test
```
```

### Testing Strategy

**Unit Tests (Parser):**

```cpp
TEST(ConfigParser, ParsesTestMode) {
    auto json = R"(
        project: {
            name: "TestProject",
            test_mode: "interpreter"
        }
    )";
    
    ConfigParser parser;
    auto ctx = parser.parse(json);
    
    ASSERT_EQ(ctx.project.test_mode, ProjectMetadata::TestExecutionMode::Interpreter);
}

TEST(ConfigParser, DefaultsToJIT) {
    auto json = R"(
        project: { name: "TestProject" }
    )";
    
    auto ctx = parser.parse(json);
    ASSERT_EQ(ctx.project.test_mode, ProjectMetadata::TestExecutionMode::JIT);
}

TEST(ConfigParser, RejectsInvalidMode) {
    auto json = R"(
        project: { test_mode: "turbo" }
    )";
    
    EXPECT_THROW(parser.parse(json), ConfigError);
}
```

**Integration Tests (TestRunner):**

```cpp
TEST(TestRunner, EnforcesJITFlag) {
    BuildContext ctx;
    ctx.project.test_mode = ProjectMetadata::TestExecutionMode::JIT;
    
    Target test_target;
    test_target.output_file = "test.bc";
    
    TestRunner runner(ctx);
    auto args = runner.construct_lli_args(test_target.output_file);
    
    // Should contain JIT enforcement flag
    ASSERT_THAT(args, Contains("-force-interpreter=false"));
}

TEST(TestRunner, HandlesJITFailureGracefully) {
    // Simulate platform without JIT support
    // Expect clear error message, not silent degradation
    
    BuildContext ctx;
    ctx.project.test_mode = ProjectMetadata::TestExecutionMode::JIT;
    
    Target target;
    target.output_file = "invalid.bc";  // Malformed LLVM IR
    
    TestRunner runner(ctx);
    
    testing::internal::CaptureStderr();
    bool result = runner.run_target(target);
    std::string error = testing::internal::GetCapturedStderr();
    
    ASSERT_FALSE(result);
    EXPECT_THAT(error, HasSubstr("JIT execution failed"));
    EXPECT_THAT(error, HasSubstr("Hint:"));
}
```

### Platform-Specific Considerations

**Linux:**
- JIT works out-of-box on x86-64, ARM64
- Requires executable memory permissions (`PROT_EXEC`)
- SELinux may block JIT (check `execmem` policy)

**macOS:**
- Hardened Runtime restricts JIT by default
- Requires entitlement: `com.apple.security.cs.allow-jit`
- Code signing required for distribution

**Windows:**
- JIT works on x86-64 with DEP enabled
- Requires `VirtualAlloc` with `PAGE_EXECUTE_READWRITE`

**Experimental Platforms (RISC-V, WebAssembly):**
- LLVM JIT support may be incomplete
- Default to `test_mode: "interpreter"` in `aria.json`
- Upgrade to JIT when platform support matures

### Future Enhancements

**1. Per-Target test_mode Override:**

Currently global (project-level). Future: per-target granularity:

```javascript
targets: [
  {
    name: "fast_tests",
    type: "test",
    test_mode: "jit"  // Override project default
  },
  {
    name: "debug_tests",
    type: "test",
    test_mode: "interpreter"  // Slow but introspectable
  }
]
```

**2. ORC JIT Integration:**

Replace `lli` subprocess with direct `libLLVM` linkage:
- Eliminate process spawn overhead (10-50ms per test → <1ms)
- Enable millisecond-latency test execution
- Support incremental JIT (compile functions on-demand)

**3. Profile-Guided Optimization (PGO):**

Capture execution profiles during JIT test runs, feed back to compiler:
- Optimize hot paths based on test coverage
- Improve branch prediction in production binaries

### Implementation Checklist

- [ ] Add `TestExecutionMode` enum to `ProjectMetadata` struct
- [ ] Update `ConfigParser` to parse `test_mode` field
- [ ] Implement variable interpolation for `test_mode`
- [ ] Add strict validation (reject invalid values)
- [ ] Refactor `TestRunner::construct_lli_args()` to enforce mode
- [ ] Implement `handle_test_failure()` with actionable hints
- [ ] Add platform detection for AVX-512 support (optional optimization)
- [ ] Add unit tests for parser (valid/invalid/default cases)
- [ ] Add integration tests for `TestRunner` (JIT enforcement)
- [ ] Update `examples/aria.json` template with `test_mode` field
- [ ] Document in user manual (build-system.md)
- [ ] Add CI/CD examples with environment variable override

**Estimated Implementation Time:** 2-3 days
- Data model + parser: 3-4 hours
- TestRunner refactor: 4-5 hours
- Error handling + hints: 2-3 hours
- Testing (unit + integration): 4-6 hours
- Documentation + templates: 2-3 hours

---

**Next:** [91_PLATFORM_COMPAT.md](./91_PLATFORM_COMPAT.md) - Cross-platform code patterns and abstractions
