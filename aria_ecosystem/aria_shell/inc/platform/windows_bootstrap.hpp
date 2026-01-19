/**
 * Windows Bootstrap Protocol - Handle Mapping for Hex-Stream Topology
 * 
 * Implements the __ARIA_FD_MAP protocol to enable six-stream I/O on Windows.
 * 
 * PROBLEM: Windows has no FD indices - only opaque HANDLEs. A child process
 * receiving inherited handles has no way to know which is stddbg vs stddati.
 * 
 * SOLUTION: Transmit a map via environment variable:
 *   __ARIA_FD_MAP=3:0x1A4;4:0x1B8;5:0x2C0
 * 
 * This maps logical indices (3=stddbg, 4=stddati, 5=stddato) to actual
 * Windows HANDLE values that the child can use.
 */

#ifndef ARIASH_WINDOWS_BOOTSTRAP_HPP
#define ARIASH_WINDOWS_BOOTSTRAP_HPP

#ifdef _WIN32

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace ariash {
namespace platform {

/**
 * Windows Handle Map for Hex-Stream topology
 * 
 * Stores the mapping of logical stream indices to Windows HANDLEs.
 */
struct WindowsHandleMap {
    HANDLE hStdIn    = INVALID_HANDLE_VALUE;   // Stream 0
    HANDLE hStdOut   = INVALID_HANDLE_VALUE;   // Stream 1
    HANDLE hStdErr   = INVALID_HANDLE_VALUE;   // Stream 2
    HANDLE hStdDbg   = INVALID_HANDLE_VALUE;   // Stream 3
    HANDLE hStdDatI  = INVALID_HANDLE_VALUE;   // Stream 4
    HANDLE hStdDatO  = INVALID_HANDLE_VALUE;   // Stream 5
    
    /**
     * Serialize to __ARIA_FD_MAP format
     * 
     * Example: "3:0x1A4;4:0x1B8;5:0x2C0"
     */
    std::wstring serialize() const;
    
    /**
     * Parse from __ARIA_FD_MAP format
     * 
     * @return true if parsing succeeded
     */
    bool parse(const std::wstring& mapString);
    
    /**
     * Validate all handles are real
     */
    bool validateHandles() const;
};

/**
 * Windows Process Launcher with Handle Mapping
 * 
 * Uses STARTUPINFOEX to ensure only whitelisted handles are inherited.
 */
class WindowsBootstrap {
public:
    WindowsBootstrap();
    ~WindowsBootstrap();
    
    /**
     * Create pipes for all six streams
     * 
     * @return true if successful
     */
    bool createPipes();
    
    /**
     * Get handle map for child process
     */
    const WindowsHandleMap& getChildHandles() const { return childHandles_; }
    
    /**
     * Get handle map for parent process
     */
    const WindowsHandleMap& getParentHandles() const { return parentHandles_; }
    
    /**
     * Launch child process with handle mapping
     * 
     * @param commandLine Command to execute
     * @param useEnvVar Use environment variable (default) vs CLI flag
     * @return Process handle if successful, INVALID_HANDLE_VALUE otherwise
     */
    HANDLE launchProcess(const std::wstring& commandLine, bool useEnvVar = true);
    
    /**
     * Close parent-side handles (call after child launched)
     */
    void closeParentHandles();
    
    /**
     * Close all handles
     */
    void closeAll();
    
private:
    WindowsHandleMap childHandles_;   // Child's ends of pipes
    WindowsHandleMap parentHandles_;  // Parent's ends of pipes
    
    PROCESS_INFORMATION processInfo_;
    LPPROC_THREAD_ATTRIBUTE_LIST attributeList_;
    
    /**
     * Create STARTUPINFOEX with handle whitelist
     */
    bool createStartupInfo(STARTUPINFOEXW& si);
    
    /**
     * Build environment block with __ARIA_FD_MAP
     */
    std::vector<wchar_t> buildEnvironmentBlock(const WindowsHandleMap& handles);
    
    /**
     * Build command line with --aria-fd-map flag
     */
    std::wstring buildCommandLineWithFlag(const std::wstring& cmdLine,
                                          const WindowsHandleMap& handles);
};

/**
 * Runtime-side Handle Map Consumer
 * 
 * Called during Aria runtime initialization to retrieve handles.
 */
class WindowsHandleMapConsumer {
public:
    /**
     * Retrieve handle map from environment or command line
     * 
     * @param envVarFirst Check environment variable before CLI (default true)
     * @return Handle map if found, empty map otherwise
     */
    static WindowsHandleMap retrieveHandleMap(bool envVarFirst = true);
    
    /**
     * Parse __ARIA_FD_MAP from environment variable
     */
    static WindowsHandleMap parseFromEnvironment();
    
    /**
     * Parse --aria-fd-map from command line
     * 
     * Also removes the flag from argv to prevent application pollution.
     */
    static WindowsHandleMap parseFromCommandLine();
    
    /**
     * Initialize Aria runtime streams with handle map
     * 
     * This would be called from aria_runtime_init() in the actual runtime.
     */
    static bool initializeStreams(const WindowsHandleMap& handles);
};

} // namespace platform
} // namespace ariash

#endif // _WIN32
#endif // ARIASH_WINDOWS_BOOTSTRAP_HPP
