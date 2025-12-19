/**
 * Aria Data Formatters for LLDB - Implementation
 * Phase 7.4.2: TBB Type Formatters
 */

#include "tools/debugger/aria_formatters.h"
#include <sstream>
#include <iomanip>

namespace aria {
namespace debugger {

// ============================================================================
// TBB Type Summary Provider Implementation
// ============================================================================

bool TBBTypeSummaryProvider::GetSummary(lldb::SBValue valobj,
                                        lldb::SBStream& stream,
                                        const lldb::SBTypeSummaryOptions& options) {
    if (!valobj.IsValid()) {
        return false;
    }

    // Get type name and extract bit width
    std::string type_name = valobj.GetTypeName();
    int bit_width = extractBitWidth(type_name);
    
    if (bit_width == 0) {
        return false;  // Not a TBB type
    }

    // Read the value as signed integer
    lldb::SBError error;
    int64_t value = 0;
    
    switch (bit_width) {
        case 8:
            value = static_cast<int64_t>(valobj.GetValueAsSigned(error));
            break;
        case 16:
            value = static_cast<int64_t>(valobj.GetValueAsSigned(error));
            break;
        case 32:
            value = static_cast<int64_t>(valobj.GetValueAsSigned(error));
            break;
        case 64:
            value = valobj.GetValueAsSigned(error);
            break;
        default:
            return false;
    }
    
    if (error.Fail()) {
        return false;
    }

    // Calculate ERR sentinel and symmetric range
    int64_t err_sentinel = calculateErrSentinel(bit_width);
    auto [min_valid, max_valid] = getSymmetricRange(bit_width);

    // Format the value
    std::ostringstream oss;
    
    if (value == err_sentinel) {
        // Display ERR sentinel
        oss << "ERR";
    } else if (value < min_valid || value > max_valid) {
        // Value outside symmetric range - overflow
        oss << value << " (OVERFLOW)";
    } else {
        // Normal value within symmetric range
        oss << value;
    }

    stream.Printf("%s", oss.str().c_str());
    return true;
}

int TBBTypeSummaryProvider::extractBitWidth(const std::string& type_name) {
    // Match pattern: tbb8, tbb16, tbb32, tbb64
    std::regex tbb_regex("tbb(\\d+)");
    std::smatch match;
    
    if (std::regex_search(type_name, match, tbb_regex)) {
        if (match.size() > 1) {
            try {
                return std::stoi(match[1].str());
            } catch (...) {
                return 0;
            }
        }
    }
    
    return 0;
}

int64_t TBBTypeSummaryProvider::calculateErrSentinel(int bit_width) {
    // ERR = -2^(N-1)
    // For 8-bit: -2^7 = -128
    // For 16-bit: -2^15 = -32768
    // For 32-bit: -2^31 = -2147483648
    // For 64-bit: -2^63 = -9223372036854775808
    
    switch (bit_width) {
        case 8:  return -128LL;
        case 16: return -32768LL;
        case 32: return -2147483648LL;
        case 64: return -9223372036854775807LL - 1LL;  // INT64_MIN
        default: return 0;
    }
}

std::pair<int64_t, int64_t> TBBTypeSummaryProvider::getSymmetricRange(int bit_width) {
    // Symmetric range: [-(2^(N-1) - 1), +(2^(N-1) - 1)]
    // For 8-bit: [-127, +127]
    // For 16-bit: [-32767, +32767]
    // For 32-bit: [-2147483647, +2147483647]
    // For 64-bit: [-9223372036854775807, +9223372036854775807]
    
    int64_t err = calculateErrSentinel(bit_width);
    int64_t max_valid = -err - 1;  // Positive bound
    int64_t min_valid = err + 1;   // Negative bound (one above ERR)
    
    return {min_valid, max_valid};
}

// ============================================================================
// GC Pointer Synthetic Provider Implementation
// ============================================================================

GCPointerSyntheticProvider::GCPointerSyntheticProvider(lldb::SBValue valobj)
    : m_valobj(valobj), m_valid_header(false) {
    Update();
}

size_t GCPointerSyntheticProvider::GetNumChildren() {
    if (!m_valid_header) {
        return 1;  // Just the value itself
    }
    // Value + 6 header fields (mark_bit, pinned_bit, forwarded_bit, 
    // is_nursery, size_class, type_id)
    return 7;
}

lldb::SBValue GCPointerSyntheticProvider::GetChildAtIndex(size_t idx) {
    if (idx == 0) {
        // First child is the dereferenced value
        lldb::SBValue deref = m_valobj.Dereference();
        if (deref.IsValid()) {
            deref.SetName("value");
            return deref;
        }
    }
    
    if (!m_valid_header || idx == 0 || idx > 6) {
        return lldb::SBValue();
    }
    
    // Read header value
    lldb::SBError error;
    uint64_t header = m_header_value.GetValueAsUnsigned(error);
    if (error.Fail()) {
        return lldb::SBValue();
    }
    
    // Extract bit fields according to ObjHeader layout
    // Reference: research_002 (GC object header structure)
    // Layout: [type_id:16][size_class:8][reserved:5][is_nursery:1][forwarded:1][pinned:1][mark:1]
    
    lldb::SBValue child;
    std::string name;
    uint64_t value = 0;
    
    switch (idx) {
        case 1:  // mark_bit (bit 0)
            name = "mark_bit";
            value = extractBitField(header, 0, 1);
            break;
        case 2:  // pinned_bit (bit 1)
            name = "pinned_bit";
            value = extractBitField(header, 1, 1);
            break;
        case 3:  // forwarded_bit (bit 2)
            name = "forwarded_bit";
            value = extractBitField(header, 2, 1);
            break;
        case 4:  // is_nursery (bit 3)
            name = "is_nursery";
            value = extractBitField(header, 3, 1);
            break;
        case 5:  // size_class (bits 8-15)
            name = "size_class";
            value = extractBitField(header, 8, 8);
            break;
        case 6:  // type_id (bits 16-31)
            name = "type_id";
            value = extractBitField(header, 16, 16);
            break;
    }
    
    // Create synthetic value
    lldb::SBType uint_type = m_valobj.GetTarget().GetBasicType(lldb::eBasicTypeUnsignedInt);
    lldb::SBData data;
    data.SetData(error, &value, sizeof(value), m_valobj.GetTarget().GetByteOrder(),
                 m_valobj.GetTarget().GetAddressByteSize());
    
    child = m_valobj.CreateValueFromData(name.c_str(), data, uint_type);
    return child;
}

size_t GCPointerSyntheticProvider::GetIndexOfChildWithName(const lldb::ConstString& name) {
    std::string name_str = name.GetCString();
    
    if (name_str == "value") return 0;
    if (name_str == "mark_bit") return 1;
    if (name_str == "pinned_bit") return 2;
    if (name_str == "forwarded_bit") return 3;
    if (name_str == "is_nursery") return 4;
    if (name_str == "size_class") return 5;
    if (name_str == "type_id") return 6;
    
    return UINT32_MAX;
}

bool GCPointerSyntheticProvider::Update() {
    m_valid_header = readObjectHeader();
    return true;
}

bool GCPointerSyntheticProvider::MightHaveChildren() {
    return true;
}

bool GCPointerSyntheticProvider::readObjectHeader() {
    if (!m_valobj.IsValid()) {
        return false;
    }
    
    // Get the pointer address
    lldb::addr_t ptr_addr = m_valobj.GetValueAsUnsigned();
    if (ptr_addr == 0 || ptr_addr == LLDB_INVALID_ADDRESS) {
        return false;
    }
    
    // Object header is at -8 bytes from the pointer
    lldb::addr_t header_addr = ptr_addr - 8;
    
    // Read 8 bytes for the header
    lldb::SBError error;
    lldb::SBProcess process = m_valobj.GetProcess();
    if (!process.IsValid()) {
        return false;
    }
    
    uint64_t header = 0;
    size_t bytes_read = process.ReadMemory(header_addr, &header, 8, error);
    
    if (error.Fail() || bytes_read != 8) {
        return false;
    }
    
    // Store header value for later access
    lldb::SBType uint64_type = m_valobj.GetTarget().GetBasicType(lldb::eBasicTypeUnsignedLongLong);
    lldb::SBData data;
    data.SetData(error, &header, sizeof(header), 
                 m_valobj.GetTarget().GetByteOrder(),
                 m_valobj.GetTarget().GetAddressByteSize());
    
    m_header_value = m_valobj.CreateValueFromData("header", data, uint64_type);
    
    return m_header_value.IsValid();
}

uint64_t GCPointerSyntheticProvider::extractBitField(uint64_t header, 
                                                     int bit_offset, 
                                                     int bit_count) {
    uint64_t mask = ((1ULL << bit_count) - 1) << bit_offset;
    return (header & mask) >> bit_offset;
}

// ============================================================================
// Result<T> Type Summary Provider Implementation
// ============================================================================

bool ResultTypeSummaryProvider::GetSummary(lldb::SBValue valobj,
                                           lldb::SBStream& stream,
                                           const lldb::SBTypeSummaryOptions& options) {
    if (!valobj.IsValid()) {
        return false;
    }

    // Result<T> structure: { err: tbb, val: T }
    lldb::SBValue err_field = valobj.GetChildMemberWithName("err");
    
    if (!err_field.IsValid()) {
        return false;
    }

    lldb::SBError error;
    int64_t err_code = err_field.GetValueAsSigned(error);
    
    if (error.Fail()) {
        return false;
    }

    std::ostringstream oss;
    
    if (err_code != 0) {
        // Error case
        oss << "Error(" << err_code << ")";
    } else {
        // Ok case - show the value
        lldb::SBValue val_field = valobj.GetChildMemberWithName("val");
        if (val_field.IsValid()) {
            const char* val_summary = val_field.GetSummary();
            if (val_summary) {
                oss << "Ok(" << val_summary << ")";
            } else {
                oss << "Ok(" << val_field.GetValue() << ")";
            }
        } else {
            oss << "Ok";
        }
    }

    stream.Printf("%s", oss.str().c_str());
    return true;
}

// ============================================================================
// Formatter Registration
// ============================================================================

bool RegisterAriaFormatters(lldb::SBDebugger& debugger) {
    // Register TBB type summary provider
    // Matches: tbb8, tbb16, tbb32, tbb64
    lldb::SBTypeCategory category = debugger.GetCategory("aria");
    if (!category.IsValid()) {
        category = debugger.CreateCategory("aria");
    }
    
    if (!category.IsValid()) {
        return false;
    }

    // TBB types - using regex to match tbb\d+
    lldb::SBTypeSummary tbb_summary = lldb::SBTypeSummary::CreateWithFunctionName(
        "aria::debugger::TBBTypeSummaryProvider::GetSummary"
    );
    
    if (tbb_summary.IsValid()) {
        category.AddTypeSummary(lldb::SBTypeNameSpecifier("tbb8", false), tbb_summary);
        category.AddTypeSummary(lldb::SBTypeNameSpecifier("tbb16", false), tbb_summary);
        category.AddTypeSummary(lldb::SBTypeNameSpecifier("tbb32", false), tbb_summary);
        category.AddTypeSummary(lldb::SBTypeNameSpecifier("tbb64", false), tbb_summary);
    }

    // Result<T> type - using regex to match result<.*>
    lldb::SBTypeSummary result_summary = lldb::SBTypeSummary::CreateWithFunctionName(
        "aria::debugger::ResultTypeSummaryProvider::GetSummary"
    );
    
    if (result_summary.IsValid()) {
        // Note: Regex matching in LLDB requires careful pattern
        category.AddTypeSummary(
            lldb::SBTypeNameSpecifier("^result<.+>$", true),  // regex = true
            result_summary
        );
    }

    // GC pointer synthetic children
    // Note: SBSyntheticValueProvider registration requires additional steps
    // This will be implemented when the full debugger framework is in place
    
    // Enable the category
    category.SetEnabled(true);
    
    return true;
}

} // namespace debugger
} // namespace aria
