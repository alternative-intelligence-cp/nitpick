/**
 * Aria Data Formatters for LLDB - Implementation
 * Phase 7.4.2: TBB Type Formatters
 */

#include "tools/debugger/aria_formatters.h"
#include <sstream>
#include <iomanip>

namespace npk {
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
// GC Pointer Synthetic Provider — Python-based (LLDB 20+)
// ============================================================================
// Ported from C++ SBSyntheticValueProvider (removed in LLDB 20) to Python.
// Script is executed via LLDB's script interpreter during formatter registration.

static const char* kGCPointerPythonScript = R"PYTHON(
import lldb

class GCPointerSyntheticProvider:
    """Synthetic children for gc_ptr<T> — exposes dereferenced value and GC header metadata."""

    # Object header layout (64-bit at ptr - 8):
    #   bits [0:1]   color          (2 bits: WHITE=0, GRAY=1, BLACK=2)
    #   bit  [2]     pinned_bit     (1 bit)
    #   bit  [3]     forwarded_bit  (1 bit)
    #   bit  [4]     is_nursery     (1 bit)
    #   bits [5:12]  size_class     (8 bits)
    #   bits [13:44] type_id        (32 bits)

    COLOR_NAMES = {0: "WHITE", 1: "GRAY", 2: "BLACK"}

    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.children = []

    def num_children(self):
        return len(self.children)

    def get_child_index(self, name):
        for i, (n, _v) in enumerate(self.children):
            if n == name:
                return i
        return -1

    def get_child_at_index(self, index):
        if index < 0 or index >= len(self.children):
            return None
        name, val = self.children[index]
        return val

    def update(self):
        self.children = []
        if not self.valobj.IsValid():
            return

        # Expose the dereferenced pointer value
        deref = self.valobj.Dereference()
        if deref.IsValid():
            self.children.append(("*value", deref))

        # Read GC object header at (ptr - 8)
        ptr_val = self.valobj.GetValueAsUnsigned(0)
        if ptr_val == 0:
            return

        header_addr = ptr_val - 8
        process = self.valobj.GetProcess()
        if not process.IsValid():
            return

        error = lldb.SBError()
        header = process.ReadUnsignedFromMemory(header_addr, 8, error)
        if error.Fail():
            return

        color = header & 0x3
        pinned = (header >> 2) & 0x1
        forwarded = (header >> 3) & 0x1
        nursery = (header >> 4) & 0x1
        size_class = (header >> 5) & 0xFF
        type_id = (header >> 13) & 0xFFFFFFFF

        # Create synthetic children from header fields
        target = self.valobj.GetTarget()
        data = lldb.SBData.CreateDataFromUInt64Array(
            target.GetByteOrder(), target.GetAddressByteSize(), [color])
        color_name = self.COLOR_NAMES.get(color, str(color))
        # Use CreateValueFromData for synthetic children
        uint8_ty = target.FindFirstType("unsigned char")
        uint32_ty = target.FindFirstType("unsigned int")
        if uint8_ty.IsValid() and uint32_ty.IsValid():
            d8 = lambda v: lldb.SBData.CreateDataFromUInt64Array(
                target.GetByteOrder(), target.GetAddressByteSize(), [v])
            d32 = lambda v: lldb.SBData.CreateDataFromUInt64Array(
                target.GetByteOrder(), target.GetAddressByteSize(), [v])
            self.children.append(("[header.color]",
                self.valobj.CreateValueFromData("[header.color]", d8(color), uint8_ty)))
            self.children.append(("[header.pinned]",
                self.valobj.CreateValueFromData("[header.pinned]", d8(pinned), uint8_ty)))
            self.children.append(("[header.forwarded]",
                self.valobj.CreateValueFromData("[header.forwarded]", d8(forwarded), uint8_ty)))
            self.children.append(("[header.is_nursery]",
                self.valobj.CreateValueFromData("[header.is_nursery]", d8(nursery), uint8_ty)))
            self.children.append(("[header.size_class]",
                self.valobj.CreateValueFromData("[header.size_class]", d8(size_class), uint8_ty)))
            self.children.append(("[header.type_id]",
                self.valobj.CreateValueFromData("[header.type_id]", d32(type_id), uint32_ty)))

    def has_children(self):
        return True
)PYTHON";

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
        "npk::debugger::TBBTypeSummaryProvider::GetSummary"
    );
    
    if (tbb_summary.IsValid()) {
        category.AddTypeSummary(lldb::SBTypeNameSpecifier("tbb8", false), tbb_summary);
        category.AddTypeSummary(lldb::SBTypeNameSpecifier("tbb16", false), tbb_summary);
        category.AddTypeSummary(lldb::SBTypeNameSpecifier("tbb32", false), tbb_summary);
        category.AddTypeSummary(lldb::SBTypeNameSpecifier("tbb64", false), tbb_summary);
    }

    // Result<T> type - using regex to match result<.*>
    lldb::SBTypeSummary result_summary = lldb::SBTypeSummary::CreateWithFunctionName(
        "npk::debugger::ResultTypeSummaryProvider::GetSummary"
    );
    
    if (result_summary.IsValid()) {
        // Note: Regex matching in LLDB requires careful pattern
        category.AddTypeSummary(
            lldb::SBTypeNameSpecifier("^result<.+>$", true),  // regex = true
            result_summary
        );
    }

    // GC pointer synthetic children — Python-based provider (LLDB 20+)
    // Execute the Python script to define the class, then register it
    lldb::SBCommandInterpreter interp = debugger.GetCommandInterpreter();
    lldb::SBCommandReturnObject ret;
    std::string script_cmd = std::string("script exec(\"\"\"") + kGCPointerPythonScript + "\"\"\")";
    interp.HandleCommand(script_cmd.c_str(), ret);
    
    if (ret.Succeeded()) {
        lldb::SBTypeSynthetic gc_synth = lldb::SBTypeSynthetic::CreateWithClassName(
            "GCPointerSyntheticProvider"
        );
        if (gc_synth.IsValid()) {
            category.AddTypeSynthetic(
                lldb::SBTypeNameSpecifier("^gc_ptr<.+>$", true),  // regex = true
                gc_synth
            );
        }
    }
    
    // Enable the category
    category.SetEnabled(true);
    
    return true;
}

} // namespace debugger
} // namespace npk
