/**
 * Aria Standard Library - TBB (Twisted Balanced Binary) Implementation
 * 
 * Implementation of error-propagating integer arithmetic with symmetric ranges.
 * 
 * Reference: docs/specs/TBB_TYPE_SYSTEM_SPEC.md
 */

#include "runtime/tbb.h"
#include <stdlib.h>

// ============================================================================
// TBB8 Operations (8-bit)
// ============================================================================

int8_t tbb8_from_int(int64_t value) {
    // Check for ERR sentinel explicitly
    if (value == (int64_t)TBB8_ERR) {
        return TBB8_ERR;
    }
    
    // Range check: must be in [-127, +127]
    if (value < TBB8_MIN || value > TBB8_MAX) {
        return TBB8_ERR;
    }
    
    return (int8_t)value;
}

bool tbb8_is_err(int8_t value) {
    return value == TBB8_ERR;
}

int64_t tbb8_to_int(int8_t value) {
    // Direct conversion - ERR (-128) becomes -128 in int64
    return (int64_t)value;
}

int8_t tbb8_add(int8_t a, int8_t b) {
    // Sticky error propagation
    if (a == TBB8_ERR || b == TBB8_ERR) {
        return TBB8_ERR;
    }
    
    // Perform addition in wider type to detect overflow
    int16_t result = (int16_t)a + (int16_t)b;
    
    // Check bounds
    if (result < TBB8_MIN || result > TBB8_MAX) {
        return TBB8_ERR;
    }
    
    return (int8_t)result;
}

int8_t tbb8_sub(int8_t a, int8_t b) {
    // Sticky error propagation
    if (a == TBB8_ERR || b == TBB8_ERR) {
        return TBB8_ERR;
    }
    
    // Perform subtraction in wider type
    int16_t result = (int16_t)a - (int16_t)b;
    
    // Check bounds
    if (result < TBB8_MIN || result > TBB8_MAX) {
        return TBB8_ERR;
    }
    
    return (int8_t)result;
}

int8_t tbb8_mul(int8_t a, int8_t b) {
    // Sticky error propagation
    if (a == TBB8_ERR || b == TBB8_ERR) {
        return TBB8_ERR;
    }
    
    // Perform multiplication in wider type
    int16_t result = (int16_t)a * (int16_t)b;
    
    // Check bounds
    if (result < TBB8_MIN || result > TBB8_MAX) {
        return TBB8_ERR;
    }
    
    return (int8_t)result;
}

int8_t tbb8_div(int8_t a, int8_t b) {
    // Sticky error propagation
    if (a == TBB8_ERR || b == TBB8_ERR) {
        return TBB8_ERR;
    }
    
    // Division by zero check
    if (b == 0) {
        return TBB8_ERR;
    }
    
    // Safe division (no overflow possible with symmetric range)
    return a / b;
}

int8_t tbb8_neg(int8_t a) {
    // Sticky error propagation
    if (a == TBB8_ERR) {
        return TBB8_ERR;
    }
    
    // Negation is safe with symmetric range
    // -127 negates to +127, +127 negates to -127
    return -a;
}

int8_t tbb8_abs(int8_t a) {
    // Sticky error propagation
    if (a == TBB8_ERR) {
        return TBB8_ERR;
    }
    
    // Absolute value is safe with symmetric range
    // abs(-127) = 127, abs(127) = 127
    return a < 0 ? -a : a;
}

// ============================================================================
// TBB16 Operations (16-bit)
// ============================================================================

int16_t tbb16_from_int(int64_t value) {
    if (value == (int64_t)TBB16_ERR) {
        return TBB16_ERR;
    }
    
    if (value < TBB16_MIN || value > TBB16_MAX) {
        return TBB16_ERR;
    }
    
    return (int16_t)value;
}

bool tbb16_is_err(int16_t value) {
    return value == TBB16_ERR;
}

int64_t tbb16_to_int(int16_t value) {
    return (int64_t)value;
}

int16_t tbb16_add(int16_t a, int16_t b) {
    if (a == TBB16_ERR || b == TBB16_ERR) {
        return TBB16_ERR;
    }
    
    int32_t result = (int32_t)a + (int32_t)b;
    
    if (result < TBB16_MIN || result > TBB16_MAX) {
        return TBB16_ERR;
    }
    
    return (int16_t)result;
}

int16_t tbb16_sub(int16_t a, int16_t b) {
    if (a == TBB16_ERR || b == TBB16_ERR) {
        return TBB16_ERR;
    }
    
    int32_t result = (int32_t)a - (int32_t)b;
    
    if (result < TBB16_MIN || result > TBB16_MAX) {
        return TBB16_ERR;
    }
    
    return (int16_t)result;
}

int16_t tbb16_mul(int16_t a, int16_t b) {
    if (a == TBB16_ERR || b == TBB16_ERR) {
        return TBB16_ERR;
    }
    
    int32_t result = (int32_t)a * (int32_t)b;
    
    if (result < TBB16_MIN || result > TBB16_MAX) {
        return TBB16_ERR;
    }
    
    return (int16_t)result;
}

int16_t tbb16_div(int16_t a, int16_t b) {
    if (a == TBB16_ERR || b == TBB16_ERR) {
        return TBB16_ERR;
    }
    
    if (b == 0) {
        return TBB16_ERR;
    }
    
    return a / b;
}

int16_t tbb16_neg(int16_t a) {
    if (a == TBB16_ERR) {
        return TBB16_ERR;
    }
    
    return -a;
}

int16_t tbb16_abs(int16_t a) {
    if (a == TBB16_ERR) {
        return TBB16_ERR;
    }
    
    return a < 0 ? -a : a;
}

// ============================================================================
// TBB32 Operations (32-bit)
// ============================================================================

int32_t tbb32_from_int(int64_t value) {
    if (value == (int64_t)TBB32_ERR) {
        return TBB32_ERR;
    }
    
    if (value < TBB32_MIN || value > TBB32_MAX) {
        return TBB32_ERR;
    }
    
    return (int32_t)value;
}

bool tbb32_is_err(int32_t value) {
    return value == TBB32_ERR;
}

int64_t tbb32_to_int(int32_t value) {
    return (int64_t)value;
}

int32_t tbb32_add(int32_t a, int32_t b) {
    if (a == TBB32_ERR || b == TBB32_ERR) {
        return TBB32_ERR;
    }
    
    int64_t result = (int64_t)a + (int64_t)b;
    
    if (result < TBB32_MIN || result > TBB32_MAX) {
        return TBB32_ERR;
    }
    
    return (int32_t)result;
}

int32_t tbb32_sub(int32_t a, int32_t b) {
    if (a == TBB32_ERR || b == TBB32_ERR) {
        return TBB32_ERR;
    }
    
    int64_t result = (int64_t)a - (int64_t)b;
    
    if (result < TBB32_MIN || result > TBB32_MAX) {
        return TBB32_ERR;
    }
    
    return (int32_t)result;
}

int32_t tbb32_mul(int32_t a, int32_t b) {
    if (a == TBB32_ERR || b == TBB32_ERR) {
        return TBB32_ERR;
    }
    
    int64_t result = (int64_t)a * (int64_t)b;
    
    if (result < TBB32_MIN || result > TBB32_MAX) {
        return TBB32_ERR;
    }
    
    return (int32_t)result;
}

int32_t tbb32_div(int32_t a, int32_t b) {
    if (a == TBB32_ERR || b == TBB32_ERR) {
        return TBB32_ERR;
    }
    
    if (b == 0) {
        return TBB32_ERR;
    }
    
    return a / b;
}

int32_t tbb32_neg(int32_t a) {
    if (a == TBB32_ERR) {
        return TBB32_ERR;
    }
    
    return -a;
}

int32_t tbb32_abs(int32_t a) {
    if (a == TBB32_ERR) {
        return TBB32_ERR;
    }
    
    return a < 0 ? -a : a;
}

// ============================================================================
// TBB64 Operations (64-bit)
// ============================================================================

int64_t tbb64_from_int(int64_t value) {
    // Special case: if input is exactly INT64_MIN, treat as ERR
    if (value == TBB64_ERR) {
        return TBB64_ERR;
    }
    
    // Note: For tbb64, the input is already int64, so we just check
    // that it's not the sentinel value. The value is already in range
    // since int64 range is [-2^63, 2^63-1] and we reserve -2^63 as ERR.
    // So any value != ERR is valid.
    return value;
}

bool tbb64_is_err(int64_t value) {
    return value == TBB64_ERR;
}

int64_t tbb64_to_int(int64_t value) {
    // Direct conversion - ERR becomes INT64_MIN in int64
    return value;
}

int64_t tbb64_add(int64_t a, int64_t b) {
    if (a == TBB64_ERR || b == TBB64_ERR) {
        return TBB64_ERR;
    }
    
    // Use __int128 to detect overflow
    __int128 result = (__int128)a + (__int128)b;
    
    if (result < TBB64_MIN || result > TBB64_MAX) {
        return TBB64_ERR;
    }
    
    return (int64_t)result;
}

int64_t tbb64_sub(int64_t a, int64_t b) {
    if (a == TBB64_ERR || b == TBB64_ERR) {
        return TBB64_ERR;
    }
    
    __int128 result = (__int128)a - (__int128)b;
    
    if (result < TBB64_MIN || result > TBB64_MAX) {
        return TBB64_ERR;
    }
    
    return (int64_t)result;
}

int64_t tbb64_mul(int64_t a, int64_t b) {
    if (a == TBB64_ERR || b == TBB64_ERR) {
        return TBB64_ERR;
    }
    
    __int128 result = (__int128)a * (__int128)b;
    
    if (result < TBB64_MIN || result > TBB64_MAX) {
        return TBB64_ERR;
    }
    
    return (int64_t)result;
}

int64_t tbb64_div(int64_t a, int64_t b) {
    if (a == TBB64_ERR || b == TBB64_ERR) {
        return TBB64_ERR;
    }
    
    if (b == 0) {
        return TBB64_ERR;
    }
    
    return a / b;
}

int64_t tbb64_neg(int64_t a) {
    if (a == TBB64_ERR) {
        return TBB64_ERR;
    }
    
    // Negation is safe with symmetric range
    return -a;
}

int64_t tbb64_abs(int64_t a) {
    if (a == TBB64_ERR) {
        return TBB64_ERR;
    }

    return a < 0 ? -a : a;
}

// ============================================================================
// TBB Widening Operations (ARIA-018: Sentinel-Preserving Conversion)
// ============================================================================
// These functions safely widen TBB types while preserving error semantics.
// Standard sign extension would convert tbb8 ERR (-128) to -128 in tbb16,
// which is a valid value, not the tbb16 sentinel (-32768).
// These functions map source sentinel → destination sentinel.

extern "C" int16_t npk_tbb_widen_8_16(int8_t val) {
    if (val == TBB8_ERR) {
        return TBB16_ERR;
    }
    return (int16_t)val;
}

extern "C" int32_t npk_tbb_widen_8_32(int8_t val) {
    if (val == TBB8_ERR) {
        return TBB32_ERR;
    }
    return (int32_t)val;
}

extern "C" int64_t npk_tbb_widen_8_64(int8_t val) {
    if (val == TBB8_ERR) {
        return TBB64_ERR;
    }
    return (int64_t)val;
}

extern "C" int32_t npk_tbb_widen_16_32(int16_t val) {
    if (val == TBB16_ERR) {
        return TBB32_ERR;
    }
    return (int32_t)val;
}

extern "C" int64_t npk_tbb_widen_16_64(int16_t val) {
    if (val == TBB16_ERR) {
        return TBB64_ERR;
    }
    return (int64_t)val;
}

extern "C" int64_t npk_tbb_widen_32_64(int32_t val) {
    if (val == TBB32_ERR) {
        return TBB64_ERR;
    }
    return (int64_t)val;
}
