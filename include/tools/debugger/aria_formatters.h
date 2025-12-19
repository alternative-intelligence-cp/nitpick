/**
 * Aria Data Formatters for LLDB
 * Phase 7.4.2: TBB Type Formatters
 * 
 * Custom LLDB formatters for Aria's unique type system:
 * - TBB (Twisted Balanced Binary) types with ERR sentinel display
 * - GC pointer synthetic children (object header inspection)
 * - Result<T> summary providers
 * 
 * Reference: docs/gemini/responses/request_036_debugger.txt (Section 4.1)
 */

#ifndef ARIA_DEBUGGER_FORMATTERS_H
#define ARIA_DEBUGGER_FORMATTERS_H

#include <lldb/API/LLDB.h>
#include <string>
#include <regex>
#include <cmath>

namespace aria {
namespace debugger {

/**
 * TBB Type Summary Provider
 * 
 * Handles display of Twisted Balanced Binary integer types:
 * - tbb8: 8-bit symmetric range [-127, +127] with ERR at -128
 * - tbb16: 16-bit symmetric range [-32767, +32767] with ERR at -32768
 * - tbb32: 32-bit symmetric range with ERR at INT32_MIN
 * - tbb64: 64-bit symmetric range with ERR at INT64_MIN
 * 
 * Format:
 * - If value == ERR sentinel: Display "ERR"
 * - If value outside symmetric range: Display "123 (OVERFLOW)"
 * - Otherwise: Display normal decimal value
 */
class TBBTypeSummaryProvider {
public:
    /**
     * Create a TBB summary provider
     * @param valobj The LLDB value object to format
     * @param internal Internal LLDB state (unused)
     * @param options Formatting options (unused)
     * @return Formatted string representation
     */
    static bool GetSummary(lldb::SBValue valobj,
                          lldb::SBStream& stream,
                          const lldb::SBTypeSummaryOptions& options);

private:
    /**
     * Extract bit width from type name (e.g., "tbb8" -> 8)
     * @param type_name Type name string
     * @return Bit width, or 0 if not a TBB type
     */
    static int extractBitWidth(const std::string& type_name);

    /**
     * Calculate ERR sentinel value for given bit width
     * @param bit_width Number of bits (8, 16, 32, or 64)
     * @return ERR sentinel (-2^(N-1))
     */
    static int64_t calculateErrSentinel(int bit_width);

    /**
     * Calculate symmetric range bounds
     * @param bit_width Number of bits
     * @return Pair of (min_valid, max_valid)
     */
    static std::pair<int64_t, int64_t> getSymmetricRange(int bit_width);
};

/**
 * GC Pointer Synthetic Children Provider
 * 
 * TODO(LLDB 20): The C++ SBSyntheticValueProvider API was removed in LLDB 20.
 * Synthetic providers now require Python scripts. This needs to be ported to:
 * - Python-based synthetic provider script
 * - Registered via CreateWithScriptCode() or CreateWithClassName()
 * 
 * Provides synthetic children for gc_ptr<T> to expose:
 * - The actual value (dereferenced pointer)
 * - Object header metadata (at -8 bytes offset):
 *   - mark_bit: GC marking status
 *   - pinned_bit: Pin status (prevents moving)
 *   - forwarded_bit: Forwarding pointer status
 *   - is_nursery: Young generation flag
 *   - size_class: Allocation size bucket
 *   - type_id: Runtime type identifier
 * 
 * Reference: research_002 (GC object headers)
 */
#if 0 // Disabled until ported to Python-based synthetic provider
class GCPointerSyntheticProvider : public lldb::SBSyntheticValueProvider {
public:
    GCPointerSyntheticProvider(lldb::SBValue valobj);
    
    virtual ~GCPointerSyntheticProvider() = default;

    virtual size_t GetNumChildren() override;
    
    virtual lldb::SBValue GetChildAtIndex(size_t idx) override;
    
    virtual size_t GetIndexOfChildWithName(const lldb::ConstString& name) override;
    
    virtual bool Update() override;
    
    virtual bool MightHaveChildren() override;

private:
    lldb::SBValue m_valobj;
    lldb::SBValue m_header_value;
    bool m_valid_header;
    
    /**
     * Read and decode the 64-bit object header
     * @return True if header was successfully read
     */
    bool readObjectHeader();
    
    /**
     * Extract bit field from header
     * @param bit_offset Offset in bits (0-63)
     * @param bit_count Number of bits to extract
     * @return Extracted value
     */
    uint64_t extractBitField(uint64_t header, int bit_offset, int bit_count);
};
#endif

/**
 * Result<T> Type Summary Provider
 * 
 * Displays Result<T> as either:
 * - "Ok(value)" if err == 0
 * - "Error(code)" if err != 0
 * 
 * Reference: research_018 (error handling)
 */
class ResultTypeSummaryProvider {
public:
    static bool GetSummary(lldb::SBValue valobj,
                          lldb::SBStream& stream,
                          const lldb::SBTypeSummaryOptions& options);
};

/**
 * Aria Formatters Registration
 * 
 * Call this to register all Aria-specific formatters with LLDB.
 * Should be invoked during debugger initialization.
 * 
 * @param debugger The LLDB debugger instance
 * @return True if registration succeeded
 */
bool RegisterAriaFormatters(lldb::SBDebugger& debugger);

} // namespace debugger
} // namespace aria

#endif // ARIA_DEBUGGER_FORMATTERS_H
