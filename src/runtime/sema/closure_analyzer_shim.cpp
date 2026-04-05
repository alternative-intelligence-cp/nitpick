// ═══════════════════════════════════════════════════════════════════════
// Closure Analyzer Shim — C bridge for self-hosted closure analysis
// v0.15.1: Port 6 of 9
// ═══════════════════════════════════════════════════════════════════════
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Capture modes (match LambdaExpr::CaptureMode)
static constexpr int32_t CAP_BY_VALUE     = 0;
static constexpr int32_t CAP_BY_REFERENCE = 1;
static constexpr int32_t CAP_BY_MOVE      = 2;

struct CaptureInfo {
    std::string name;
    int32_t mode;         // CAP_BY_VALUE, CAP_BY_REFERENCE, CAP_BY_MOVE
    bool is_mutated;
    bool is_addr_taken;
    int32_t usage_count;
};

struct CLAState {
    std::unordered_set<std::string> params;       // parameter names (not captures)
    std::unordered_set<std::string> locals;       // local variables (not captures)
    std::unordered_map<std::string, CaptureInfo> captures;
    std::vector<std::string> capture_order;       // insertion order
    std::vector<std::string> errors;
};

static thread_local char cla_ret_buf[4096];

static const char* cla_return_str(const std::string& s) {
    size_t len = s.size();
    if (len >= sizeof(cla_ret_buf)) len = sizeof(cla_ret_buf) - 1;
    memcpy(cla_ret_buf, s.c_str(), len);
    cla_ret_buf[len] = '\0';
    return cla_ret_buf;
}

extern "C" {

// ─── State management ─────────────────────────────────────────────
void* cla_new() { return new CLAState(); }

int32_t cla_free(void* st) {
    delete static_cast<CLAState*>(st);
    return 0;
}

void* cla_null_ptr() { return nullptr; }
int32_t cla_release(void* p) { (void)p; return 0; }

// ─── Parameter / local registration ──────────────────────────────
int32_t cla_add_param(void* st, const char* name) {
    static_cast<CLAState*>(st)->params.insert(name);
    return 0;
}

int32_t cla_add_local(void* st, const char* name) {
    static_cast<CLAState*>(st)->locals.insert(name);
    return 0;
}

int32_t cla_is_param(void* st, const char* name) {
    return static_cast<CLAState*>(st)->params.count(name) ? 1 : 0;
}

int32_t cla_is_local(void* st, const char* name) {
    return static_cast<CLAState*>(st)->locals.count(name) ? 1 : 0;
}

// ─── Capture tracking ────────────────────────────────────────────
int32_t cla_record_capture(void* st, const char* name) {
    auto* s = static_cast<CLAState*>(st);
    if (s->captures.count(name) == 0) {
        CaptureInfo ci;
        ci.name = name;
        ci.mode = CAP_BY_VALUE;
        ci.is_mutated = false;
        ci.is_addr_taken = false;
        ci.usage_count = 0;
        s->captures[name] = ci;
        s->capture_order.push_back(name);
    }
    s->captures[name].usage_count++;
    return 0;
}

int32_t cla_mark_mutated(void* st, const char* name) {
    auto* s = static_cast<CLAState*>(st);
    auto it = s->captures.find(name);
    if (it != s->captures.end()) {
        it->second.is_mutated = true;
    }
    return 0;
}

int32_t cla_mark_addr_taken(void* st, const char* name) {
    auto* s = static_cast<CLAState*>(st);
    auto it = s->captures.find(name);
    if (it != s->captures.end()) {
        it->second.is_addr_taken = true;
    }
    return 0;
}

int32_t cla_capture_count(void* st) {
    return static_cast<int32_t>(static_cast<CLAState*>(st)->captures.size());
}

const char* cla_capture_name(void* st, int32_t index) {
    auto* s = static_cast<CLAState*>(st);
    if (index < 0 || index >= (int32_t)s->capture_order.size())
        return cla_return_str("");
    return cla_return_str(s->capture_order[index]);
}

int32_t cla_capture_is_mutated(void* st, const char* name) {
    auto* s = static_cast<CLAState*>(st);
    auto it = s->captures.find(name);
    if (it != s->captures.end()) return it->second.is_mutated ? 1 : 0;
    return 0;
}

int32_t cla_capture_is_addr_taken(void* st, const char* name) {
    auto* s = static_cast<CLAState*>(st);
    auto it = s->captures.find(name);
    if (it != s->captures.end()) return it->second.is_addr_taken ? 1 : 0;
    return 0;
}

int32_t cla_capture_usage(void* st, const char* name) {
    auto* s = static_cast<CLAState*>(st);
    auto it = s->captures.find(name);
    if (it != s->captures.end()) return it->second.usage_count;
    return 0;
}

// ─── Capture mode determination ──────────────────────────────────
int32_t cla_set_capture_mode(void* st, const char* name, int32_t cap_mode) {
    auto* s = static_cast<CLAState*>(st);
    auto it = s->captures.find(name);
    if (it != s->captures.end()) {
        it->second.mode = cap_mode;
        return 0;
    }
    return 1;
}

int32_t cla_get_capture_mode(void* st, const char* name) {
    auto* s = static_cast<CLAState*>(st);
    auto it = s->captures.find(name);
    if (it != s->captures.end()) return it->second.mode;
    return CAP_BY_VALUE;
}

// ─── Determine mode automatically based on usage ─────────────────
int32_t cla_determine_mode(void* st, const char* name, int32_t type_is_primitive) {
    auto* s = static_cast<CLAState*>(st);
    auto it = s->captures.find(name);
    if (it == s->captures.end()) return CAP_BY_VALUE;

    // Rule 1: mutated or address-taken → BY_REFERENCE
    if (it->second.is_mutated || it->second.is_addr_taken)
        return CAP_BY_REFERENCE;

    // Rule 2: primitive types → BY_VALUE
    if (type_is_primitive) return CAP_BY_VALUE;

    // Default: BY_VALUE for immutable captures
    return CAP_BY_VALUE;
}

// ─── Reset for new lambda ────────────────────────────────────────
int32_t cla_reset(void* st) {
    auto* s = static_cast<CLAState*>(st);
    s->params.clear();
    s->locals.clear();
    s->captures.clear();
    s->capture_order.clear();
    s->errors.clear();
    return 0;
}

// ─── Error management ────────────────────────────────────────────
int32_t cla_add_error(void* st, const char* msg) {
    static_cast<CLAState*>(st)->errors.push_back(msg);
    return 0;
}

int32_t cla_error_count(void* st) {
    return static_cast<int32_t>(static_cast<CLAState*>(st)->errors.size());
}

const char* cla_get_error(void* st, int32_t index) {
    auto* s = static_cast<CLAState*>(st);
    if (index < 0 || index >= (int32_t)s->errors.size())
        return cla_return_str("");
    return cla_return_str(s->errors[index]);
}

int32_t cla_clear_errors(void* st) {
    static_cast<CLAState*>(st)->errors.clear();
    return 0;
}

} // extern "C"
