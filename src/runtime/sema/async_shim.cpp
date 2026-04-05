/**
 * @file async_shim.cpp
 * @brief C bridge for self-hosted Async Semantic Analyzer (v0.15.0)
 *
 * Provides extern "C" functions for state management and error collection.
 * The actual async/await validation logic lives in Aria
 * (stdlib/async_analyzer.aria).
 *
 * Pattern: Same as visibility_shim.cpp — state handle + error collection.
 */

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// ═══════════════════════════════════════════════════════════════════════
// Async Analyzer State
// ═══════════════════════════════════════════════════════════════════════

struct AsyncAnalyzerState {
    std::vector<std::string> errors;
};

// Returned C strings must remain valid until the caller copies them.
static thread_local std::string aa_str_buf;

static const char* aa_return_str(const std::string& s) {
    aa_str_buf = s;
    return aa_str_buf.c_str();
}

extern "C" {

// ── Null pointer helper ──────────────────────────────────────────────

void* aa_null_ptr() {
    return nullptr;
}

// ── State management ─────────────────────────────────────────────────

void* aa_new() {
    return new AsyncAnalyzerState();
}

int32_t aa_free(void* state) {
    delete (AsyncAnalyzerState*)state;
    return 0;
}

// ── Error collection ─────────────────────────────────────────────────

int32_t aa_add_error(void* state, const char* message) {
    if (!state || !message) return 0;
    auto* s = (AsyncAnalyzerState*)state;
    s->errors.push_back(std::string(message));
    return 0;
}

int32_t aa_error_count(void* state) {
    if (!state) return 0;
    auto* s = (AsyncAnalyzerState*)state;
    return (int32_t)s->errors.size();
}

const char* aa_get_error(void* state, int32_t index) {
    if (!state) return "";
    auto* s = (AsyncAnalyzerState*)state;
    if (index < 0 || index >= (int32_t)s->errors.size()) {
        return "";
    }
    return aa_return_str(s->errors[index]);
}

int32_t aa_clear_errors(void* state) {
    if (!state) return 0;
    auto* s = (AsyncAnalyzerState*)state;
    s->errors.clear();
    return 0;
}

} // extern "C"
