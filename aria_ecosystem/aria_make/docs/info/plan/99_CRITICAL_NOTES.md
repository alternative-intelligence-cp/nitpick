# Critical Implementation Notes

⚠️ **READ THIS BEFORE IMPLEMENTING** ⚠️

This document contains critical gotchas, common pitfalls, and non-obvious requirements discovered during research analysis.

---

## 🔴 CRITICAL: Pipe Deadlock Prevention

**Problem:** When spawning a child process with piped stdout/stderr, if the child writes more data than the pipe buffer size (typically 64KB), it will BLOCK waiting for the parent to drain the pipe. If the parent is also blocked waiting for the child to exit (`waitpid()`), you have a **deadlock**.

**Symptoms:**
- Process hangs indefinitely
- No output captured
- CPU idle (both processes sleeping)

**Solution:** Always spawn dedicated drain threads

```cpp
ExecResult execute(cmd, args) {
    // Create pipes
    int stdout_pipe[2], stderr_pipe[2];
    pipe(stdout_pipe);
    pipe(stderr_pipe);
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child: redirect stdout/stderr
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        execvp(cmd, args);
    }
    
    // ⚠️ CRITICAL: Close write ends in parent!
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    
    // ✅ Spawn drain threads (non-blocking)
    std::string out, err;
    std::thread out_thread([&]() { drainPipe(stdout_pipe[0], out); });
    std::thread err_thread([&]() { drainPipe(stderr_pipe[0], err); });
    
    // Now safe to wait
    int status;
    waitpid(pid, &status, 0);
    
    // Join threads
    out_thread.join();
    err_thread.join();
    
    return {WEXITSTATUS(status), out, err};
}
```

**Why this works:**
- Drain threads continuously read from pipes → never full
- Parent can safely wait without blocking child

**Testing:** Compile a program that prints 1MB of output and verify it doesn't hang.

---

## 🔴 CRITICAL: Close Parent's Write Pipe Ends

**Problem:** If you forget to close the write ends of pipes in the parent process, the drain threads will never detect EOF and will hang forever.

**Why:** The pipe remains "open for writing" from the parent's perspective, so `read()` waits indefinitely even though the child has exited.

```cpp
// ❌ WRONG: Forgot to close write ends
pipe(stdout_pipe);
fork();
// ... child setup ...
// Parent continues without closing stdout_pipe[1]
std::thread drain([&]() {
    char buf[4096];
    while (read(stdout_pipe[0], buf, sizeof(buf)) > 0) { ... }  // ← Hangs here!
});
```

```cpp
// ✅ CORRECT: Always close write ends
close(stdout_pipe[1]);  // MUST do this!
close(stderr_pipe[1]);
```

**Testing:** Comment out the `close()` calls and verify the test hangs (then fix it).

---

## 🔴 CRITICAL: Always Sort Filesystem Results

**Problem:** `std::filesystem::directory_iterator` returns entries in **filesystem order**, which is non-deterministic (depends on inode table, disk layout, etc.). This breaks reproducible builds.

**Example:**
```bash
# Run 1
$ aria_make
Compiling: b.aria, a.aria, c.aria

# Run 2 (same source)
$ aria_make
Compiling: a.aria, c.aria, b.aria  # Different order!
```

**Solution:** Always sort results

```cpp
std::vector<fs::path> expand(const std::string& pattern) {
    std::vector<fs::path> results;
    
    for (auto& entry : fs::recursive_directory_iterator(root)) {
        if (matches(entry.path(), pattern)) {
            results.push_back(entry.path());
        }
    }
    
    // ✅ CRITICAL: Sort before returning
    std::sort(results.begin(), results.end());
    
    return results;
}
```

**Why this matters:** Build reproducibility, cache keys, dependency order.

---

## 🔴 CRITICAL: Atomic In-Degree Updates

**Problem:** In parallel builds, multiple worker threads may complete dependencies simultaneously and decrement the same node's in-degree counter. Without atomics, this is a **data race**.

**Example:**
```
Thread 1: Finishes LibA → Decrements App.in_degree (read: 2, write: 1)
Thread 2: Finishes LibB → Decrements App.in_degree (read: 2, write: 1)
Result: in_degree = 1 (should be 0!)
```

**Solution:** Use `std::atomic<int>`

```cpp
class Node {
    std::atomic<int> in_degree_;  // ✅ Thread-safe
    
    int decrementInDegree() {
        return --in_degree_;  // Atomic decrement + return
    }
};
```

**Testing:** Build a graph with diamond dependency, run parallel build 100 times, verify correct scheduling.

---

## 🔴 CRITICAL: Normalize Paths Early

**Problem:** Windows uses `\` separator, Unix uses `/`. Mixing them breaks comparisons:

```cpp
"src\\main.aria" != "src/main.aria"  // String comparison fails!
```

**Solution:** Normalize immediately on input

```cpp
std::string normalizePath(const fs::path& path) {
    std::string s = path.string();
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

// Use normalized paths everywhere
auto path = normalizePath(fs::path(user_input));
```

**Why this matters:** Cache keys, dependency matching, exclusion patterns.

---

## ⚠️ Tri-Color Marking: Diamonds vs Cycles

**Problem:** Naive cycle detection marks ALL shared dependencies as cycles, rejecting valid diamond dependencies.

**Diamond (VALID):**
```
    App
   /   \
LibA   LibB
   \   /
   Core
```

**Cycle (INVALID):**
```
A → B → C → A
```

**Solution:** Use three colors

```cpp
enum Color { WHITE, GRAY, BLACK };

bool dfs(Node* u) {
    color[u] = GRAY;  // Entering
    
    for (auto v : u->dependencies) {
        if (color[v] == GRAY) {
            // Still in recursion → CYCLE!
            return true;
        }
        if (color[v] == WHITE) {
            if (dfs(v)) return true;
        }
        // BLACK: Already completed → safe (diamond)
    }
    
    color[u] = BLACK;  // Completed
    return false;
}
```

**Key insight:** GRAY means "in current path". BLACK means "safely visited before".

---

## ⚠️ Double Escaping in compile_commands.json

**Problem:** The `command` field must be both **shell-escaped** AND **JSON-escaped**.

**Example:**
```
Filename: src/file with spaces.aria
Shell command: ariac "src/file with spaces.aria" -o output.o
JSON string: "ariac \"src/file with spaces.aria\" -o output.o"
```

**Solution:** Escape twice

```cpp
std::string escapeShell(const std::string& arg) {
    if (arg.find(' ') != std::string::npos) {
        return "\"" + arg + "\"";
    }
    return arg;
}

std::string escapeJson(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            default: result += c;
        }
    }
    return result;
}

// Usage
std::string shell_cmd = "ariac " + escapeShell(filename) + " -o output.o";
std::string json_field = "\"command\": \"" + escapeJson(shell_cmd) + "\"";
```

---

## ⚠️ C++17 file_time_type is NOT Unix Time

**Problem:** `std::filesystem::file_time_type` is implementation-defined. On Windows (MSVC), it's based on Windows epoch (1601). On Unix (GCC/Clang), it's based on Unix epoch (1970).

**Symptom:** Timestamp comparisons work on one platform but not another.

**Solution:** Use `.time_since_epoch().count()` for raw ticks

```cpp
bool isNewer(const fs::path& a, const fs::path& b) {
    auto time_a = fs::last_write_time(a).time_since_epoch().count();
    auto time_b = fs::last_write_time(b).time_since_epoch().count();
    return time_a > time_b;
}
```

**Why this works:** Ticks are always monotonic, regardless of epoch.

---

## ⚠️ Glob Exclusions Must Prune Directories

**Problem:** Naive exclusion checks each file individually. This wastes time descending into `node_modules/` (10,000+ files).

**Bad:**
```cpp
for (auto& entry : fs::recursive_directory_iterator(root)) {
    if (!isExcluded(entry)) {  // ← Still descended into node_modules!
        results.push_back(entry);
    }
}
```

**Good:**
```cpp
for (auto it = fs::recursive_directory_iterator(root); it != end; ++it) {
    if (isExcluded(it->path())) {
        if (it->is_directory()) {
            it.disable_recursion_pending();  // ✅ Skip entire tree
        }
        continue;
    }
    results.push_back(*it);
}
```

**Performance impact:** 10,000 files → 10 files (1000x speedup).

---

## ⚠️ Cache Variable Resolution

**Problem:** A variable like `&{base}` might appear 50+ times in a config. Re-resolving it every time is wasteful.

**Example:**
```javascript
variables: {
    base: "/usr/local"
}

targets: [
    {flags: "-I&{base}/include"},   // Resolve 1
    {flags: "-I&{base}/include"},   // Resolve 2 (same!)
    {flags: "-L&{base}/lib"},       // Resolve 3 (same base!)
    // ... 47 more times ...
]
```

**Solution:** Memoization cache

```cpp
std::string resolveVariable(const std::string& key) {
    if (cache_.count(key)) {
        return cache_[key];  // ✅ Cached
    }
    
    std::string value = /* resolve */;
    cache_[key] = value;  // Store for next time
    return value;
}
```

**Performance:** 50 resolutions → 1 resolution + 49 cache hits.

---

## ⚠️ Anchor Point Optimization for Globs

**Problem:** Naive glob starts from project root and checks every file.

**Bad:**
```cpp
// Pattern: "src/core/**/*.aria"
// Starts at project root, checks:
//   build/
//   docs/
//   tests/
//   node_modules/
//   src/
//     core/  ← Finally found it!
```

**Good:**
```cpp
// Extract anchor: "src/core" (deepest static path)
// Start directly at src/core/
fs::path anchor = findAnchor("src/core/**/*.aria");  // → "src/core"
for (auto& entry : fs::recursive_directory_iterator(anchor)) {
    // Only checks src/core/ tree
}
```

**Performance:** O(entire repo) → O(relevant subtree).

---

## ⚠️ Panic Mode Must Synchronize

**Problem:** After a parse error, if you don't skip to a sync point, you get cascading errors.

**Example:**
```javascript
project: {
    name "MyApp"  // ← ERROR: Missing colon
    version: "1.0"
}
```

**Bad:**
```
Error: Expected ':' after 'name'
Error: Unexpected STRING "MyApp"
Error: Unexpected IDENTIFIER 'version'
Error: Unexpected STRING "1.0"
```

**Good:**
```
Error: Expected ':' after 'name' at line 2
(Synchronized at next member)
```

**Solution:**
```cpp
void synchronize() {
    while (current_token_.type != EOF) {
        switch (current_token_.type) {
            case COMMA:
            case RIGHT_BRACE:
            case RIGHT_BRACKET:
                return;  // Found sync point
            default:
                advance();
        }
    }
}
```

---

## ⚠️ Environment Variables Are Platform-Specific

**POSIX:**
```cpp
const char* val = std::getenv("HOME");
```

**Windows:**
```cpp
char* buf = nullptr;
size_t sz = 0;
_dupenv_s(&buf, &sz, "USERPROFILE");  // Not HOME!
if (buf) {
    std::string result(buf);
    free(buf);  // Must free!
    return result;
}
```

**Portable wrapper required!**

---

## ⚠️ Hidden Files Differ by Platform

**Unix:** Filename starts with `.`
```cpp
bool isHidden(const fs::path& path) {
    return path.filename().string()[0] == '.';
}
```

**Windows:** Check file attributes
```cpp
#include <windows.h>
bool isHidden(const fs::path& path) {
    DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES 
        && (attrs & FILE_ATTRIBUTE_HIDDEN);
}
```

**Portable implementation required!**

---

## ⚠️ Command Signature Hashing

**Why needed:** Timestamp checking alone misses flag changes:

```javascript
// Build 1
{flags: "-O0"}  // Debug build

// Build 2
{flags: "-O3"}  // Release build - source unchanged!
```

**Without command hashing:** No rebuild (timestamps unchanged).  
**With command hashing:** Command changed → full rebuild.

**Implementation:**
```cpp
uint64_t hashCommand(const std::string& cmd) {
    // FNV-1a 64-bit
    uint64_t hash = 14695981039346656037ULL;
    for (char c : cmd) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}
```

---

## ⚠️ Avoid regex for Glob Matching

**Why regex is bad for globs:**
1. `.` in regex matches any character (in glob, it's literal)
2. `*` in regex means "0+ of previous" (in glob, it's wildcard)
3. Directory separator must be explicitly excluded
4. Regex compilation is slow

**Good: Shifting Wildcard Algorithm**
```cpp
bool match(std::string_view str, std::string_view pattern) {
    // See 90_ALGORITHMS.md for full implementation
    // O(N) worst case, no compilation overhead
}
```

---

## ✅ Testing Checklist

Before marking a component complete:

- [ ] Unit tests pass on Linux
- [ ] Unit tests pass on Windows (if applicable)
- [ ] Unit tests pass on macOS (if applicable)
- [ ] Valgrind reports no leaks
- [ ] ThreadSanitizer reports no data races
- [ ] Edge cases tested (empty input, missing files, etc.)
- [ ] Error messages are helpful (not just "error")
- [ ] Performance targets met

---

## 📚 Reference

For detailed algorithms, see:
- [90_ALGORITHMS.md](./90_ALGORITHMS.md) - Kahn's, Tri-color DFS, Shifting Wildcard
- [91_PLATFORM_COMPAT.md](./91_PLATFORM_COMPAT.md) - Cross-platform code patterns

---

**Remember:** When in doubt, consult this document before implementing!
