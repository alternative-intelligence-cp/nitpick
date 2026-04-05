/**
 * @file module_table_shim.cpp
 * @brief C bridge for self-hosted Module Table (v0.15.0)
 *
 * Provides extern "C" functions for module storage and data extraction.
 * The resolution/traversal logic lives in Aria (stdlib/module_table.aria).
 *
 * Pattern: Same as visibility_shim.cpp — opaque handles + data extraction.
 */

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// ═══════════════════════════════════════════════════════════════════════
// Module Types (standalone, no compiler dependency)
// ═══════════════════════════════════════════════════════════════════════

struct MTModule {
    std::string name;
    std::string path;
    MTModule* parent;
    std::vector<MTModule*> submodules;   // non-owning
    std::vector<std::string> imports;
    std::unordered_map<std::string, int32_t> exports;  // name → visibility
    bool resolved;

    MTModule(const std::string& n, const std::string& p, MTModule* par)
        : name(n), path(p), parent(par), resolved(false) {}
};

struct MTState {
    std::vector<std::unique_ptr<MTModule>> modules;  // owns all modules
    std::unordered_map<std::string, MTModule*> registry;  // fullpath → module
    MTModule* root;
    std::vector<std::string> errors;
};

static thread_local std::string mt_str_buf;

static const char* mt_return_str(const std::string& s) {
    mt_str_buf = s;
    return mt_str_buf.c_str();
}

extern "C" {

// ── State management ─────────────────────────────────────────────────

void* mt_new() {
    auto* s = new MTState();
    auto root = std::make_unique<MTModule>("root", "<root>", nullptr);
    s->root = root.get();
    s->registry["root"] = root.get();
    s->modules.push_back(std::move(root));
    return s;
}

int32_t mt_free(void* state) {
    delete (MTState*)state;
    return 0;
}

// ── Module creation ──────────────────────────────────────────────────

void* mt_create_module(void* state, const char* name, const char* path, void* parent) {
    if (!state || !name || !path) return nullptr;
    auto* s = (MTState*)state;
    MTModule* par = parent ? (MTModule*)parent : s->root;

    auto mod = std::make_unique<MTModule>(name, path, par);
    MTModule* ptr = mod.get();
    par->submodules.push_back(ptr);
    s->modules.push_back(std::move(mod));
    return ptr;
}

// Register module in registry by given full path
int32_t mt_register(void* state, const char* full_path, void* module_ptr) {
    if (!state || !full_path || !module_ptr) return 0;
    auto* s = (MTState*)state;
    s->registry[std::string(full_path)] = (MTModule*)module_ptr;
    return 0;
}

void* mt_get_module(void* state, const char* full_path) {
    if (!state || !full_path) return nullptr;
    auto* s = (MTState*)state;
    auto it = s->registry.find(std::string(full_path));
    if (it != s->registry.end()) return it->second;
    return nullptr;
}

void* mt_get_root(void* state) {
    if (!state) return nullptr;
    return ((MTState*)state)->root;
}

void* mt_null_ptr() {
    return nullptr;
}

// No-op: modules are owned by MTState. This exists to satisfy
// the borrow checker's requirement that wild pointers are "freed".
int32_t mt_mod_release(void* mod) {
    (void)mod;
    return 0;
}

// ── Module data extraction ───────────────────────────────────────────

const char* mt_mod_get_name(void* mod) {
    if (!mod) return "";
    return mt_return_str(((MTModule*)mod)->name);
}

const char* mt_mod_get_path(void* mod) {
    if (!mod) return "";
    return mt_return_str(((MTModule*)mod)->path);
}

void* mt_mod_get_parent(void* mod) {
    if (!mod) return nullptr;
    return ((MTModule*)mod)->parent;
}

int32_t mt_mod_is_null(void* mod) {
    return mod == nullptr ? 1 : 0;
}

void* mt_mod_get_submodule(void* mod, const char* name) {
    if (!mod || !name) return nullptr;
    auto* m = (MTModule*)mod;
    for (auto* sub : m->submodules) {
        if (sub->name == name) return sub;
    }
    return nullptr;
}

int32_t mt_mod_submodule_count(void* mod) {
    if (!mod) return 0;
    return (int32_t)((MTModule*)mod)->submodules.size();
}

void* mt_mod_get_submodule_at(void* mod, int32_t index) {
    if (!mod) return nullptr;
    auto* m = (MTModule*)mod;
    if (index < 0 || index >= (int32_t)m->submodules.size()) return nullptr;
    return m->submodules[index];
}

// ── Export management ────────────────────────────────────────────────

int32_t mt_mod_export(void* mod, const char* name, int32_t visibility) {
    if (!mod || !name) return 0;
    ((MTModule*)mod)->exports[std::string(name)] = visibility;
    return 0;
}

int32_t mt_mod_is_exported(void* mod, const char* name) {
    if (!mod || !name) return 0;
    auto* m = (MTModule*)mod;
    return m->exports.find(std::string(name)) != m->exports.end() ? 1 : 0;
}

int32_t mt_mod_get_export_vis(void* mod, const char* name) {
    if (!mod || !name) return -1;
    auto* m = (MTModule*)mod;
    auto it = m->exports.find(std::string(name));
    if (it != m->exports.end()) return it->second;
    return -1;
}

// ── Import management ────────────────────────────────────────────────

int32_t mt_mod_add_import(void* mod, const char* import_path) {
    if (!mod || !import_path) return 0;
    ((MTModule*)mod)->imports.push_back(std::string(import_path));
    return 0;
}

int32_t mt_mod_import_count(void* mod) {
    if (!mod) return 0;
    return (int32_t)((MTModule*)mod)->imports.size();
}

const char* mt_mod_get_import(void* mod, int32_t index) {
    if (!mod) return "";
    auto* m = (MTModule*)mod;
    if (index < 0 || index >= (int32_t)m->imports.size()) return "";
    return mt_return_str(m->imports[index]);
}

int32_t mt_mod_is_resolved(void* mod) {
    if (!mod) return 0;
    return ((MTModule*)mod)->resolved ? 1 : 0;
}

int32_t mt_mod_mark_resolved(void* mod) {
    if (!mod) return 0;
    ((MTModule*)mod)->resolved = true;
    return 0;
}

// ── String utility: split by dot ────────────────────────────────────
// Returns count of segments. Stores first segment into buffer.
// Used by Aria for import path resolution via iterative calls.

// Get segment count from dot-separated path
int32_t mt_path_segment_count(const char* path_str) {
    if (!path_str || path_str[0] == '\0') return 0;
    int32_t count = 1;
    for (const char* p = path_str; *p; p++) {
        if (*p == '.') count++;
    }
    return count;
}

// Get nth segment from dot-separated path
const char* mt_path_segment(const char* path_str, int32_t index) {
    if (!path_str) return "";
    std::string path(path_str);
    int32_t current = 0;
    size_t start = 0;
    for (size_t i = 0; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '.') {
            if (current == index) {
                return mt_return_str(path.substr(start, i - start));
            }
            current++;
            start = i + 1;
        }
    }
    return "";
}

// ── Error management ─────────────────────────────────────────────────

int32_t mt_add_error(void* state, const char* msg) {
    if (!state || !msg) return 0;
    ((MTState*)state)->errors.push_back(std::string(msg));
    return 0;
}

int32_t mt_error_count(void* state) {
    if (!state) return 0;
    return (int32_t)((MTState*)state)->errors.size();
}

const char* mt_get_error(void* state, int32_t index) {
    if (!state) return "";
    auto* s = (MTState*)state;
    if (index < 0 || index >= (int32_t)s->errors.size()) return "";
    return mt_return_str(s->errors[index]);
}

int32_t mt_clear_errors(void* state) {
    if (!state) return 0;
    ((MTState*)state)->errors.clear();
    return 0;
}

} // extern "C"
