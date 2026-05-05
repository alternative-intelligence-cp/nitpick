/**
 * Async/Await Debugging Support
 * Phase 7.4.5: Logical call stack reconstruction for async functions
 * 
 * Provides debugging facilities for Aria's async/await system including:
 * - Logical call stack reconstruction across suspension points
 * - Future/Promise state tracking
 * - Awaiter information display
 * - Suspension point visualization
 * 
 * Reference: research_029 (async/await system)
 */

#ifndef ARIA_DEBUGGER_ASYNC_DEBUGGER_H
#define ARIA_DEBUGGER_ASYNC_DEBUGGER_H

#include <lldb/API/LLDB.h>
#include <string>
#include <vector>
#include <memory>

namespace npk {
namespace debugger {

/**
 * AsyncFrameInfo - Information about an async stack frame
 * 
 * Represents one level in the logical async call stack,
 * which may span multiple physical stack frames across
 * suspension/resumption points.
 */
struct AsyncFrameInfo {
    std::string function_name;        // Async function name
    std::string source_file;          // Source file path
    uint32_t line_number;             // Current line number
    lldb::addr_t frame_pointer;       // Coroutine frame address
    lldb::addr_t future_address;      // Associated Future<T> address
    std::string future_state;         // "PENDING", "READY", "ERROR"
    bool is_suspended;                // Currently at suspension point?
    std::string suspend_reason;       // What are we awaiting?
};

/**
 * FutureInfo - State of a Future<T> object
 */
struct FutureInfo {
    lldb::addr_t address;             // Future object address
    std::string type_name;            // Future<T> type (e.g., "Future<int32>")
    std::string state;                // "PENDING", "READY", "ERROR"
    lldb::SBValue value;              // The T value (if READY)
    bool has_error;                   // TBB ERR flag
};

/**
 * CoroutineInfo - State of an LLVM coroutine
 */
struct CoroutineInfo {
    lldb::addr_t handle;              // Coroutine handle (i8*)
    bool is_done;                     // Coroutine completed?
    lldb::addr_t frame_address;       // Coroutine frame allocation
    size_t frame_size;                // Frame size in bytes
    std::vector<std::string> locals;  // Local variables in frame
};

/**
 * AsyncDebugger - Helper for debugging async/await code
 * 
 * Provides utilities for reconstructing the logical call stack
 * of async functions by traversing coroutine frames and Future chains.
 */
class AsyncDebugger {
public:
    AsyncDebugger(lldb::SBTarget target, lldb::SBProcess process);
    ~AsyncDebugger() = default;
    
    /**
     * Get the logical async call stack for the current thread
     * 
     * Reconstructs the full async call chain including:
     * - Current async function at suspension point
     * - Parent async functions that are awaiting this one
     * - Entire chain back to root synchronous caller
     * 
     * @param thread The thread to get async stack for
     * @return Vector of async frame info, root first
     */
    std::vector<AsyncFrameInfo> getAsyncCallStack(lldb::SBThread thread);
    
    /**
     * Get information about a Future<T> object
     * 
     * @param future_value SBValue pointing to Future<T>
     * @return Future state information
     */
    FutureInfo getFutureInfo(lldb::SBValue future_value);
    
    /**
     * Get information about a coroutine handle
     * 
     * @param coro_handle Address of coroutine handle (i8*)
     * @return Coroutine state information
     */
    CoroutineInfo getCoroutineInfo(lldb::addr_t coro_handle);
    
    /**
     * Check if the current frame is an async function
     * 
     * @param frame Frame to check
     * @return True if frame is executing async function code
     */
    bool isAsyncFrame(lldb::SBFrame frame);
    
    /**
     * Get the awaiter expression for a suspended async function
     * 
     * When an async function is suspended at an await point,
     * this returns what it's waiting for (e.g., "readFile(path)")
     * 
     * @param frame Suspended async frame
     * @return String describing what's being awaited
     */
    std::string getAwaiterExpression(lldb::SBFrame frame);
    
    /**
     * Get all active Future objects in the current scope
     * 
     * Searches local variables and heap for Future<T> instances
     * 
     * @param thread Thread to search
     * @return Vector of active futures
     */
    std::vector<FutureInfo> getActiveFutures(lldb::SBThread thread);
    
    /**
     * Format async stack frame for display
     * 
     * Creates a human-readable string for an async frame,
     * including suspension state and awaiter info
     * 
     * @param frame_info Async frame information
     * @return Formatted string for display
     */
    std::string formatAsyncFrame(const AsyncFrameInfo& frame_info);
    
private:
    lldb::SBTarget m_target;
    lldb::SBProcess m_process;
    
    /**
     * Read Future<T> state from memory
     * 
     * @param future_addr Address of Future object
     * @param type_size Size of T
     * @return Future state information
     */
    FutureInfo readFutureState(lldb::addr_t future_addr, size_t type_size);
    
    /**
     * Find the parent async frame that's awaiting this one
     * 
     * Traces back through coroutine frames to find the caller
     * 
     * @param current_frame Current async frame
     * @return Parent async frame, or nullptr if at root
     */
    lldb::SBFrame findParentAsyncFrame(lldb::SBFrame current_frame);
    
    /**
     * Extract coroutine frame pointer from async function
     * 
     * Async functions have a coroutine frame that persists
     * across suspension. This finds that frame address.
     * 
     * @param frame Async function frame
     * @return Coroutine frame address
     */
    lldb::addr_t getCoroutineFrameAddress(lldb::SBFrame frame);
    
    /**
     * Detect suspension points in async function
     * 
     * Identifies where await expressions occur in the code
     * 
     * @param frame Async function frame
     * @return Vector of line numbers with await points
     */
    std::vector<uint32_t> getSuspensionPoints(lldb::SBFrame frame);
};

} // namespace debugger
} // namespace npk

#endif // ARIA_DEBUGGER_ASYNC_DEBUGGER_H
