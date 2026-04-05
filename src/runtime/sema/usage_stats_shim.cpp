// ═══════════════════════════════════════════════════════════════════════
// usage_stats_shim.cpp — C bridge for the self-hosted Usage Stats
// v0.15.1 Port 9
// ═══════════════════════════════════════════════════════════════════════
// Exposes: telemetry counters, feature tracking, system info, session
//          management. Networking is stubbed (no real HTTP POST).
// ═══════════════════════════════════════════════════════════════════════

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>

#ifdef __linux__
#include <sys/utsname.h>
#include <unistd.h>
#endif

struct USState {
    // Session
    int64_t session_id = 0;
    bool enabled       = true;

    // Atomic counters
    std::atomic<int64_t> compilations{0};
    std::atomic<int64_t> ffi_calls{0};
    std::atomic<int64_t> int128_usage{0};
    std::atomic<int64_t> fix256_usage{0};
    std::atomic<int64_t> async_tasks{0};
    std::atomic<int64_t> memory_allocs{0};
    std::atomic<int64_t> panics{0};

    // Features list
    std::mutex features_mu;
    std::vector<std::string> features_used;

    // System info (populated on init)
    std::string os_name;
    std::string cpu_arch;
    std::string compiler_ver;
    int32_t total_memory_mb = 0;
    int32_t cpu_cores       = 0;

    // Flush history (stub — counts how many times flush was called)
    int32_t flush_count = 0;
};

static thread_local std::string tls_buf;
static const char* tls_ret(const std::string& s) {
    tls_buf = s;
    return tls_buf.c_str();
}

extern "C" {

// ─── Lifecycle ───────────────────────────────────────────────────────
void* us_new() {
    auto* s = new USState;
    // Generate session ID from monotonic clock
    auto now = std::chrono::steady_clock::now();
    s->session_id = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();

    // Populate system info
#ifdef __linux__
    struct utsname uts;
    if (uname(&uts) == 0) {
        s->os_name  = uts.sysname;
        s->cpu_arch = uts.machine;
    }
    long pages    = sysconf(_SC_PHYS_PAGES);
    long page_sz  = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_sz > 0)
        s->total_memory_mb = static_cast<int32_t>((pages * page_sz) / (1024 * 1024));
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (cores > 0) s->cpu_cores = static_cast<int32_t>(cores);
#else
    s->os_name  = "unknown";
    s->cpu_arch = "unknown";
#endif
    s->compiler_ver = "ariac-0.15.1";
    return s;
}

int32_t us_free(void* state) {
    delete static_cast<USState*>(state);
    return 0;
}

void* us_null_ptr() { return nullptr; }
int32_t us_release(void*) { return 0; }

// ─── Session ─────────────────────────────────────────────────────────
int64_t us_session_id(void* state) {
    return static_cast<USState*>(state)->session_id;
}

int32_t us_set_enabled(void* state, int32_t flag) {
    static_cast<USState*>(state)->enabled = (flag != 0);
    return 0;
}

int32_t us_is_enabled(void* state) {
    return static_cast<USState*>(state)->enabled ? 1 : 0;
}

// ─── Record events (increment counters) ──────────────────────────────
int32_t us_record_compilation(void* state) {
    auto* s = static_cast<USState*>(state);
    if (!s->enabled) return 0;
    s->compilations.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int32_t us_record_ffi(void* state) {
    auto* s = static_cast<USState*>(state);
    if (!s->enabled) return 0;
    s->ffi_calls.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int32_t us_record_type(void* state, int32_t type_kind) {
    auto* s = static_cast<USState*>(state);
    if (!s->enabled) return 0;
    // 0 = int128, 1 = fix256
    if (type_kind == 0) s->int128_usage.fetch_add(1, std::memory_order_relaxed);
    else if (type_kind == 1) s->fix256_usage.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int32_t us_record_async(void* state) {
    auto* s = static_cast<USState*>(state);
    if (!s->enabled) return 0;
    s->async_tasks.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int32_t us_record_alloc(void* state) {
    auto* s = static_cast<USState*>(state);
    if (!s->enabled) return 0;
    s->memory_allocs.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

int32_t us_record_panic(void* state) {
    auto* s = static_cast<USState*>(state);
    if (!s->enabled) return 0;
    s->panics.fetch_add(1, std::memory_order_relaxed);
    return 0;
}

// ─── Feature tracking ────────────────────────────────────────────────
int32_t us_record_feature(void* state, const char* feature) {
    auto* s = static_cast<USState*>(state);
    if (!s->enabled || !feature) return 0;
    std::lock_guard<std::mutex> lk(s->features_mu);
    s->features_used.push_back(feature);
    return 0;
}

int32_t us_feature_count(void* state) {
    auto* s = static_cast<USState*>(state);
    std::lock_guard<std::mutex> lk(s->features_mu);
    return static_cast<int32_t>(s->features_used.size());
}

const char* us_get_feature(void* state, int32_t idx) {
    auto* s = static_cast<USState*>(state);
    std::lock_guard<std::mutex> lk(s->features_mu);
    if (idx < 0 || idx >= (int32_t)s->features_used.size()) return tls_ret("");
    return tls_ret(s->features_used[idx]);
}

// ─── Counter queries ─────────────────────────────────────────────────
int64_t us_get_compilations(void* state) {
    return static_cast<USState*>(state)->compilations.load(std::memory_order_relaxed);
}
int64_t us_get_ffi_calls(void* state) {
    return static_cast<USState*>(state)->ffi_calls.load(std::memory_order_relaxed);
}
int64_t us_get_int128_usage(void* state) {
    return static_cast<USState*>(state)->int128_usage.load(std::memory_order_relaxed);
}
int64_t us_get_fix256_usage(void* state) {
    return static_cast<USState*>(state)->fix256_usage.load(std::memory_order_relaxed);
}
int64_t us_get_async_tasks(void* state) {
    return static_cast<USState*>(state)->async_tasks.load(std::memory_order_relaxed);
}
int64_t us_get_memory_allocs(void* state) {
    return static_cast<USState*>(state)->memory_allocs.load(std::memory_order_relaxed);
}
int64_t us_get_panics(void* state) {
    return static_cast<USState*>(state)->panics.load(std::memory_order_relaxed);
}

// ─── System info ─────────────────────────────────────────────────────
const char* us_os_name(void* state) {
    return tls_ret(static_cast<USState*>(state)->os_name);
}
const char* us_cpu_arch(void* state) {
    return tls_ret(static_cast<USState*>(state)->cpu_arch);
}
const char* us_compiler_version(void* state) {
    return tls_ret(static_cast<USState*>(state)->compiler_ver);
}
int32_t us_total_memory_mb(void* state) {
    return static_cast<USState*>(state)->total_memory_mb;
}
int32_t us_cpu_cores(void* state) {
    return static_cast<USState*>(state)->cpu_cores;
}

// ─── Flush (stub — no real network, just increments counter) ────────
int32_t us_flush(void* state) {
    auto* s = static_cast<USState*>(state);
    if (!s->enabled) return 0;
    s->flush_count++;
    return 0;
}

int32_t us_flush_count(void* state) {
    return static_cast<USState*>(state)->flush_count;
}

// ─── Reset counters ──────────────────────────────────────────────────
int32_t us_reset(void* state) {
    auto* s = static_cast<USState*>(state);
    s->compilations.store(0, std::memory_order_relaxed);
    s->ffi_calls.store(0, std::memory_order_relaxed);
    s->int128_usage.store(0, std::memory_order_relaxed);
    s->fix256_usage.store(0, std::memory_order_relaxed);
    s->async_tasks.store(0, std::memory_order_relaxed);
    s->memory_allocs.store(0, std::memory_order_relaxed);
    s->panics.store(0, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lk(s->features_mu);
        s->features_used.clear();
    }
    s->flush_count = 0;
    return 0;
}

} // extern "C"
