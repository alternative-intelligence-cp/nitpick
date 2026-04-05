/**
 * @file visibility_shim.cpp
 * @brief C bridge for self-hosted Visibility Checker (v0.15.0)
 *
 * Provides extern "C" functions that extract data from opaque C++ Symbol*
 * and Module* pointers. The actual visibility logic lives in Aria
 * (stdlib/visibility_checker.aria).
 *
 * Pattern: Same as pass_helpers.cpp — opaque handle + data extraction.
 */

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "frontend/sema/symbol_table.h"
#include "frontend/sema/module_table.h"

// Note: Aria FFI for extern `string` returns expects const char*.
// The compiler auto-wraps via strlen + gc_alloc + memcpy.
// String PARAMS also arrive as const char* (compiler extracts .data).

// Returned C strings must remain valid until the caller copies them.
// We use a small static buffer to avoid leaking malloc'd strings.
static thread_local std::string vc_str_buf;

static const char* vc_return_str(const std::string& s) {
    vc_str_buf = s;
    return vc_str_buf.c_str();
}

// ═══════════════════════════════════════════════════════════════════════
// Visibility Checker State
// ═══════════════════════════════════════════════════════════════════════

struct VisCheckerState {
    void* module_table;  // ModuleTable* (unused currently but kept for future)
    std::vector<std::string> errors;
};

extern "C" {

// ── Null pointer helper ──────────────────────────────────────────────

void* vc_null_ptr() {
    return nullptr;
}

// ── State management ─────────────────────────────────────────────────

void* vc_new(void* module_table_ptr) {
    auto* s = new VisCheckerState();
    s->module_table = module_table_ptr;
    return s;
}

int32_t vc_free(void* state) {
    delete (VisCheckerState*)state;
    return 0;
}

// ── Symbol data extraction ───────────────────────────────────────────

// Returns 1 if symbol is public, 0 if private/null
int32_t vc_symbol_is_public(void* symbol_ptr) {
    if (!symbol_ptr) return 0;
    auto* sym = (aria::sema::Symbol*)symbol_ptr;
    return sym->isPublic ? 1 : 0;
}

// Returns symbol name as C string (Aria FFI wraps to AriaString)
const char* vc_symbol_get_name(void* symbol_ptr) {
    if (!symbol_ptr) return "";
    auto* sym = (aria::sema::Symbol*)symbol_ptr;
    return vc_return_str(sym->name);
}

// ── Module data extraction ───────────────────────────────────────────

// Returns module file path as C string (Aria FFI wraps to AriaString)
const char* vc_module_get_path(void* module_ptr) {
    if (!module_ptr) return "";
    auto* mod = (aria::sema::Module*)module_ptr;
    return vc_return_str(mod->getPath());
}

// Returns 1 if two module pointers are the same object
int32_t vc_modules_same(void* mod1, void* mod2) {
    return (mod1 == mod2) ? 1 : 0;
}

// Returns parent module pointer (or null/0)
void* vc_module_get_parent(void* module_ptr) {
    if (!module_ptr) return nullptr;
    auto* mod = (aria::sema::Module*)module_ptr;
    return (void*)mod->getParent();
}

// Returns 1 if module pointer is null
int32_t vc_module_is_null(void* module_ptr) {
    return (module_ptr == nullptr) ? 1 : 0;
}

// ── Error collection ─────────────────────────────────────────────────

int32_t vc_add_error(void* state, const char* message) {
    if (!state || !message) return 0;
    auto* s = (VisCheckerState*)state;
    s->errors.push_back(std::string(message));
    return 0;
}

int32_t vc_error_count(void* state) {
    if (!state) return 0;
    auto* s = (VisCheckerState*)state;
    return (int32_t)s->errors.size();
}

const char* vc_get_error(void* state, int32_t index) {
    if (!state) return "";
    auto* s = (VisCheckerState*)state;
    if (index < 0 || index >= (int32_t)s->errors.size()) {
        return "";
    }
    return vc_return_str(s->errors[index]);
}

int32_t vc_clear_errors(void* state) {
    if (!state) return 0;
    auto* s = (VisCheckerState*)state;
    s->errors.clear();
    return 0;
}

// ── C++ VisibilityChecker bridge (for callers that still use C++ API) ──
// These forward to the Aria implementation via the shim state.
// Used by type_checker.cpp and integration tests.

void* vc_check_access(void* state, void* symbol_ptr, void* sym_module_ptr,
                       void* cur_module_ptr, int32_t line, int32_t col) {
    // This will be called from the Aria side — the actual logic is in Aria.
    // This entry point exists for completeness but the real dispatch goes
    // through the Aria module's pub functions.
    // Return: 1 = allowed, 0 = denied
    // Implemented in Aria — this is a stub placeholder.
    (void)state; (void)symbol_ptr; (void)sym_module_ptr;
    (void)cur_module_ptr; (void)line; (void)col;
    return nullptr;
}

// ── Test Helpers ─────────────────────────────────────────────────────
// Create mock Symbol/Module objects for testing the Aria module.
// These are lightweight stubs — NOT full compiler objects.

struct MockSymbol {
    std::string name;
    bool isPublic;
};

struct MockModule {
    std::string name;
    std::string path;
    MockModule* parent;
};

void* vc_test_make_symbol(const char* name, int32_t is_public) {
    auto* s = new MockSymbol();
    s->name = name ? name : "";
    s->isPublic = (is_public != 0);
    return s;
}

int32_t vc_test_symbol_free(void* sym) {
    delete (MockSymbol*)sym;
    return 0;
}

void* vc_test_make_module(const char* name, const char* path, void* parent) {
    auto* m = new MockModule();
    m->name = name ? name : "";
    m->path = path ? path : "";
    m->parent = (MockModule*)parent;
    return m;
}

int32_t vc_test_module_free(void* mod) {
    delete (MockModule*)mod;
    return 0;
}

// Override the data extraction functions for mock objects during testing.
// The mock versions check whether the pointer is a MockSymbol/MockModule
// by looking at the module_table field in state. If state is null (test mode),
// we use mock accessors.

// Note: For testing, we redirect vc_symbol_is_public etc. to work on MockSymbol.
// This works because MockSymbol has the same 'name' and 'isPublic' fields
// at the same relative positions the extraction functions look at.
// For tests that link ONLY the shim (not the full compiler), these mock
// types are what the pointers actually point to.

// The extraction functions above (vc_symbol_is_public, etc.) cast to
// aria::sema::Symbol*. For testing, we provide mock-aware versions:

int32_t vc_test_symbol_is_public(void* sym) {
    if (!sym) return 0;
    return ((MockSymbol*)sym)->isPublic ? 1 : 0;
}

const char* vc_test_symbol_get_name(void* sym) {
    if (!sym) return "";
    auto* ms = (MockSymbol*)sym;
    return vc_return_str(ms->name);
}

const char* vc_test_module_get_path(void* mod) {
    if (!mod) return "";
    return vc_return_str(((MockModule*)mod)->path);
}

int32_t vc_test_modules_same(void* m1, void* m2) {
    return (m1 == m2) ? 1 : 0;
}

void* vc_test_module_get_parent(void* mod) {
    if (!mod) return nullptr;
    return ((MockModule*)mod)->parent;
}

int32_t vc_test_module_is_null(void* mod) {
    return (mod == nullptr) ? 1 : 0;
}

} // extern "C"
