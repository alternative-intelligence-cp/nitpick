/**
 * Locale-independent floating-point formatting for ARIA determinism
 * 
 * CRITICAL: sprintf/snprintf use the system's LC_NUMERIC locale, which means:
 * - US/UK (en_US): 0.5 uses decimal POINT
 * - Germany (de_DE): 0,5 uses decimal COMMA
 * - France, Poland, many others: comma separator
 * 
 * This breaks ARIA's determinism guarantee: same code must produce same output
 * everywhere, regardless of system locale settings.
 * 
 * Solution: Always use C locale for float-to-string conversion.
 */

#include <stdio.h>
#include <locale.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Deterministic snprintf for floating-point values
 * Always uses '.' as decimal separator (C locale) regardless of system locale
 * 
 * @param buffer Output buffer
 * @param size Buffer size
 * @param format Format string (should be like "%.17g")
 * @param value Double value to format
 * @return Number of characters written (excluding null terminator)
 */
int aria_snprintf_c_locale(char* buffer, uint64_t size, const char* format, double value) {
    // Save current locale
    char* old_locale = setlocale(LC_NUMERIC, NULL);
    
    // Force C locale (always uses '.' as decimal point)
    setlocale(LC_NUMERIC, "C");
    
    // Format the number with C locale active
    int result = snprintf(buffer, (size_t)size, format, value);
    
    // Restore original locale
    if (old_locale) {
        setlocale(LC_NUMERIC, old_locale);
    }
    
    return result;
}

#ifdef __cplusplus
}
#endif
