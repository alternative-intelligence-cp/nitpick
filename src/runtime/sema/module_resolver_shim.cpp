// ═══════════════════════════════════════════════════════════════════════
// module_resolver_shim.cpp — C bridge for the self-hosted Module Resolver
// v0.15.1 Port 8
// ═══════════════════════════════════════════════════════════════════════
// Exposes: search-path management, path resolution patterns, circular
//          dependency detection stack, path normalization utilities.
// ═══════════════════════════════════════════════════════════════════════

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_set>

#ifdef _WIN32
#include <direct.h>
#define PATH_SEP '\\'
#else
#include <unistd.h>
#define PATH_SEP '/'
#endif

struct MRState {
    std::string root_path;
    std::vector<std::string> search_paths;
    // Circular-dependency detection
    std::vector<std::string> loading_stack;
    std::unordered_set<std::string> loading_set;
    // Errors
    std::vector<std::string> errors;
};

static thread_local std::string tls_buf;
static const char* tls_ret(const std::string& s) {
    tls_buf = s;
    return tls_buf.c_str();
}

// ─── Path utilities (pure string, no filesystem) ─────────────────────
static std::string normalize(const std::string& p) {
    if (p.empty()) return ".";
    std::vector<std::string> parts;
    std::string seg;
    for (size_t i = 0; i < p.size(); ++i) {
        if (p[i] == '/' || p[i] == '\\') {
            if (!seg.empty()) {
                if (seg == "..") { if (!parts.empty() && parts.back() != "..") parts.pop_back(); else parts.push_back(seg); }
                else if (seg != ".") parts.push_back(seg);
                seg.clear();
            }
        } else {
            seg += p[i];
        }
    }
    if (!seg.empty()) {
        if (seg == "..") { if (!parts.empty() && parts.back() != "..") parts.pop_back(); else parts.push_back(seg); }
        else if (seg != ".") parts.push_back(seg);
    }
    std::string result;
    if (!p.empty() && p[0] == '/') result = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += '/';
        result += parts[i];
    }
    return result.empty() ? "." : result;
}

static std::string get_directory(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

static std::string build_path(const std::string& base, const std::string& rel) {
    if (base.empty()) return rel;
    if (rel.empty()) return base;
    char last = base.back();
    if (last == '/' || last == '\\') return base + rel;
    return base + '/' + rel;
}

extern "C" {

// ─── Lifecycle ───────────────────────────────────────────────────────
void* mr_new(const char* root) {
    auto* s = new MRState;
    s->root_path = root ? root : "";
    // Default search paths (mimics C++ constructor)
    if (!s->root_path.empty()) {
        s->search_paths.push_back(build_path(s->root_path, "stdlib"));
    }
    s->search_paths.push_back("/usr/lib/aria");
    s->search_paths.push_back("/usr/local/lib/aria");
    // ARIA_PATH env
    const char* env = std::getenv("ARIA_PATH");
    if (env) {
        std::string e(env);
        size_t start = 0;
        while (start < e.size()) {
            auto pos = e.find(':', start);
            std::string seg = (pos == std::string::npos) ? e.substr(start) : e.substr(start, pos - start);
            if (!seg.empty()) s->search_paths.push_back(seg);
            if (pos == std::string::npos) break;
            start = pos + 1;
        }
    }
    return s;
}

int32_t mr_free(void* state) {
    delete static_cast<MRState*>(state);
    return 0;
}

void* mr_null_ptr() { return nullptr; }
int32_t mr_release(void*) { return 0; }

// ─── Root path ───────────────────────────────────────────────────────
const char* mr_root_path(void* state) {
    return tls_ret(static_cast<MRState*>(state)->root_path);
}

// ─── Search paths ────────────────────────────────────────────────────
int32_t mr_search_path_count(void* state) {
    return static_cast<int32_t>(static_cast<MRState*>(state)->search_paths.size());
}

const char* mr_search_path(void* state, int32_t idx) {
    auto* s = static_cast<MRState*>(state);
    if (idx < 0 || idx >= (int32_t)s->search_paths.size()) return tls_ret("");
    return tls_ret(s->search_paths[idx]);
}

int32_t mr_add_search_path(void* state, const char* path) {
    static_cast<MRState*>(state)->search_paths.push_back(path ? path : "");
    return 0;
}

// ─── Path utilities exposed to Aria ─────────────────────────────────
const char* mr_normalize(const char* path) {
    return tls_ret(normalize(path ? path : ""));
}

const char* mr_get_directory(const char* path) {
    return tls_ret(get_directory(path ? path : ""));
}

const char* mr_build_path(const char* base, const char* rel) {
    return tls_ret(build_path(base ? base : "", rel ? rel : ""));
}

int32_t mr_is_absolute(const char* path) {
    if (!path || !path[0]) return 0;
    return path[0] == '/' ? 1 : 0;
}

int32_t mr_is_relative(const char* path) {
    if (!path || !path[0]) return 1;
    return path[0] != '/' ? 1 : 0;
}

// ─── Module resolution patterns ─────────────────────────────────────
// Pattern 1: <base>/<logical>.aria
const char* mr_try_pattern1(const char* base, const char* logical) {
    std::string result = build_path(base ? base : "", std::string(logical ? logical : "") + ".aria");
    return tls_ret(normalize(result));
}

// Pattern 2: <base>/<logical>/mod.aria
const char* mr_try_pattern2(const char* base, const char* logical) {
    std::string mid = build_path(base ? base : "", logical ? logical : "");
    std::string result = build_path(mid, "mod.aria");
    return tls_ret(normalize(result));
}

// Resolve relative to current module's directory
const char* mr_resolve_relative(const char* current_module, const char* import_path) {
    std::string dir = get_directory(current_module ? current_module : "");
    std::string result = build_path(dir, import_path ? import_path : "");
    return tls_ret(normalize(result));
}

// ─── Circular dependency detection ──────────────────────────────────
int32_t mr_begin_loading(void* state, const char* module_path) {
    auto* s = static_cast<MRState*>(state);
    std::string p = normalize(module_path ? module_path : "");
    if (s->loading_set.count(p)) return -1; // circular!
    s->loading_set.insert(p);
    s->loading_stack.push_back(p);
    return 0;
}

int32_t mr_end_loading(void* state, const char* module_path) {
    auto* s = static_cast<MRState*>(state);
    std::string p = normalize(module_path ? module_path : "");
    s->loading_set.erase(p);
    if (!s->loading_stack.empty() && s->loading_stack.back() == p) {
        s->loading_stack.pop_back();
    }
    return 0;
}

int32_t mr_is_loading(void* state, const char* module_path) {
    auto* s = static_cast<MRState*>(state);
    std::string p = normalize(module_path ? module_path : "");
    return s->loading_set.count(p) ? 1 : 0;
}

int32_t mr_loading_depth(void* state) {
    return static_cast<int32_t>(static_cast<MRState*>(state)->loading_stack.size());
}

const char* mr_loading_at(void* state, int32_t idx) {
    auto* s = static_cast<MRState*>(state);
    if (idx < 0 || idx >= (int32_t)s->loading_stack.size()) return tls_ret("");
    return tls_ret(s->loading_stack[idx]);
}

// ─── Error management ───────────────────────────────────────────────
int32_t mr_add_error(void* state, const char* msg) {
    static_cast<MRState*>(state)->errors.push_back(msg ? msg : "");
    return 0;
}

int32_t mr_error_count(void* state) {
    return static_cast<int32_t>(static_cast<MRState*>(state)->errors.size());
}

const char* mr_get_error(void* state, int32_t idx) {
    auto* s = static_cast<MRState*>(state);
    if (idx < 0 || idx >= (int32_t)s->errors.size()) return tls_ret("");
    return tls_ret(s->errors[idx]);
}

int32_t mr_clear_errors(void* state) {
    static_cast<MRState*>(state)->errors.clear();
    return 0;
}

int32_t mr_reset(void* state) {
    auto* s = static_cast<MRState*>(state);
    s->loading_stack.clear();
    s->loading_set.clear();
    s->errors.clear();
    return 0;
}

} // extern "C"
