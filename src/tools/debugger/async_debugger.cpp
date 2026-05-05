/**
 * Async/Await Debugging Support Implementation
 * Phase 7.4.5
 */

#include "tools/debugger/async_debugger.h"
#include <sstream>

namespace npk {
namespace debugger {

AsyncDebugger::AsyncDebugger(lldb::SBTarget target, lldb::SBProcess process)
    : m_target(target), m_process(process) {
}

std::vector<AsyncFrameInfo> AsyncDebugger::getAsyncCallStack(lldb::SBThread thread) {
    std::vector<AsyncFrameInfo> async_stack;
    
    // Iterate through physical stack frames
    uint32_t num_frames = thread.GetNumFrames();
    for (uint32_t i = 0; i < num_frames; i++) {
        lldb::SBFrame frame = thread.GetFrameAtIndex(i);
        
        if (!isAsyncFrame(frame)) {
            continue;
        }
        
        // This is an async function frame
        AsyncFrameInfo info;
        info.function_name = frame.GetFunctionName();
        
        lldb::SBLineEntry line_entry = frame.GetLineEntry();
        lldb::SBFileSpec file_spec = line_entry.GetFileSpec();
        info.source_file = file_spec.GetFilename();
        info.line_number = line_entry.GetLine();
        
        info.frame_pointer = getCoroutineFrameAddress(frame);
        
        // Try to find associated Future<T>
        lldb::SBValue future_var = frame.FindVariable("__coro_future");
        if (future_var.IsValid()) {
            lldb::SBError error;
            info.future_address = future_var.GetLoadAddress();
            
            FutureInfo future_info = getFutureInfo(future_var);
            info.future_state = future_info.state;
        } else {
            info.future_state = "UNKNOWN";
        }
        
        // Check if at suspension point
        // LLVM coroutines insert calls to __aria_coro_suspend at await points
        std::string func_name = frame.GetFunctionName();
        info.is_suspended = (func_name.find("suspend") != std::string::npos ||
                            func_name.find("__coro") != std::string::npos);
        
        if (info.is_suspended) {
            info.suspend_reason = getAwaiterExpression(frame);
        }
        
        async_stack.push_back(info);
    }
    
    return async_stack;
}

FutureInfo AsyncDebugger::getFutureInfo(lldb::SBValue future_value) {
    FutureInfo info;
    info.address = future_value.GetLoadAddress();
    info.type_name = future_value.GetTypeName();
    
    // Read Future state enum
    lldb::SBValue state_field = future_value.GetChildMemberWithName("state");
    if (state_field.IsValid()) {
        lldb::SBError error;
        int32_t state_val = state_field.GetValueAsSigned(error);
        
        if (!error.Fail()) {
            switch (state_val) {
                case 0: info.state = "PENDING"; break;
                case 1: info.state = "READY"; break;
                case 2: info.state = "ERROR"; break;
                default: info.state = "INVALID"; break;
            }
        }
    } else {
        info.state = "UNKNOWN";
    }
    
    // Read error flag
    lldb::SBValue error_field = future_value.GetChildMemberWithName("hasError");
    if (error_field.IsValid()) {
        lldb::SBError error;
        info.has_error = error_field.GetValueAsUnsigned(error) != 0;
    }
    
    // If READY, get the value
    if (info.state == "READY") {
        lldb::SBValue value_field = future_value.GetChildMemberWithName("value");
        if (value_field.IsValid()) {
            // Need to dereference the void* and cast to proper type
            // For now, just store the pointer value
            info.value = value_field.Dereference();
        }
    }
    
    return info;
}

CoroutineInfo AsyncDebugger::getCoroutineInfo(lldb::addr_t coro_handle) {
    CoroutineInfo info;
    info.handle = coro_handle;
    info.is_done = false;
    info.frame_address = 0;
    info.frame_size = 0;
    
    if (coro_handle == 0 || coro_handle == LLDB_INVALID_ADDRESS) {
        return info;
    }
    
    // Call __aria_coro_done to check if coroutine finished
    // This requires evaluating an expression in the debuggee
    lldb::SBThread thread = m_process.GetSelectedThread();
    if (thread.IsValid()) {
        std::ostringstream expr;
        expr << "__aria_coro_done((void*)" << std::hex << coro_handle << ")";
        
        lldb::SBFrame frame = thread.GetSelectedFrame();
        lldb::SBValue result = frame.EvaluateExpression(expr.str().c_str());
        
        if (result.IsValid()) {
            lldb::SBError error;
            info.is_done = result.GetValueAsUnsigned(error) != 0;
        }
    }
    
    // Coroutine frames are opaque, but we can get frame address
    // from the handle (first few bytes typically point to frame)
    lldb::SBError error;
    uint64_t frame_ptr = 0;
    size_t bytes_read = m_process.ReadMemory(coro_handle, &frame_ptr, 8, error);
    
    if (!error.Fail() && bytes_read == 8) {
        info.frame_address = frame_ptr;
    }
    
    return info;
}

bool AsyncDebugger::isAsyncFrame(lldb::SBFrame frame) {
    std::string func_name = frame.GetFunctionName();
    
    // Async functions have mangled names containing "async" or coroutine markers
    if (func_name.find("async") != std::string::npos) {
        return true;
    }
    
    // LLVM coroutines generate functions with "__coro_" prefix
    if (func_name.find("__coro_") != std::string::npos) {
        return true;
    }
    
    // Check if frame has coroutine-specific variables
    lldb::SBValue coro_var = frame.FindVariable("__coro_frame");
    if (coro_var.IsValid()) {
        return true;
    }
    
    // Check for Future return type
    lldb::SBValue return_val = frame.FindVariable("__coro_future");
    if (return_val.IsValid()) {
        return true;
    }
    
    return false;
}

std::string AsyncDebugger::getAwaiterExpression(lldb::SBFrame frame) {
    // Try to find the expression being awaited
    // This is stored in a local variable by the coroutine transformation
    
    lldb::SBValue awaiter = frame.FindVariable("__awaiter");
    if (awaiter.IsValid()) {
        std::string type_name = awaiter.GetTypeName();
        
        // If it's a Future, show the Future type
        if (type_name.find("Future") != std::string::npos) {
            return "await " + type_name;
        }
        
        return "await <" + type_name + ">";
    }
    
    // Check for specific async operation variables
    lldb::SBValue async_op = frame.FindVariable("__async_op");
    if (async_op.IsValid()) {
        std::string op_name = async_op.GetSummary();
        if (!op_name.empty()) {
            return "await " + op_name;
        }
    }
    
    // Fallback: just say we're at an await point
    return "await <unknown>";
}

std::vector<FutureInfo> AsyncDebugger::getActiveFutures(lldb::SBThread thread) {
    std::vector<FutureInfo> futures;
    
    // Iterate through all frames and find Future variables
    uint32_t num_frames = thread.GetNumFrames();
    for (uint32_t i = 0; i < num_frames; i++) {
        lldb::SBFrame frame = thread.GetFrameAtIndex(i);
        lldb::SBValueList variables = frame.GetVariables(true, true, false, false);
        
        uint32_t num_vars = variables.GetSize();
        for (uint32_t j = 0; j < num_vars; j++) {
            lldb::SBValue var = variables.GetValueAtIndex(j);
            std::string type_name = var.GetTypeName();
            
            // Check if this is a Future type
            if (type_name.find("Future") != std::string::npos) {
                FutureInfo info = getFutureInfo(var);
                futures.push_back(info);
            }
        }
    }
    
    return futures;
}

std::string AsyncDebugger::formatAsyncFrame(const AsyncFrameInfo& frame_info) {
    std::ostringstream ss;
    
    // Function name and location
    ss << frame_info.function_name;
    ss << " at " << frame_info.source_file << ":" << frame_info.line_number;
    
    // Future state
    if (!frame_info.future_state.empty() && frame_info.future_state != "UNKNOWN") {
        ss << " [Future: " << frame_info.future_state << "]";
    }
    
    // Suspension state
    if (frame_info.is_suspended) {
        ss << " <suspended>";
        if (!frame_info.suspend_reason.empty()) {
            ss << " - " << frame_info.suspend_reason;
        }
    } else {
        ss << " <running>";
    }
    
    return ss.str();
}

// Private helper methods

FutureInfo AsyncDebugger::readFutureState(lldb::addr_t future_addr, size_t type_size) {
    FutureInfo info;
    info.address = future_addr;
    
    if (future_addr == 0 || future_addr == LLDB_INVALID_ADDRESS) {
        info.state = "INVALID";
        return info;
    }
    
    // Read Future state from memory
    // Layout: state (4 bytes), value ptr (8 bytes), hasError (1 byte), valueSize (8 bytes)
    uint8_t buffer[32];
    lldb::SBError error;
    size_t bytes_read = m_process.ReadMemory(future_addr, buffer, sizeof(buffer), error);
    
    if (error.Fail() || bytes_read < 4) {
        info.state = "READ_ERROR";
        return info;
    }
    
    // Parse state enum (first 4 bytes)
    int32_t state_val = *reinterpret_cast<int32_t*>(buffer);
    switch (state_val) {
        case 0: info.state = "PENDING"; break;
        case 1: info.state = "READY"; break;
        case 2: info.state = "ERROR"; break;
        default: info.state = "INVALID"; break;
    }
    
    // Parse error flag (byte at offset 16)
    if (bytes_read >= 17) {
        info.has_error = buffer[16] != 0;
    }
    
    return info;
}

lldb::SBFrame AsyncDebugger::findParentAsyncFrame(lldb::SBFrame current_frame) {
    lldb::SBThread thread = current_frame.GetThread();
    uint32_t current_idx = current_frame.GetFrameID();
    
    // Search up the stack for the next async frame
    uint32_t num_frames = thread.GetNumFrames();
    for (uint32_t i = current_idx + 1; i < num_frames; i++) {
        lldb::SBFrame frame = thread.GetFrameAtIndex(i);
        if (isAsyncFrame(frame)) {
            return frame;
        }
    }
    
    return lldb::SBFrame(); // Invalid frame means no parent
}

lldb::addr_t AsyncDebugger::getCoroutineFrameAddress(lldb::SBFrame frame) {
    // Look for the coroutine frame pointer variable
    lldb::SBValue coro_frame = frame.FindVariable("__coro_frame");
    if (coro_frame.IsValid()) {
        lldb::SBError error;
        return coro_frame.GetValueAsUnsigned(error);
    }
    
    // Try alternate names
    lldb::SBValue frame_ptr = frame.FindVariable("__frame_ptr");
    if (frame_ptr.IsValid()) {
        lldb::SBError error;
        return frame_ptr.GetValueAsUnsigned(error);
    }
    
    return 0;
}

std::vector<uint32_t> AsyncDebugger::getSuspensionPoints(lldb::SBFrame frame) {
    std::vector<uint32_t> suspension_lines;
    
    // Get the function's compilation unit
    lldb::SBSymbolContext context = frame.GetSymbolContext(lldb::eSymbolContextFunction | 
                                                           lldb::eSymbolContextLineEntry);
    lldb::SBFunction function = context.GetFunction();
    
    if (!function.IsValid()) {
        return suspension_lines;
    }
    
    // Iterate through line entries looking for await/suspend calls
    lldb::SBInstructionList instructions = function.GetInstructions(m_target);
    uint32_t num_instructions = instructions.GetSize();
    
    for (uint32_t i = 0; i < num_instructions; i++) {
        lldb::SBInstruction inst = instructions.GetInstructionAtIndex(i);
        lldb::SBAddress addr = inst.GetAddress();
        lldb::SBLineEntry line_entry = addr.GetLineEntry();
        
        if (line_entry.IsValid()) {
            std::string mnemonic = inst.GetMnemonic(m_target);
            std::string operands = inst.GetOperands(m_target);
            
            // Look for calls to coroutine suspend functions
            if (mnemonic == "call" || mnemonic == "callq") {
                if (operands.find("suspend") != std::string::npos ||
                    operands.find("await") != std::string::npos) {
                    uint32_t line = line_entry.GetLine();
                    if (line > 0) {
                        suspension_lines.push_back(line);
                    }
                }
            }
        }
    }
    
    return suspension_lines;
}

} // namespace debugger
} // namespace npk
