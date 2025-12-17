/**
 * Aria Timer/Clock Library - Phase 5.9
 * 
 * Provides high-resolution timing, monotonic clocks, and timer management.
 * Uses TBB64 for symmetric time arithmetic with overflow detection.
 * 
 * Time Model:
 * - Monotonic Time: Strictly increasing, never affected by clock adjustments
 * - Wall-Clock Time: Real-world UTC time, subject to NTP adjustments
 * - Duration: TBB64 nanoseconds with symmetric overflow handling
 * 
 * Key Features:
 * - Sub-microsecond precision where hardware supports it
 * - TBB64 prevents asymmetry bugs (no abs(INT64_MIN) issues)
 * - Sticky error propagation for invalid time calculations
 * - Cross-platform: Linux (CLOCK_MONOTONIC), macOS (mach_absolute_time), Windows (QPC)
 */

#ifndef ARIA_RUNTIME_TIMER_H
#define ARIA_RUNTIME_TIMER_H

#include "runtime/io.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Time Constants
 * ============================================================================ */

// Time unit: nanoseconds (1 billionth of a second)
// Using TBB64 provides symmetric range: [-2^63+1, 2^63-1] nanoseconds
// Range: +/- 292 years from epoch

#define ARIA_NANOSECOND   ((int64_t)1)
#define ARIA_MICROSECOND  ((int64_t)1000)
#define ARIA_MILLISECOND  ((int64_t)1000000)
#define ARIA_SECOND       ((int64_t)1000000000)
#define ARIA_MINUTE       ((int64_t)60000000000)
#define ARIA_HOUR         ((int64_t)3600000000000)

// TBB64 Error Sentinel for time operations
#define ARIA_TIME_ERR     ((int64_t)-9223372036854775807LL - 1)

// TBB64 Max/Min values for time
#define ARIA_TIME_MAX     ((int64_t)9223372036854775807LL)
#define ARIA_TIME_MIN     ((int64_t)-9223372036854775807LL)

/* ============================================================================
 * Core Time Types
 * ============================================================================ */

/**
 * Duration: A span of time in nanoseconds.
 * 
 * Uses TBB64 for symmetric overflow handling:
 * - Range: +/- 292 years
 * - Overflow produces ARIA_TIME_ERR (sticky error propagation)
 * - No asymmetry bugs: abs(-MAX) works correctly
 */
typedef struct AriaDuration AriaDuration;

/**
 * Instant: A point in monotonic time.
 * 
 * Represents an opaque point on the monotonic clock timeline.
 * Cannot be compared across system reboots (epoch is boot time).
 * Safe for measuring elapsed time and setting deadlines.
 */
typedef struct AriaInstant AriaInstant;

/**
 * SystemTime: A point in wall-clock time (UTC).
 * 
 * Represents real-world time, synchronized with UTC.
 * Subject to clock adjustments (NTP, manual changes).
 * Use only for timestamps, NOT for duration measurement.
 */
typedef struct AriaSystemTime AriaSystemTime;

/* ============================================================================
 * Duration Operations
 * ============================================================================ */

/**
 * Create a duration from nanoseconds.
 * Returns ERR if value is already ERR or invalid.
 */
AriaDuration* aria_duration_from_nanos(int64_t nanos);

/**
 * Create duration from various time units.
 */
AriaDuration* aria_duration_from_micros(int64_t micros);
AriaDuration* aria_duration_from_millis(int64_t millis);
AriaDuration* aria_duration_from_secs(int64_t secs);

/**
 * Extract duration components.
 * Returns ARIA_TIME_ERR if duration is ERR.
 */
int64_t aria_duration_as_nanos(const AriaDuration* duration);
int64_t aria_duration_as_micros(const AriaDuration* duration);
int64_t aria_duration_as_millis(const AriaDuration* duration);
int64_t aria_duration_as_secs(const AriaDuration* duration);

/**
 * Duration arithmetic with TBB64 overflow detection.
 * Returns ERR duration if either operand is ERR or result overflows.
 */
AriaDuration* aria_duration_add(const AriaDuration* a, const AriaDuration* b);
AriaDuration* aria_duration_sub(const AriaDuration* a, const AriaDuration* b);
AriaDuration* aria_duration_mul(const AriaDuration* d, int64_t scalar);
AriaDuration* aria_duration_div(const AriaDuration* d, int64_t divisor);

/**
 * Duration comparison.
 * If either is ERR, returns false for all comparisons.
 */
bool aria_duration_is_zero(const AriaDuration* duration);
bool aria_duration_is_err(const AriaDuration* duration);
int aria_duration_compare(const AriaDuration* a, const AriaDuration* b); // -1, 0, 1

/**
 * Destroy a duration.
 */
void aria_duration_destroy(AriaDuration* duration);

/* ============================================================================
 * Monotonic Clock (Instant)
 * ============================================================================ */

/**
 * Get current monotonic time.
 * 
 * This clock:
 * - Never goes backward
 * - Unaffected by system clock changes
 * - Has arbitrary epoch (usually boot time)
 * - Ideal for measuring elapsed time
 * 
 * Platform notes:
 * - Linux: CLOCK_MONOTONIC via clock_gettime
 * - macOS: mach_absolute_time
 * - Windows: QueryPerformanceCounter
 */
AriaInstant* aria_instant_now(void);

/**
 * Calculate elapsed time since an instant.
 * Returns ERR duration if instant is invalid or calculation overflows.
 */
AriaDuration* aria_instant_elapsed(const AriaInstant* instant);

/**
 * Calculate duration between two instants.
 * Returns (later - earlier). ERR if either is invalid or overflow occurs.
 */
AriaDuration* aria_instant_duration_since(const AriaInstant* later, const AriaInstant* earlier);

/**
 * Add duration to instant.
 * Returns new instant or ERR instant if overflow.
 */
AriaInstant* aria_instant_add(const AriaInstant* instant, const AriaDuration* duration);

/**
 * Subtract duration from instant.
 * Returns new instant or ERR instant if underflow.
 */
AriaInstant* aria_instant_sub(const AriaInstant* instant, const AriaDuration* duration);

/**
 * Check if instant is ERR.
 */
bool aria_instant_is_err(const AriaInstant* instant);

/**
 * Destroy an instant.
 */
void aria_instant_destroy(AriaInstant* instant);

/* ============================================================================
 * Wall-Clock Time (SystemTime)
 * ============================================================================ */

/**
 * Get current wall-clock time (UTC).
 * 
 * WARNING: Do NOT use for duration measurements!
 * This clock can jump forward or backward due to:
 * - Manual date/time changes
 * - NTP corrections
 * - Leap seconds
 * 
 * Use only for user-facing timestamps.
 * 
 * Platform notes:
 * - Linux: CLOCK_REALTIME via clock_gettime
 * - macOS: gettimeofday
 * - Windows: GetSystemTimePreciseAsFileTime
 */
AriaSystemTime* aria_systemtime_now(void);

/**
 * Create SystemTime from Unix timestamp (seconds since 1970-01-01 UTC).
 */
AriaSystemTime* aria_systemtime_from_unix_secs(int64_t secs);

/**
 * Create SystemTime from Unix timestamp with nanosecond precision.
 */
AriaSystemTime* aria_systemtime_from_unix_nanos(int64_t nanos);

/**
 * Convert SystemTime to Unix timestamp.
 * Returns ARIA_TIME_ERR if systemtime is invalid.
 */
int64_t aria_systemtime_to_unix_secs(const AriaSystemTime* time);
int64_t aria_systemtime_to_unix_nanos(const AriaSystemTime* time);

/**
 * Calculate duration since Unix epoch (1970-01-01 00:00:00 UTC).
 * Returns ERR duration if systemtime is invalid.
 */
AriaDuration* aria_systemtime_duration_since_epoch(const AriaSystemTime* time);

/**
 * Check if systemtime is ERR.
 */
bool aria_systemtime_is_err(const AriaSystemTime* time);

/**
 * Destroy a system time.
 */
void aria_systemtime_destroy(AriaSystemTime* time);

/* ============================================================================
 * Sleep/Delay Functions
 * ============================================================================ */

/**
 * Sleep for the specified duration.
 * 
 * This is a blocking call that suspends the current thread.
 * For async/non-blocking sleep, use the async runtime's sleep function.
 * 
 * Returns:
 * - 0 on success (slept for full duration)
 * - -1 if interrupted or duration is ERR
 * 
 * Note: Actual sleep time may be slightly longer due to scheduler granularity.
 */
int aria_sleep(const AriaDuration* duration);

/**
 * Sleep until the specified deadline (monotonic).
 * 
 * Sleeps until the monotonic clock reaches the specified instant.
 * Returns immediately if deadline has already passed.
 * 
 * Returns:
 * - 0 on success
 * - -1 if interrupted or instant is ERR
 */
int aria_sleep_until(const AriaInstant* deadline);

/* ============================================================================
 * High-Resolution Timing Utilities
 * ============================================================================ */

/**
 * Get timer resolution in nanoseconds.
 * 
 * Returns the smallest measurable time unit on this platform.
 * Typical values:
 * - Modern Linux: ~1-10ns (VDSO TSC)
 * - macOS: ~1ns (mach_absolute_time)
 * - Windows: ~100ns (QueryPerformanceCounter)
 */
int64_t aria_timer_resolution(void);

/**
 * Check if high-resolution timers are available.
 * Returns true if nanosecond precision is supported.
 */
bool aria_timer_has_high_resolution(void);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_TIMER_H
