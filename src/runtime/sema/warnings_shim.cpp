// ═══════════════════════════════════════════════════════════════════════
// warnings_shim.cpp — C bridge for the self-hosted Warnings System
// v0.15.1 Port 7
// ═══════════════════════════════════════════════════════════════════════
// Exposes: config (enable/disable), flag parsing, warning emission,
//          type ↔ string conversion.
// ═══════════════════════════════════════════════════════════════════════

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

// Warning type constants (must match Aria-side consts 0..8)
enum WarnType : int32_t {
    WT_UNUSED_VARIABLE    = 0,
    WT_UNUSED_PARAMETER   = 1,
    WT_UNUSED_FUNCTION    = 2,
    WT_DEAD_CODE          = 3,
    WT_UNREACHABLE_CODE   = 4,
    WT_IMPLICIT_CONVERSION = 5,
    WT_EMPTY_BLOCK        = 6,
    WT_CONSTANT_CONDITION = 7,
    WT_SHADOWING          = 8,
    WT_COUNT              = 9
};

struct WarnEntry {
    int32_t type;
    std::string message;
};

struct WarnState {
    std::unordered_set<int32_t> enabled;
    bool warnings_as_errors = false;
    int32_t warning_count   = 0;
    std::vector<WarnEntry> warnings;
};

// Thread-local string return buffer
static thread_local std::string tls_buf;

static const char* tls_ret(const std::string& s) {
    tls_buf = s;
    return tls_buf.c_str();
}

// ─── Warning type name tables ────────────────────────────────────────
static const char* type_names[WT_COUNT] = {
    "unused-variable",
    "unused-parameter",
    "unused-function",
    "dead-code",
    "unreachable-code",
    "implicit-conversion",
    "empty-block",
    "constant-condition",
    "shadowing"
};

extern "C" {

// ─── Lifecycle ───────────────────────────────────────────────────────
void* warn_new() {
    auto* s = new WarnState;
    // Enable all by default (matches C++ WarningConfig ctor)
    for (int32_t i = 0; i < WT_COUNT; ++i) s->enabled.insert(i);
    return s;
}

int32_t warn_free(void* state) {
    delete static_cast<WarnState*>(state);
    return 0;
}

void* warn_null_ptr() { return nullptr; }
int32_t warn_release(void*) { return 0; }

// ─── Config: enable / disable ────────────────────────────────────────
int32_t warn_enable(void* state, int32_t wtype) {
    auto* s = static_cast<WarnState*>(state);
    if (wtype < 0 || wtype >= WT_COUNT) return -1;
    s->enabled.insert(wtype);
    return 0;
}

int32_t warn_disable(void* state, int32_t wtype) {
    auto* s = static_cast<WarnState*>(state);
    if (wtype < 0 || wtype >= WT_COUNT) return -1;
    s->enabled.erase(wtype);
    return 0;
}

int32_t warn_enable_all(void* state) {
    auto* s = static_cast<WarnState*>(state);
    for (int32_t i = 0; i < WT_COUNT; ++i) s->enabled.insert(i);
    return 0;
}

int32_t warn_disable_all(void* state) {
    auto* s = static_cast<WarnState*>(state);
    s->enabled.clear();
    return 0;
}

int32_t warn_is_enabled(void* state, int32_t wtype) {
    auto* s = static_cast<WarnState*>(state);
    return s->enabled.count(wtype) ? 1 : 0;
}

// ─── Warnings as errors ─────────────────────────────────────────────
int32_t warn_set_as_errors(void* state, int32_t flag) {
    auto* s = static_cast<WarnState*>(state);
    s->warnings_as_errors = (flag != 0);
    return 0;
}

int32_t warn_as_errors(void* state) {
    auto* s = static_cast<WarnState*>(state);
    return s->warnings_as_errors ? 1 : 0;
}

// ─── Type ↔ string ──────────────────────────────────────────────────
const char* warn_type_to_string(int32_t wtype) {
    if (wtype < 0 || wtype >= WT_COUNT) return tls_ret("unknown");
    return tls_ret(type_names[wtype]);
}

int32_t warn_string_to_type(const char* name) {
    if (!name) return -1;
    for (int32_t i = 0; i < WT_COUNT; ++i) {
        if (std::strcmp(name, type_names[i]) == 0) return i;
    }
    return -1;
}

// ─── Flag parsing (matches WarningFlagParser) ────────────────────────
int32_t warn_parse_flag(void* state, const char* flag) {
    if (!flag || !state) return -1;
    std::string f(flag);
    auto* s = static_cast<WarnState*>(state);

    if (f == "-Werror") { s->warnings_as_errors = true; return 0; }
    if (f == "-Wall")   { warn_enable_all(state); return 0; }
    if (f == "-Wno-all"){ warn_disable_all(state); return 0; }

    if (f.size() > 2 && f[0] == '-' && f[1] == 'W') {
        std::string rest = f.substr(2);
        bool neg = false;
        if (rest.size() > 3 && rest.substr(0, 3) == "no-") {
            neg = true;
            rest = rest.substr(3);
        }
        int32_t wt = warn_string_to_type(rest.c_str());
        if (wt < 0) return -1;
        if (neg) s->enabled.erase(wt); else s->enabled.insert(wt);
        return 0;
    }
    return -1;
}

// ─── Warning emission / query ────────────────────────────────────────
int32_t warn_emit(void* state, int32_t wtype, const char* message) {
    auto* s = static_cast<WarnState*>(state);
    if (!s->enabled.count(wtype)) return 0; // suppressed
    s->warning_count++;
    std::string full = std::string("[") + type_names[wtype] + "] " + (message ? message : "");
    s->warnings.push_back({wtype, full});
    return s->warnings_as_errors ? 2 : 1; // 2 = promoted to error
}

int32_t warn_count(void* state) {
    return static_cast<WarnState*>(state)->warning_count;
}

int32_t warn_entry_count(void* state) {
    return static_cast<int32_t>(static_cast<WarnState*>(state)->warnings.size());
}

const char* warn_get_message(void* state, int32_t index) {
    auto* s = static_cast<WarnState*>(state);
    if (index < 0 || index >= (int32_t)s->warnings.size()) return tls_ret("");
    return tls_ret(s->warnings[index].message);
}

int32_t warn_get_type(void* state, int32_t index) {
    auto* s = static_cast<WarnState*>(state);
    if (index < 0 || index >= (int32_t)s->warnings.size()) return -1;
    return s->warnings[index].type;
}

int32_t warn_clear(void* state) {
    auto* s = static_cast<WarnState*>(state);
    s->warnings.clear();
    s->warning_count = 0;
    return 0;
}

// ─── Terminal-statement check (simplified for shim) ──────────────────
// 0=return, 1=pass, 2=fail, 3=break, 4=continue
int32_t warn_is_terminal(int32_t stmt_kind) {
    return (stmt_kind >= 0 && stmt_kind <= 4) ? 1 : 0;
}

// ─── Supported flags list (count + indexed access) ───────────────────
static const std::vector<std::string>& supported_flags() {
    static const std::vector<std::string> flags = {
        "-Wall", "-Werror", "-Wno-all",
        "-Wunused-variable",  "-Wno-unused-variable",
        "-Wunused-parameter", "-Wno-unused-parameter",
        "-Wunused-function",  "-Wno-unused-function",
        "-Wdead-code",        "-Wno-dead-code",
        "-Wunreachable-code", "-Wno-unreachable-code",
        "-Wimplicit-conversion","-Wno-implicit-conversion",
        "-Wempty-block",      "-Wno-empty-block",
        "-Wconstant-condition","-Wno-constant-condition",
        "-Wshadowing",        "-Wno-shadowing"
    };
    return flags;
}

int32_t warn_supported_flag_count() {
    return static_cast<int32_t>(supported_flags().size());
}

const char* warn_supported_flag(int32_t index) {
    auto& v = supported_flags();
    if (index < 0 || index >= (int32_t)v.size()) return tls_ret("");
    return tls_ret(v[index]);
}

} // extern "C"
