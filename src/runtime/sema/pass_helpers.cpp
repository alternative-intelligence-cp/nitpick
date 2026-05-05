/**
 * @file pass_helpers.cpp
 * @brief C helpers for Phase 5.5 self-hosted supporting passes:
 *        Safety Checker, Exhaustiveness Checker, Const Evaluator, Borrow Checker
 *
 * Provides opaque state management and complex operations for Aria modules.
 * Each pass uses a prefixed API: safety_*, exhaust_*, consteval_*, borrow_*
 */

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <limits>

// AriaString: matches the runtime ABI format {data, length}
struct AriaString {
    char* data;
    int64_t length;
};

static char* dup_str(const char* s) {
    if (!s) return nullptr;
    size_t len = strlen(s);
    char* d = (char*)malloc(len + 1);
    memcpy(d, s, len + 1);
    return d;
}

static AriaString* make_aria_str(const std::string& s) {
    auto* as = (AriaString*)malloc(sizeof(AriaString));
    as->data = (char*)malloc(s.size() + 1);
    memcpy(as->data, s.c_str(), s.size() + 1);
    as->length = (int64_t)s.size();
    return as;
}

// ═══════════════════════════════════════════════════════════════════════
// SAFETY CHECKER HELPERS
// ═══════════════════════════════════════════════════════════════════════

struct SafetyIssue {
    int32_t operation;    // 0=WILD, 1=ASYNC, 2=POINTER, 3=FFI, 4=WILDX, 5=CAST, 6=ARITH
    std::string message;
    std::string suggestion;
    int32_t line, col;
    bool acknowledged;
};

struct SafetyState {
    int32_t level;  // 0=PERMISSIVE, 1=STRICT, 2=PARANOID
    std::vector<SafetyIssue> issues;
};

static const char* safety_op_names[] = {
    "wild memory", "async/concurrency", "pointer operation",
    "FFI/extern", "executable memory", "pointer cast", "pointer arithmetic"
};

static const char* safety_help_texts[] = {
    "wild memory must be manually freed - you are responsible for deallocation",
    "async code may race without proper synchronization",
    "pointer operations bypass bounds checking - ensure validity manually",
    "extern functions bypass Nitpick safety - validate all FFI boundaries",
    "executable memory has security implications - ensure proper validation",
    "pointer casts can violate type safety - verify correctness",
    "pointer arithmetic can cause buffer overflows - validate bounds"
};

extern "C" {

void* safety_new(int32_t level) {
    auto* s = new SafetyState();
    s->level = level;
    return s;
}

void safety_free(void* state) {
    delete (SafetyState*)state;
}

void safety_add_issue(void* state, int32_t operation,
                      const char* message, const char* suggestion,
                      int32_t line, int32_t col) {
    auto* s = (SafetyState*)state;
    SafetyIssue issue;
    issue.operation = operation;
    issue.message = message ? message : "";
    issue.suggestion = suggestion ? suggestion : "";
    issue.line = line;
    issue.col = col;
    issue.acknowledged = false;
    s->issues.push_back(issue);
}

int32_t safety_issue_count(void* state) {
    return (int32_t)((SafetyState*)state)->issues.size();
}

int32_t safety_has_errors(void* state) {
    auto* s = (SafetyState*)state;
    if (s->level == 0) return 0;  // PERMISSIVE: no errors
    for (auto& issue : s->issues) {
        if (!issue.acknowledged) return 1;
    }
    return 0;
}

AriaString* safety_get_issue_message(void* state, int32_t index) {
    auto* s = (SafetyState*)state;
    if (index < 0 || index >= (int32_t)s->issues.size())
        return make_aria_str("");
    return make_aria_str(s->issues[index].message);
}

int32_t safety_get_issue_op(void* state, int32_t index) {
    auto* s = (SafetyState*)state;
    if (index < 0 || index >= (int32_t)s->issues.size()) return -1;
    return s->issues[index].operation;
}

AriaString* safety_op_name(int32_t op) {
    if (op >= 0 && op < 7) return make_aria_str(safety_op_names[op]);
    return make_aria_str("unsafe operation");
}

AriaString* safety_help_text(int32_t op) {
    if (op >= 0 && op < 7) return make_aria_str(safety_help_texts[op]);
    return make_aria_str("see SAFETY.md for details");
}

} // extern "C" for safety

// ═══════════════════════════════════════════════════════════════════════
// EXHAUSTIVENESS CHECKER HELPERS
// ═══════════════════════════════════════════════════════════════════════

// Domain kinds
enum DomainKind : int32_t {
    DOMAIN_BOOLEAN = 0,
    DOMAIN_INT_RANGE = 1,
    DOMAIN_ENUM = 2,
    DOMAIN_INFINITE = 3,
    DOMAIN_UNKNOWN = 4
};

struct ValueRange {
    int64_t min, max;
};

struct CoverageSet {
    std::vector<ValueRange> ranges;
    std::set<std::string> symbols;
    bool has_default;
    bool has_err;
    bool has_unknown;
};

struct DomainInfo {
    DomainKind kind;
    int64_t range_min, range_max;
    bool requires_err;
    std::vector<std::string> enum_symbols;
};

static DomainInfo get_domain_for_type(const char* type_name) {
    DomainInfo d;
    d.requires_err = false;
    std::string name(type_name ? type_name : "");

    if (name == "bool") {
        d.kind = DOMAIN_BOOLEAN;
        d.range_min = 0; d.range_max = 1;
    } else if (name == "int8") {
        d.kind = DOMAIN_INT_RANGE;
        d.range_min = -128; d.range_max = 127;
    } else if (name == "uint8") {
        d.kind = DOMAIN_INT_RANGE;
        d.range_min = 0; d.range_max = 255;
    } else if (name == "int16") {
        d.kind = DOMAIN_INT_RANGE;
        d.range_min = -32768; d.range_max = 32767;
    } else if (name == "uint16") {
        d.kind = DOMAIN_INT_RANGE;
        d.range_min = 0; d.range_max = 65535;
    } else if (name == "tbb8") {
        d.kind = DOMAIN_INT_RANGE;
        d.range_min = -127; d.range_max = 127;
        d.requires_err = true;
    } else if (name == "tbb16") {
        d.kind = DOMAIN_INT_RANGE;
        d.range_min = -32767; d.range_max = 32767;
        d.requires_err = true;
    } else if (name == "tbb32") {
        d.kind = DOMAIN_INT_RANGE;
        d.range_min = -2147483647LL; d.range_max = 2147483647LL;
        d.requires_err = true;
    } else if (name == "tbb64") {
        d.kind = DOMAIN_INT_RANGE;
        d.range_min = -(int64_t)9223372036854775807LL;
        d.range_max = (int64_t)9223372036854775807LL;
        d.requires_err = true;
    } else {
        d.kind = DOMAIN_INFINITE;
        d.range_min = 0; d.range_max = 0;
    }
    return d;
}

extern "C" {

void* exhaust_new() {
    auto* c = new CoverageSet();
    c->has_default = false;
    c->has_err = false;
    c->has_unknown = false;
    return c;
}

void exhaust_free(void* cov) {
    delete (CoverageSet*)cov;
}

void exhaust_add_range(void* cov, int64_t min, int64_t max) {
    ((CoverageSet*)cov)->ranges.push_back({min, max});
}

void exhaust_add_symbol(void* cov, const char* symbol) {
    ((CoverageSet*)cov)->symbols.insert(symbol ? symbol : "");
}

void exhaust_add_default(void* cov) {
    ((CoverageSet*)cov)->has_default = true;
}

void exhaust_add_err(void* cov) {
    ((CoverageSet*)cov)->has_err = true;
}

void exhaust_add_unknown(void* cov) {
    ((CoverageSet*)cov)->has_unknown = true;
}

// Get domain kind for a type name
int32_t exhaust_domain_kind(const char* type_name) {
    return (int32_t)get_domain_for_type(type_name).kind;
}

int64_t exhaust_domain_min(const char* type_name) {
    return get_domain_for_type(type_name).range_min;
}

int64_t exhaust_domain_max(const char* type_name) {
    return get_domain_for_type(type_name).range_max;
}

int32_t exhaust_domain_requires_err(const char* type_name) {
    return get_domain_for_type(type_name).requires_err ? 1 : 0;
}

// Check if coverage is exhaustive for the given type
int32_t exhaust_is_exhaustive(void* cov, const char* type_name) {
    auto* c = (CoverageSet*)cov;
    if (c->has_default) return 1;

    auto domain = get_domain_for_type(type_name);

    switch (domain.kind) {
        case DOMAIN_BOOLEAN:
            return (c->symbols.count("true") && c->symbols.count("false")) ? 1 : 0;

        case DOMAIN_INT_RANGE: {
            // Sort ranges and check for complete coverage
            auto sorted = c->ranges;
            std::sort(sorted.begin(), sorted.end(),
                [](const ValueRange& a, const ValueRange& b) { return a.min < b.min; });

            int64_t current = domain.range_min;
            for (auto& r : sorted) {
                if (r.min > current) return 0;  // Gap found
                if (r.max >= current) current = r.max + 1;
            }
            bool ranges_covered = (current > domain.range_max);

            if (domain.requires_err) {
                return (ranges_covered && c->has_err) ? 1 : 0;
            }
            return ranges_covered ? 1 : 0;
        }

        case DOMAIN_ENUM:
            // Would need enum variant names — check symbol coverage
            return 0;  // Cannot determine without enum info

        case DOMAIN_INFINITE:
            return 0;

        case DOMAIN_UNKNOWN:
            return 0;
    }
    return 0;
}

// Get missing case description
AriaString* exhaust_missing_message(void* cov, const char* type_name) {
    auto* c = (CoverageSet*)cov;
    auto domain = get_domain_for_type(type_name);

    std::ostringstream msg;
    msg << "Missing cases: ";
    bool first = true;

    if (domain.requires_err && !c->has_err) {
        msg << "ERR";
        first = false;
    }

    if (domain.kind == DOMAIN_BOOLEAN) {
        if (!c->symbols.count("true")) {
            if (!first) msg << ", ";
            msg << "true";
            first = false;
        }
        if (!c->symbols.count("false")) {
            if (!first) msg << ", ";
            msg << "false";
            first = false;
        }
    }

    if (domain.kind == DOMAIN_INT_RANGE) {
        auto sorted = c->ranges;
        std::sort(sorted.begin(), sorted.end(),
            [](const ValueRange& a, const ValueRange& b) { return a.min < b.min; });

        int64_t current = domain.range_min;
        int gaps = 0;
        for (auto& r : sorted) {
            if (r.min > current && gaps < 3) {
                if (!first) msg << ", ";
                int64_t gap_end = r.min - 1;
                if (current == gap_end) msg << current;
                else msg << current << ".." << gap_end;
                first = false;
                gaps++;
            }
            if (r.max >= current) current = r.max + 1;
        }
        if (current <= domain.range_max && gaps < 3) {
            if (!first) msg << ", ";
            if (current == domain.range_max) msg << current;
            else msg << current << ".." << domain.range_max;
        }
    }

    return make_aria_str(msg.str());
}

} // extern "C" for exhaustiveness

// ═══════════════════════════════════════════════════════════════════════
// CONST EVALUATOR HELPERS
// ═══════════════════════════════════════════════════════════════════════

enum ValKind : int32_t {
    VAL_INT = 0,
    VAL_FLOAT = 1,
    VAL_BOOL = 2,
    VAL_STRING = 3,
    VAL_ERR = 4,
    VAL_NULL = 5
};

struct ComptimeValue {
    ValKind kind;
    int64_t int_val;
    double float_val;
    bool bool_val;
    std::string str_val;
};

extern "C" {

void* consteval_make_int(int64_t val) {
    auto* v = new ComptimeValue();
    v->kind = VAL_INT;
    v->int_val = val;
    v->float_val = 0;
    v->bool_val = false;
    return v;
}

void* consteval_make_float(double val) {
    auto* v = new ComptimeValue();
    v->kind = VAL_FLOAT;
    v->float_val = val;
    v->int_val = 0;
    v->bool_val = false;
    return v;
}

void* consteval_make_bool(int32_t val) {
    auto* v = new ComptimeValue();
    v->kind = VAL_BOOL;
    v->bool_val = (val != 0);
    v->int_val = 0;
    v->float_val = 0;
    return v;
}

void* consteval_make_string(const char* val) {
    auto* v = new ComptimeValue();
    v->kind = VAL_STRING;
    v->str_val = val ? val : "";
    v->int_val = 0;
    v->float_val = 0;
    v->bool_val = false;
    return v;
}

void* consteval_make_err() {
    auto* v = new ComptimeValue();
    v->kind = VAL_ERR;
    v->int_val = 0;
    v->float_val = 0;
    v->bool_val = false;
    return v;
}

void* consteval_make_null() {
    auto* v = new ComptimeValue();
    v->kind = VAL_NULL;
    v->int_val = 0;
    v->float_val = 0;
    v->bool_val = false;
    return v;
}

void consteval_free(void* val) {
    delete (ComptimeValue*)val;
}

int32_t consteval_val_kind(void* val) {
    if (!val) return VAL_NULL;
    return (int32_t)((ComptimeValue*)val)->kind;
}

int64_t consteval_val_int(void* val) {
    if (!val) return 0;
    auto* v = (ComptimeValue*)val;
    if (v->kind == VAL_INT) return v->int_val;
    if (v->kind == VAL_BOOL) return v->bool_val ? 1 : 0;
    if (v->kind == VAL_FLOAT) return (int64_t)v->float_val;
    return 0;
}

double consteval_val_float(void* val) {
    if (!val) return 0.0;
    auto* v = (ComptimeValue*)val;
    if (v->kind == VAL_FLOAT) return v->float_val;
    if (v->kind == VAL_INT) return (double)v->int_val;
    return 0.0;
}

int32_t consteval_val_bool(void* val) {
    if (!val) return 0;
    auto* v = (ComptimeValue*)val;
    if (v->kind == VAL_BOOL) return v->bool_val ? 1 : 0;
    if (v->kind == VAL_INT) return v->int_val != 0 ? 1 : 0;
    return 0;
}

AriaString* consteval_val_string(void* val) {
    if (!val) return make_aria_str("");
    auto* v = (ComptimeValue*)val;
    if (v->kind == VAL_STRING) return make_aria_str(v->str_val);
    // Convert other types to string
    switch (v->kind) {
        case VAL_INT: return make_aria_str(std::to_string(v->int_val));
        case VAL_FLOAT: return make_aria_str(std::to_string(v->float_val));
        case VAL_BOOL: return make_aria_str(v->bool_val ? "true" : "false");
        case VAL_ERR: return make_aria_str("ERR");
        case VAL_NULL: return make_aria_str("NIL");
        default: return make_aria_str("");
    }
}

// Arithmetic: returns new value (caller must free)
void* consteval_add(void* a, void* b) {
    if (!a || !b) return consteval_make_err();
    auto* va = (ComptimeValue*)a;
    auto* vb = (ComptimeValue*)b;
    if (va->kind == VAL_ERR || vb->kind == VAL_ERR) return consteval_make_err();
    if (va->kind == VAL_INT && vb->kind == VAL_INT)
        return consteval_make_int(va->int_val + vb->int_val);
    if (va->kind == VAL_FLOAT || vb->kind == VAL_FLOAT) {
        double fa = (va->kind == VAL_FLOAT) ? va->float_val : (double)va->int_val;
        double fb = (vb->kind == VAL_FLOAT) ? vb->float_val : (double)vb->int_val;
        return consteval_make_float(fa + fb);
    }
    if (va->kind == VAL_STRING && vb->kind == VAL_STRING)
        return consteval_make_string((va->str_val + vb->str_val).c_str());
    return consteval_make_err();
}

void* consteval_sub(void* a, void* b) {
    if (!a || !b) return consteval_make_err();
    auto* va = (ComptimeValue*)a;
    auto* vb = (ComptimeValue*)b;
    if (va->kind == VAL_ERR || vb->kind == VAL_ERR) return consteval_make_err();
    if (va->kind == VAL_INT && vb->kind == VAL_INT)
        return consteval_make_int(va->int_val - vb->int_val);
    if (va->kind == VAL_FLOAT || vb->kind == VAL_FLOAT) {
        double fa = (va->kind == VAL_FLOAT) ? va->float_val : (double)va->int_val;
        double fb = (vb->kind == VAL_FLOAT) ? vb->float_val : (double)vb->int_val;
        return consteval_make_float(fa - fb);
    }
    return consteval_make_err();
}

void* consteval_mul(void* a, void* b) {
    if (!a || !b) return consteval_make_err();
    auto* va = (ComptimeValue*)a;
    auto* vb = (ComptimeValue*)b;
    if (va->kind == VAL_ERR || vb->kind == VAL_ERR) return consteval_make_err();
    if (va->kind == VAL_INT && vb->kind == VAL_INT)
        return consteval_make_int(va->int_val * vb->int_val);
    if (va->kind == VAL_FLOAT || vb->kind == VAL_FLOAT) {
        double fa = (va->kind == VAL_FLOAT) ? va->float_val : (double)va->int_val;
        double fb = (vb->kind == VAL_FLOAT) ? vb->float_val : (double)vb->int_val;
        return consteval_make_float(fa * fb);
    }
    return consteval_make_err();
}

void* consteval_div(void* a, void* b) {
    if (!a || !b) return consteval_make_err();
    auto* va = (ComptimeValue*)a;
    auto* vb = (ComptimeValue*)b;
    if (va->kind == VAL_ERR || vb->kind == VAL_ERR) return consteval_make_err();
    if (va->kind == VAL_INT && vb->kind == VAL_INT) {
        if (vb->int_val == 0) return consteval_make_err();
        return consteval_make_int(va->int_val / vb->int_val);
    }
    if (va->kind == VAL_FLOAT || vb->kind == VAL_FLOAT) {
        double fa = (va->kind == VAL_FLOAT) ? va->float_val : (double)va->int_val;
        double fb = (vb->kind == VAL_FLOAT) ? vb->float_val : (double)vb->int_val;
        if (fb == 0.0) return consteval_make_err();
        return consteval_make_float(fa / fb);
    }
    return consteval_make_err();
}

void* consteval_mod(void* a, void* b) {
    if (!a || !b) return consteval_make_err();
    auto* va = (ComptimeValue*)a;
    auto* vb = (ComptimeValue*)b;
    if (va->kind == VAL_ERR || vb->kind == VAL_ERR) return consteval_make_err();
    if (va->kind == VAL_INT && vb->kind == VAL_INT) {
        if (vb->int_val == 0) return consteval_make_err();
        return consteval_make_int(va->int_val % vb->int_val);
    }
    if (va->kind == VAL_FLOAT || vb->kind == VAL_FLOAT) {
        double fa = (va->kind == VAL_FLOAT) ? va->float_val : (double)va->int_val;
        double fb = (vb->kind == VAL_FLOAT) ? vb->float_val : (double)vb->int_val;
        if (fb == 0.0) return consteval_make_err();
        return consteval_make_float(fmod(fa, fb));
    }
    return consteval_make_err();
}

void* consteval_neg(void* val) {
    if (!val) return consteval_make_err();
    auto* v = (ComptimeValue*)val;
    if (v->kind == VAL_ERR) return consteval_make_err();
    if (v->kind == VAL_INT) return consteval_make_int(-v->int_val);
    if (v->kind == VAL_FLOAT) return consteval_make_float(-v->float_val);
    return consteval_make_err();
}

// TBB arithmetic with sticky error propagation
void* consteval_tbb_add(void* a, void* b, int32_t bits) {
    if (!a || !b) return consteval_make_err();
    auto* va = (ComptimeValue*)a;
    auto* vb = (ComptimeValue*)b;
    if (va->kind == VAL_ERR || vb->kind == VAL_ERR) return consteval_make_err();

    int64_t result = va->int_val + vb->int_val;
    int64_t max_val = (1LL << (bits - 1)) - 1;
    int64_t min_val = -max_val;
    int64_t err_sentinel = min_val - 1;  // e.g., -128 for tbb8

    if (va->int_val == err_sentinel || vb->int_val == err_sentinel)
        return consteval_make_err();
    if (result > max_val || result < min_val)
        return consteval_make_err();
    return consteval_make_int(result);
}

void* consteval_tbb_sub(void* a, void* b, int32_t bits) {
    if (!a || !b) return consteval_make_err();
    auto* va = (ComptimeValue*)a;
    auto* vb = (ComptimeValue*)b;
    if (va->kind == VAL_ERR || vb->kind == VAL_ERR) return consteval_make_err();

    int64_t result = va->int_val - vb->int_val;
    int64_t max_val = (1LL << (bits - 1)) - 1;
    int64_t min_val = -max_val;
    int64_t err_sentinel = min_val - 1;

    if (va->int_val == err_sentinel || vb->int_val == err_sentinel)
        return consteval_make_err();
    if (result > max_val || result < min_val)
        return consteval_make_err();
    return consteval_make_int(result);
}

void* consteval_tbb_mul(void* a, void* b, int32_t bits) {
    if (!a || !b) return consteval_make_err();
    auto* va = (ComptimeValue*)a;
    auto* vb = (ComptimeValue*)b;
    if (va->kind == VAL_ERR || vb->kind == VAL_ERR) return consteval_make_err();

    int64_t result = va->int_val * vb->int_val;
    int64_t max_val = (1LL << (bits - 1)) - 1;
    int64_t min_val = -max_val;
    int64_t err_sentinel = min_val - 1;

    if (va->int_val == err_sentinel || vb->int_val == err_sentinel)
        return consteval_make_err();
    if (result > max_val || result < min_val)
        return consteval_make_err();
    return consteval_make_int(result);
}

// Comparison: returns 1 for true, 0 for false, -1 for incomparable
int32_t consteval_compare(void* a, void* b, int32_t op) {
    // op: 0=EQ, 1=NE, 2=LT, 3=LE, 4=GT, 5=GE
    if (!a || !b) return 0;
    auto* va = (ComptimeValue*)a;
    auto* vb = (ComptimeValue*)b;
    if (va->kind == VAL_ERR || vb->kind == VAL_ERR) return 0;

    double fa = 0, fb = 0;
    bool comparable = false;

    if (va->kind == VAL_INT && vb->kind == VAL_INT) {
        fa = (double)va->int_val;
        fb = (double)vb->int_val;
        comparable = true;
    } else if ((va->kind == VAL_INT || va->kind == VAL_FLOAT) &&
               (vb->kind == VAL_INT || vb->kind == VAL_FLOAT)) {
        fa = (va->kind == VAL_FLOAT) ? va->float_val : (double)va->int_val;
        fb = (vb->kind == VAL_FLOAT) ? vb->float_val : (double)vb->int_val;
        comparable = true;
    } else if (va->kind == VAL_BOOL && vb->kind == VAL_BOOL) {
        fa = va->bool_val ? 1.0 : 0.0;
        fb = vb->bool_val ? 1.0 : 0.0;
        comparable = true;
    } else if (va->kind == VAL_STRING && vb->kind == VAL_STRING) {
        int cmp = va->str_val.compare(vb->str_val);
        switch (op) {
            case 0: return (cmp == 0) ? 1 : 0;
            case 1: return (cmp != 0) ? 1 : 0;
            case 2: return (cmp < 0) ? 1 : 0;
            case 3: return (cmp <= 0) ? 1 : 0;
            case 4: return (cmp > 0) ? 1 : 0;
            case 5: return (cmp >= 0) ? 1 : 0;
        }
        return 0;
    }

    if (!comparable) return 0;

    switch (op) {
        case 0: return (fa == fb) ? 1 : 0;
        case 1: return (fa != fb) ? 1 : 0;
        case 2: return (fa < fb) ? 1 : 0;
        case 3: return (fa <= fb) ? 1 : 0;
        case 4: return (fa > fb) ? 1 : 0;
        case 5: return (fa >= fb) ? 1 : 0;
    }
    return 0;
}

int32_t consteval_logical_and(void* a, void* b) {
    return (consteval_val_bool(a) && consteval_val_bool(b)) ? 1 : 0;
}

int32_t consteval_logical_or(void* a, void* b) {
    return (consteval_val_bool(a) || consteval_val_bool(b)) ? 1 : 0;
}

int32_t consteval_logical_not(void* val) {
    return consteval_val_bool(val) ? 0 : 1;
}

} // extern "C" for consteval

// ═══════════════════════════════════════════════════════════════════════
// BORROW CHECKER HELPERS
// ═══════════════════════════════════════════════════════════════════════

enum WildState : int32_t {
    WILD_UNINITIALIZED = 0,
    WILD_ALLOCATED = 1,
    WILD_FREED = 2,
    WILD_MOVED = 3,
    WILD_MAY_FREED = 4,
    WILD_UNKNOWN = 5
};

struct Loan {
    std::string borrower;
    bool is_mutable;
    int32_t line, col;
};

struct VarInfo {
    int32_t scope_depth;
    bool is_wild;
    WildState wild_state;
    bool is_moved;
    bool is_initialized;
};

struct BorrowState {
    int32_t scope_depth;
    std::map<std::string, VarInfo> variables;
    std::map<std::string, std::vector<Loan>> active_loans;
    std::map<std::string, std::string> active_pins;  // pinned → pinner
    std::set<std::string> pending_wild_frees;
    std::vector<std::string> errors;
};

extern "C" {

void* borrow_new() {
    auto* s = new BorrowState();
    s->scope_depth = 0;
    return s;
}

void borrow_free(void* state) {
    delete (BorrowState*)state;
}

void borrow_enter_scope(void* state) {
    ((BorrowState*)state)->scope_depth++;
}

void borrow_exit_scope(void* state) {
    auto* s = (BorrowState*)state;

    // Check for unfreed wild variables at this scope depth
    for (auto& [name, info] : s->variables) {
        if (info.scope_depth == s->scope_depth && info.is_wild) {
            if (info.wild_state == WILD_ALLOCATED) {
                s->errors.push_back("Memory leak: wild variable '" + name +
                    "' was not freed before scope exit");
                s->pending_wild_frees.erase(name);
            }
        }
    }

    // Release loans from variables at this depth
    std::vector<std::string> to_remove;
    for (auto& [host, loans] : s->active_loans) {
        loans.erase(
            std::remove_if(loans.begin(), loans.end(),
                [&](const Loan& l) {
                    auto it = s->variables.find(l.borrower);
                    return it != s->variables.end() &&
                           it->second.scope_depth == s->scope_depth;
                }),
            loans.end());
        if (loans.empty()) to_remove.push_back(host);
    }
    for (auto& h : to_remove) s->active_loans.erase(h);

    // Remove variables at this depth
    for (auto it = s->variables.begin(); it != s->variables.end(); ) {
        if (it->second.scope_depth == s->scope_depth)
            it = s->variables.erase(it);
        else ++it;
    }

    s->scope_depth--;
}

void borrow_register_var(void* state, const char* name, int32_t is_wild) {
    auto* s = (BorrowState*)state;
    VarInfo info;
    info.scope_depth = s->scope_depth;
    info.is_wild = (is_wild != 0);
    info.wild_state = is_wild ? WILD_UNINITIALIZED : WILD_UNKNOWN;
    info.is_moved = false;
    info.is_initialized = true;
    s->variables[name] = info;
    if (is_wild) {
        s->pending_wild_frees.insert(name);
    }
}

void borrow_mark_allocated(void* state, const char* name) {
    auto* s = (BorrowState*)state;
    auto it = s->variables.find(name);
    if (it != s->variables.end()) {
        it->second.wild_state = WILD_ALLOCATED;
    }
}

void borrow_mark_freed(void* state, const char* name) {
    auto* s = (BorrowState*)state;
    auto it = s->variables.find(name);
    if (it != s->variables.end()) {
        if (it->second.wild_state == WILD_FREED) {
            s->errors.push_back("Double free: variable '" + std::string(name) +
                "' was already freed");
        }
        it->second.wild_state = WILD_FREED;
        s->pending_wild_frees.erase(name);
    }
}

void borrow_mark_moved(void* state, const char* name) {
    auto* s = (BorrowState*)state;
    auto it = s->variables.find(name);
    if (it != s->variables.end()) {
        it->second.is_moved = true;
        it->second.wild_state = WILD_MOVED;
    }
}

// Record a borrow. Returns 0 on success, 1 on conflict.
int32_t borrow_record_borrow(void* state, const char* borrower,
                              const char* host, int32_t is_mutable,
                              int32_t line, int32_t col) {
    auto* s = (BorrowState*)state;
    auto& loans = s->active_loans[host];

    // Check: if mutable, no other borrows allowed
    if (is_mutable) {
        if (!loans.empty()) {
            std::string existing = loans[0].borrower;
            s->errors.push_back("Cannot borrow '" + std::string(host) +
                "' as mutable: already borrowed by '" + existing + "'");
            return 1;
        }
    } else {
        // Immutable: no mutable borrows allowed
        for (auto& loan : loans) {
            if (loan.is_mutable) {
                s->errors.push_back("Cannot borrow '" + std::string(host) +
                    "' as immutable: already mutably borrowed by '" +
                    loan.borrower + "'");
                return 1;
            }
        }
    }

    Loan loan;
    loan.borrower = borrower;
    loan.is_mutable = (is_mutable != 0);
    loan.line = line;
    loan.col = col;
    loans.push_back(loan);
    return 0;
}

int32_t borrow_record_pin(void* state, const char* pinner,
                           const char* host, int32_t line, int32_t col) {
    auto* s = (BorrowState*)state;
    auto it = s->active_pins.find(host);
    if (it != s->active_pins.end()) {
        s->errors.push_back("Cannot pin '" + std::string(host) +
            "': already pinned by '" + it->second + "'");
        return 1;
    }
    s->active_pins[host] = pinner;
    return 0;
}

int32_t borrow_is_borrowed(void* state, const char* name) {
    auto* s = (BorrowState*)state;
    auto it = s->active_loans.find(name);
    return (it != s->active_loans.end() && !it->second.empty()) ? 1 : 0;
}

int32_t borrow_is_pinned(void* state, const char* name) {
    return ((BorrowState*)state)->active_pins.count(name) ? 1 : 0;
}

int32_t borrow_is_moved(void* state, const char* name) {
    auto* s = (BorrowState*)state;
    auto it = s->variables.find(name);
    return (it != s->variables.end() && it->second.is_moved) ? 1 : 0;
}

int32_t borrow_wild_state(void* state, const char* name) {
    auto* s = (BorrowState*)state;
    auto it = s->variables.find(name);
    if (it == s->variables.end()) return WILD_UNKNOWN;
    return (int32_t)it->second.wild_state;
}

// Check use-after-move
int32_t borrow_check_use(void* state, const char* name, int32_t line, int32_t col) {
    auto* s = (BorrowState*)state;
    auto it = s->variables.find(name);
    if (it == s->variables.end()) return 0;  // Not tracked

    if (it->second.is_moved) {
        s->errors.push_back("Use after move: variable '" + std::string(name) +
            "' has been moved");
        return 1;
    }
    if (it->second.wild_state == WILD_FREED) {
        s->errors.push_back("Use after free: variable '" + std::string(name) +
            "' has been freed");
        return 1;
    }
    return 0;
}

int32_t borrow_error_count(void* state) {
    return (int32_t)((BorrowState*)state)->errors.size();
}

AriaString* borrow_get_error(void* state, int32_t index) {
    auto* s = (BorrowState*)state;
    if (index < 0 || index >= (int32_t)s->errors.size())
        return make_aria_str("");
    return make_aria_str(s->errors[index]);
}

int32_t borrow_pending_free_count(void* state) {
    return (int32_t)((BorrowState*)state)->pending_wild_frees.size();
}

void borrow_add_error(void* state, const char* msg) {
    ((BorrowState*)state)->errors.push_back(msg ? msg : "");
}

} // extern "C" for borrow checker
