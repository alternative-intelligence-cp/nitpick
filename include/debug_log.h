#ifndef ARIA_DEBUG_LOG_H
#define ARIA_DEBUG_LOG_H

// Gate [DEBUG] / [TBB DEBUG] stderr output behind ARIA_DEBUG_CODEGEN.
// Build with -DARIA_DEBUG_CODEGEN to enable codegen debug output.
// In release builds these compile to nothing via a null stream.

#include <iostream>

#ifdef ARIA_DEBUG_CODEGEN
  #define ARIA_DBG_STREAM std::cerr
#else
  // Null stream that discards all output at compile time
  struct AriaDbgNull {
    template<typename T>
    AriaDbgNull& operator<<(const T&) { return *this; }
    AriaDbgNull& operator<<(std::ostream& (*)(std::ostream&)) { return *this; }
  };
  inline AriaDbgNull npk_dbg_null;
  #define ARIA_DBG_STREAM npk_dbg_null
#endif

#endif // ARIA_DEBUG_LOG_H
