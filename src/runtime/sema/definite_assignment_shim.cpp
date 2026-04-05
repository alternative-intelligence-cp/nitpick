// ═══════════════════════════════════════════════════════════════════════
// Definite Assignment Shim — C bridge for self-hosted DA analysis
// v0.15.0: Port 5 of 5
// ═══════════════════════════════════════════════════════════════════════
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

// Assignment states
static constexpr int32_t DA_UNASSIGNED    = 0;
static constexpr int32_t DA_ASSIGNED      = 1;
static constexpr int32_t DA_MAYBE         = 2;

struct DAScope {
    std::vector<std::string> vars;
};

struct DASnapshot {
    std::unordered_map<std::string, int32_t> var_states;
    std::vector<DAScope> scope_stack;
    int32_t depth;
};

struct DAState {
    std::unordered_map<std::string, int32_t> var_states;
    std::vector<DAScope> scope_stack;
    int32_t depth = 0;
    DASnapshot slots[16];
    std::vector<std::string> errors;
};

static thread_local char da_ret_buf[4096];

static const char* da_return_str(const std::string& s) {
    size_t len = s.size();
    if (len >= sizeof(da_ret_buf)) len = sizeof(da_ret_buf) - 1;
    memcpy(da_ret_buf, s.c_str(), len);
    da_ret_buf[len] = '\0';
    return da_ret_buf;
}

extern "C" {

// ─── State management ─────────────────────────────────────────────
void* da_new() {
    return new DAState();
}

int32_t da_free(void* st) {
    delete static_cast<DAState*>(st);
    return 0;
}

void* da_null_ptr() { return nullptr; }
int32_t da_release(void* p) { (void)p; return 0; }

// ─── Scope management ─────────────────────────────────────────────
int32_t da_enter_scope(void* st) {
    auto* s = static_cast<DAState*>(st);
    s->depth++;
    s->scope_stack.push_back({});
    return 0;
}

int32_t da_exit_scope(void* st) {
    auto* s = static_cast<DAState*>(st);
    if (s->depth > 0 && !s->scope_stack.empty()) {
        for (const auto& var : s->scope_stack.back().vars) {
            s->var_states.erase(var);
        }
        s->scope_stack.pop_back();
        s->depth--;
    }
    return 0;
}

// ─── Variable tracking ────────────────────────────────────────────
int32_t da_declare(void* st, const char* name) {
    auto* s = static_cast<DAState*>(st);
    s->var_states[name] = DA_UNASSIGNED;
    if (!s->scope_stack.empty()) {
        s->scope_stack.back().vars.push_back(name);
    }
    return 0;
}

int32_t da_declare_init(void* st, const char* name) {
    auto* s = static_cast<DAState*>(st);
    s->var_states[name] = DA_ASSIGNED;
    if (!s->scope_stack.empty()) {
        s->scope_stack.back().vars.push_back(name);
    }
    return 0;
}

int32_t da_declare_wild(void* st, const char* name) {
    auto* s = static_cast<DAState*>(st);
    s->var_states[name] = DA_ASSIGNED;
    if (!s->scope_stack.empty()) {
        s->scope_stack.back().vars.push_back(name);
    }
    return 0;
}

int32_t da_assign(void* st, const char* name) {
    auto* s = static_cast<DAState*>(st);
    s->var_states[name] = DA_ASSIGNED;
    return 0;
}

int32_t da_get_state(void* st, const char* name) {
    auto* s = static_cast<DAState*>(st);
    auto it = s->var_states.find(name);
    if (it != s->var_states.end()) return it->second;
    return DA_ASSIGNED; // unknown vars assumed assigned (outer scope)
}

// ─── Snapshot / restore / merge ───────────────────────────────────
int32_t da_save(void* st, int32_t slot) {
    auto* s = static_cast<DAState*>(st);
    if (slot < 0 || slot >= 16) return 1;
    s->slots[slot].var_states = s->var_states;
    s->slots[slot].scope_stack = s->scope_stack;
    s->slots[slot].depth = s->depth;
    return 0;
}

int32_t da_load(void* st, int32_t slot) {
    auto* s = static_cast<DAState*>(st);
    if (slot < 0 || slot >= 16) return 1;
    s->var_states = s->slots[slot].var_states;
    s->scope_stack = s->slots[slot].scope_stack;
    s->depth = s->slots[slot].depth;
    return 0;
}

int32_t da_merge(void* st, int32_t slot_a, int32_t slot_b) {
    auto* s = static_cast<DAState*>(st);
    if (slot_a < 0 || slot_a >= 16 || slot_b < 0 || slot_b >= 16) return 1;
    const auto& a = s->slots[slot_a].var_states;
    const auto& b = s->slots[slot_b].var_states;

    // Collect all variable names from both snapshots
    std::unordered_map<std::string, int32_t> merged;
    for (const auto& [name, _] : a) merged[name] = DA_UNASSIGNED;
    for (const auto& [name, _] : b) merged[name] = DA_UNASSIGNED;

    for (auto& [name, _] : merged) {
        int32_t sa = DA_UNASSIGNED, sb = DA_UNASSIGNED;
        auto ia = a.find(name); if (ia != a.end()) sa = ia->second;
        auto ib = b.find(name); if (ib != b.end()) sb = ib->second;

        if (sa == DA_ASSIGNED && sb == DA_ASSIGNED) {
            merged[name] = DA_ASSIGNED;
        } else if (sa == DA_UNASSIGNED && sb == DA_UNASSIGNED) {
            merged[name] = DA_UNASSIGNED;
        } else {
            merged[name] = DA_MAYBE;
        }
    }

    // Apply merged states into current context
    for (const auto& [name, state] : merged) {
        s->var_states[name] = state;
    }
    return 0;
}

// ─── Read checking ────────────────────────────────────────────────
int32_t da_check_read(void* st, const char* name) {
    auto* s = static_cast<DAState*>(st);
    int32_t state = da_get_state(st, name);
    if (state == DA_UNASSIGNED) {
        s->errors.push_back(std::string("Use of uninitialized variable '") + name + "'");
        return 1;
    } else if (state == DA_MAYBE) {
        s->errors.push_back(std::string("Use of possibly uninitialized variable '") + name + "'");
        return 1;
    }
    return 0;
}

// ─── Error management ─────────────────────────────────────────────
int32_t da_add_error(void* st, const char* msg) {
    static_cast<DAState*>(st)->errors.push_back(msg);
    return 0;
}

int32_t da_error_count(void* st) {
    return static_cast<int32_t>(static_cast<DAState*>(st)->errors.size());
}

const char* da_get_error(void* st, int32_t index) {
    auto* s = static_cast<DAState*>(st);
    if (index < 0 || index >= (int32_t)s->errors.size()) return da_return_str("");
    return da_return_str(s->errors[index]);
}

int32_t da_clear_errors(void* st) {
    static_cast<DAState*>(st)->errors.clear();
    return 0;
}

// ─── Mark loop-assigned variables as MAYBE ────────────────────────
// Compare current state with snapshot in slot: any var that was
// not ASSIGNED in the snapshot but IS ASSIGNED now → MAYBE
int32_t da_mark_loop_maybe(void* st, int32_t before_slot) {
    auto* s = static_cast<DAState*>(st);
    if (before_slot < 0 || before_slot >= 16) return 1;
    const auto& before = s->slots[before_slot].var_states;

    for (auto& [name, state] : s->var_states) {
        if (state == DA_ASSIGNED) {
            auto it = before.find(name);
            if (it == before.end() || it->second != DA_ASSIGNED) {
                state = DA_MAYBE;
            }
        }
    }
    return 0;
}

} // extern "C"
