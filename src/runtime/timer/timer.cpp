/**
 * Aria Timer/Clock Library Implementation - Phase 5.9
 * 
 * Cross-platform high-resolution timing with TBB64 arithmetic.
 * Provides monotonic clock, wall-clock time, and sleep primitives.
 */

#include "runtime/timer.h"
#include <cstdlib>
#include <cstring>

// Platform-specific includes
#ifdef __linux__
    #include <time.h>
    #include <unistd.h>
#elif defined(__APPLE__)
    #include <mach/mach_time.h>
    #include <sys/time.h>
    #include <unistd.h>
#elif defined(_WIN32)
    #include <windows.h>
#endif

/* ============================================================================
 * Internal Type Definitions
 * ============================================================================ */

struct AriaDuration {
    int64_t nanos;  // TBB64 nanoseconds
};

struct AriaInstant {
    int64_t nanos;  // Platform-specific monotonic time in nanoseconds
};

struct AriaSystemTime {
    int64_t nanos;  // Unix timestamp in nanoseconds
};

/* ============================================================================
 * TBB64 Arithmetic Helpers
 * ============================================================================ */

// Check if value is ERR sentinel
static inline bool is_time_err(int64_t value) {
    return value == ARIA_TIME_ERR;
}

// TBB64 addition with overflow detection
static inline int64_t tbb64_add(int64_t a, int64_t b) {
    if (is_time_err(a) || is_time_err(b)) return ARIA_TIME_ERR;
    
    // Check for overflow using builtin if available
    #if defined(__has_builtin) && __has_builtin(__builtin_add_overflow)
        int64_t result;
        if (__builtin_add_overflow(a, b, &result)) return ARIA_TIME_ERR;
        if (result == ARIA_TIME_ERR) return ARIA_TIME_ERR;
        return result;
    #else
        // Manual overflow check
        if (b > 0 && a > ARIA_TIME_MAX - b) return ARIA_TIME_ERR;
        if (b < 0 && a < ARIA_TIME_MIN - b) return ARIA_TIME_ERR;
        int64_t result = a + b;
        if (result == ARIA_TIME_ERR) return ARIA_TIME_ERR;
        return result;
    #endif
}

// TBB64 subtraction with underflow detection
static inline int64_t tbb64_sub(int64_t a, int64_t b) {
    if (is_time_err(a) || is_time_err(b)) return ARIA_TIME_ERR;
    
    #if defined(__has_builtin) && __has_builtin(__builtin_sub_overflow)
        int64_t result;
        if (__builtin_sub_overflow(a, b, &result)) return ARIA_TIME_ERR;
        if (result == ARIA_TIME_ERR) return ARIA_TIME_ERR;
        return result;
    #else
        if (b < 0 && a > ARIA_TIME_MAX + b) return ARIA_TIME_ERR;
        if (b > 0 && a < ARIA_TIME_MIN + b) return ARIA_TIME_ERR;
        int64_t result = a - b;
        if (result == ARIA_TIME_ERR) return ARIA_TIME_ERR;
        return result;
    #endif
}

// TBB64 multiplication with overflow detection
static inline int64_t tbb64_mul(int64_t a, int64_t b) {
    if (is_time_err(a) || is_time_err(b)) return ARIA_TIME_ERR;
    if (b == 0) return 0;
    
    #if defined(__has_builtin) && __has_builtin(__builtin_mul_overflow)
        int64_t result;
        if (__builtin_mul_overflow(a, b, &result)) return ARIA_TIME_ERR;
        if (result == ARIA_TIME_ERR) return ARIA_TIME_ERR;
        return result;
    #else
        // Check overflow before multiplication
        if (a > 0 && b > 0 && a > ARIA_TIME_MAX / b) return ARIA_TIME_ERR;
        if (a > 0 && b < 0 && b < ARIA_TIME_MIN / a) return ARIA_TIME_ERR;
        if (a < 0 && b > 0 && a < ARIA_TIME_MIN / b) return ARIA_TIME_ERR;
        if (a < 0 && b < 0 && a < ARIA_TIME_MAX / b) return ARIA_TIME_ERR;
        int64_t result = a * b;
        if (result == ARIA_TIME_ERR) return ARIA_TIME_ERR;
        return result;
    #endif
}

/* ============================================================================
 * Platform-Specific Clock Access
 * ============================================================================ */

// Get monotonic time in nanoseconds
static int64_t platform_monotonic_nanos(void) {
    #ifdef __linux__
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
            return ARIA_TIME_ERR;
        }
        int64_t secs_ns = tbb64_mul(ts.tv_sec, ARIA_SECOND);
        return tbb64_add(secs_ns, ts.tv_nsec);
        
    #elif defined(__APPLE__)
        static mach_timebase_info_data_t timebase = {0, 0};
        if (timebase.denom == 0) {
            mach_timebase_info(&timebase);
        }
        uint64_t abs_time = mach_absolute_time();
        // Convert to nanoseconds: (abs_time * numer) / denom
        return (int64_t)((abs_time * timebase.numer) / timebase.denom);
        
    #elif defined(_WIN32)
        static LARGE_INTEGER frequency = {0};
        if (frequency.QuadPart == 0) {
            QueryPerformanceFrequency(&frequency);
        }
        LARGE_INTEGER counter;
        if (!QueryPerformanceCounter(&counter)) {
            return ARIA_TIME_ERR;
        }
        // Convert to nanoseconds: (counter * 1e9) / frequency
        return (int64_t)((counter.QuadPart * ARIA_SECOND) / frequency.QuadPart);
        
    #else
        #error "Unsupported platform for monotonic clock"
    #endif
}

// Get wall-clock time in nanoseconds since Unix epoch
static int64_t platform_realtime_nanos(void) {
    #ifdef __linux__
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
            return ARIA_TIME_ERR;
        }
        int64_t secs_ns = tbb64_mul(ts.tv_sec, ARIA_SECOND);
        return tbb64_add(secs_ns, ts.tv_nsec);
        
    #elif defined(__APPLE__)
        struct timeval tv;
        if (gettimeofday(&tv, NULL) != 0) {
            return ARIA_TIME_ERR;
        }
        int64_t secs_ns = tbb64_mul(tv.tv_sec, ARIA_SECOND);
        int64_t usecs_ns = tbb64_mul(tv.tv_usec, ARIA_MICROSECOND);
        return tbb64_add(secs_ns, usecs_ns);
        
    #elif defined(_WIN32)
        FILETIME ft;
        GetSystemTimePreciseAsFileTime(&ft);
        
        // FILETIME is 100-nanosecond intervals since 1601-01-01
        ULARGE_INTEGER uli;
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        
        // Convert to nanoseconds
        int64_t win_nanos = (int64_t)(uli.QuadPart * 100);
        
        // Subtract offset from 1601 to 1970 (11644473600 seconds)
        int64_t unix_offset = tbb64_mul(11644473600LL, ARIA_SECOND);
        return tbb64_sub(win_nanos, unix_offset);
        
    #else
        #error "Unsupported platform for real-time clock"
    #endif
}

/* ============================================================================
 * Duration Operations
 * ============================================================================ */

AriaDuration* aria_duration_from_nanos(int64_t nanos) {
    AriaDuration* duration = (AriaDuration*)malloc(sizeof(AriaDuration));
    if (!duration) return NULL;
    duration->nanos = nanos;
    return duration;
}

AriaDuration* aria_duration_from_micros(int64_t micros) {
    int64_t nanos = tbb64_mul(micros, ARIA_MICROSECOND);
    return aria_duration_from_nanos(nanos);
}

AriaDuration* aria_duration_from_millis(int64_t millis) {
    int64_t nanos = tbb64_mul(millis, ARIA_MILLISECOND);
    return aria_duration_from_nanos(nanos);
}

AriaDuration* aria_duration_from_secs(int64_t secs) {
    int64_t nanos = tbb64_mul(secs, ARIA_SECOND);
    return aria_duration_from_nanos(nanos);
}

int64_t aria_duration_as_nanos(const AriaDuration* duration) {
    if (!duration) return ARIA_TIME_ERR;
    return duration->nanos;
}

int64_t aria_duration_as_micros(const AriaDuration* duration) {
    if (!duration) return ARIA_TIME_ERR;
    if (is_time_err(duration->nanos)) return ARIA_TIME_ERR;
    return duration->nanos / ARIA_MICROSECOND;
}

int64_t aria_duration_as_millis(const AriaDuration* duration) {
    if (!duration) return ARIA_TIME_ERR;
    if (is_time_err(duration->nanos)) return ARIA_TIME_ERR;
    return duration->nanos / ARIA_MILLISECOND;
}

int64_t aria_duration_as_secs(const AriaDuration* duration) {
    if (!duration) return ARIA_TIME_ERR;
    if (is_time_err(duration->nanos)) return ARIA_TIME_ERR;
    return duration->nanos / ARIA_SECOND;
}

AriaDuration* aria_duration_add(const AriaDuration* a, const AriaDuration* b) {
    if (!a || !b) return aria_duration_from_nanos(ARIA_TIME_ERR);
    int64_t result = tbb64_add(a->nanos, b->nanos);
    return aria_duration_from_nanos(result);
}

AriaDuration* aria_duration_sub(const AriaDuration* a, const AriaDuration* b) {
    if (!a || !b) return aria_duration_from_nanos(ARIA_TIME_ERR);
    int64_t result = tbb64_sub(a->nanos, b->nanos);
    return aria_duration_from_nanos(result);
}

AriaDuration* aria_duration_mul(const AriaDuration* d, int64_t scalar) {
    if (!d) return aria_duration_from_nanos(ARIA_TIME_ERR);
    int64_t result = tbb64_mul(d->nanos, scalar);
    return aria_duration_from_nanos(result);
}

AriaDuration* aria_duration_div(const AriaDuration* d, int64_t divisor) {
    if (!d || divisor == 0) return aria_duration_from_nanos(ARIA_TIME_ERR);
    if (is_time_err(d->nanos)) return aria_duration_from_nanos(ARIA_TIME_ERR);
    return aria_duration_from_nanos(d->nanos / divisor);
}

bool aria_duration_is_zero(const AriaDuration* duration) {
    return duration && duration->nanos == 0;
}

bool aria_duration_is_err(const AriaDuration* duration) {
    return !duration || is_time_err(duration->nanos);
}

int aria_duration_compare(const AriaDuration* a, const AriaDuration* b) {
    if (!a || !b) return 0;
    if (is_time_err(a->nanos) || is_time_err(b->nanos)) return 0;
    if (a->nanos < b->nanos) return -1;
    if (a->nanos > b->nanos) return 1;
    return 0;
}

void aria_duration_destroy(AriaDuration* duration) {
    free(duration);
}

/* ============================================================================
 * Monotonic Clock (Instant)
 * ============================================================================ */

AriaInstant* aria_instant_now(void) {
    AriaInstant* instant = (AriaInstant*)malloc(sizeof(AriaInstant));
    if (!instant) return NULL;
    instant->nanos = platform_monotonic_nanos();
    return instant;
}

AriaDuration* aria_instant_elapsed(const AriaInstant* instant) {
    if (!instant) return aria_duration_from_nanos(ARIA_TIME_ERR);
    
    AriaInstant* now = aria_instant_now();
    if (!now) return aria_duration_from_nanos(ARIA_TIME_ERR);
    
    AriaDuration* elapsed = aria_instant_duration_since(now, instant);
    aria_instant_destroy(now);
    return elapsed;
}

AriaDuration* aria_instant_duration_since(const AriaInstant* later, const AriaInstant* earlier) {
    if (!later || !earlier) return aria_duration_from_nanos(ARIA_TIME_ERR);
    int64_t diff = tbb64_sub(later->nanos, earlier->nanos);
    return aria_duration_from_nanos(diff);
}

AriaInstant* aria_instant_add(const AriaInstant* instant, const AriaDuration* duration) {
    if (!instant || !duration) {
        AriaInstant* err = (AriaInstant*)malloc(sizeof(AriaInstant));
        if (err) err->nanos = ARIA_TIME_ERR;
        return err;
    }
    
    AriaInstant* result = (AriaInstant*)malloc(sizeof(AriaInstant));
    if (!result) return NULL;
    result->nanos = tbb64_add(instant->nanos, duration->nanos);
    return result;
}

AriaInstant* aria_instant_sub(const AriaInstant* instant, const AriaDuration* duration) {
    if (!instant || !duration) {
        AriaInstant* err = (AriaInstant*)malloc(sizeof(AriaInstant));
        if (err) err->nanos = ARIA_TIME_ERR;
        return err;
    }
    
    AriaInstant* result = (AriaInstant*)malloc(sizeof(AriaInstant));
    if (!result) return NULL;
    result->nanos = tbb64_sub(instant->nanos, duration->nanos);
    return result;
}

bool aria_instant_is_err(const AriaInstant* instant) {
    return !instant || is_time_err(instant->nanos);
}

void aria_instant_destroy(AriaInstant* instant) {
    free(instant);
}

/* ============================================================================
 * Wall-Clock Time (SystemTime)
 * ============================================================================ */

AriaSystemTime* aria_systemtime_now(void) {
    AriaSystemTime* time = (AriaSystemTime*)malloc(sizeof(AriaSystemTime));
    if (!time) return NULL;
    time->nanos = platform_realtime_nanos();
    return time;
}

AriaSystemTime* aria_systemtime_from_unix_secs(int64_t secs) {
    int64_t nanos = tbb64_mul(secs, ARIA_SECOND);
    AriaSystemTime* time = (AriaSystemTime*)malloc(sizeof(AriaSystemTime));
    if (!time) return NULL;
    time->nanos = nanos;
    return time;
}

AriaSystemTime* aria_systemtime_from_unix_nanos(int64_t nanos) {
    AriaSystemTime* time = (AriaSystemTime*)malloc(sizeof(AriaSystemTime));
    if (!time) return NULL;
    time->nanos = nanos;
    return time;
}

int64_t aria_systemtime_to_unix_secs(const AriaSystemTime* time) {
    if (!time) return ARIA_TIME_ERR;
    if (is_time_err(time->nanos)) return ARIA_TIME_ERR;
    return time->nanos / ARIA_SECOND;
}

int64_t aria_systemtime_to_unix_nanos(const AriaSystemTime* time) {
    if (!time) return ARIA_TIME_ERR;
    return time->nanos;
}

AriaDuration* aria_systemtime_duration_since_epoch(const AriaSystemTime* time) {
    if (!time) return aria_duration_from_nanos(ARIA_TIME_ERR);
    return aria_duration_from_nanos(time->nanos);
}

bool aria_systemtime_is_err(const AriaSystemTime* time) {
    return !time || is_time_err(time->nanos);
}

void aria_systemtime_destroy(AriaSystemTime* time) {
    free(time);
}

/* ============================================================================
 * Sleep/Delay Functions
 * ============================================================================ */

int aria_sleep(const AriaDuration* duration) {
    if (!duration || is_time_err(duration->nanos)) return -1;
    if (duration->nanos <= 0) return 0; // No sleep for zero or negative duration
    
    #ifdef __linux__
        struct timespec ts;
        ts.tv_sec = duration->nanos / ARIA_SECOND;
        ts.tv_nsec = duration->nanos % ARIA_SECOND;
        
        // Use clock_nanosleep for precise sleeping
        while (clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, &ts) != 0) {
            // Interrupted by signal, continue sleeping remaining time
            if (ts.tv_sec == 0 && ts.tv_nsec == 0) break;
        }
        return 0;
        
    #elif defined(__APPLE__)
        struct timespec ts;
        ts.tv_sec = duration->nanos / ARIA_SECOND;
        ts.tv_nsec = duration->nanos % ARIA_SECOND;
        
        while (nanosleep(&ts, &ts) != 0) {
            if (ts.tv_sec == 0 && ts.tv_nsec == 0) break;
        }
        return 0;
        
    #elif defined(_WIN32)
        // Windows Sleep is millisecond precision
        int64_t millis = duration->nanos / ARIA_MILLISECOND;
        if (millis > 0) {
            Sleep((DWORD)millis);
        }
        return 0;
        
    #else
        #error "Unsupported platform for sleep"
    #endif
}

int aria_sleep_until(const AriaInstant* deadline) {
    if (!deadline || is_time_err(deadline->nanos)) return -1;
    
    AriaInstant* now = aria_instant_now();
    if (!now) return -1;
    
    AriaDuration* remaining = aria_instant_duration_since(deadline, now);
    aria_instant_destroy(now);
    
    if (!remaining) return -1;
    
    int result = 0;
    if (remaining->nanos > 0) {
        result = aria_sleep(remaining);
    }
    
    aria_duration_destroy(remaining);
    return result;
}

/* ============================================================================
 * High-Resolution Timing Utilities
 * ============================================================================ */

int64_t aria_timer_resolution(void) {
    #ifdef __linux__
        struct timespec ts;
        if (clock_getres(CLOCK_MONOTONIC, &ts) != 0) {
            return ARIA_MILLISECOND; // Fallback to 1ms
        }
        return ts.tv_sec * ARIA_SECOND + ts.tv_nsec;
        
    #elif defined(__APPLE__)
        static mach_timebase_info_data_t timebase = {0, 0};
        if (timebase.denom == 0) {
            mach_timebase_info(&timebase);
        }
        // Resolution is (1 * numer) / denom nanoseconds
        return (int64_t)(timebase.numer / timebase.denom);
        
    #elif defined(_WIN32)
        static LARGE_INTEGER frequency = {0};
        if (frequency.QuadPart == 0) {
            QueryPerformanceFrequency(&frequency);
        }
        // Resolution: 1 / frequency seconds = 1e9 / frequency nanoseconds
        return ARIA_SECOND / frequency.QuadPart;
        
    #else
        return ARIA_MILLISECOND; // Conservative fallback
    #endif
}

bool aria_timer_has_high_resolution(void) {
    int64_t resolution = aria_timer_resolution();
    return resolution <= ARIA_MICROSECOND; // Consider < 1μs as high-resolution
}
