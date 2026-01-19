#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <stdexcept>

namespace aria_make {

/**
 * CompilerInterface - Manages invocation of the Aria compiler (ariac)
 * 
 * Responsibilities:
 * - Construct command-line arguments from build configuration
 * - Spawn compiler process with proper I/O handling
 * - Capture stdout/stderr for error reporting
 * - Track compilation duration for performance metrics
 * - Preserve Hex-Stream file descriptors (FD 3-5) for AGI telemetry
 * 
 * Platform Support: Linux (fork/exec), Windows support planned
 */
class CompilerInterface {
public:
    /**
     * Result of a compilation operation
     */
    struct CompileResult {
        int exit_code;                          // Process exit code (0 = success)
        std::string stdout_output;               // Compiler stdout (usually empty)
        std::string stderr_output;               // Compiler stderr (errors/warnings)
        std::chrono::milliseconds duration;      // Compilation time
        
        bool success() const { return exit_code == 0; }
    };
    
    /**
     * Compilation task specification
     */
    struct CompileTask {
        std::vector<std::string> sources;        // Input .aria files
        std::string output;                      // Output file path (-o flag)
        std::vector<std::string> flags;          // Additional flags (-O2, -Wall, etc)
        std::vector<std::string> include_paths;  // Module search paths (-I flags)
        
        // Target type derived from output extension or explicit flag
        // - If output ends with .ll → --emit-llvm
        // - If output ends with .bc → --emit-llvm-bc
        // - If output ends with .s  → --emit-asm
        // - Otherwise → executable binary (default)
    };
    
    /**
     * Construct interface with path to ariac compiler
     * 
     * @param compiler_path Absolute path to ariac binary
     * @throws std::runtime_error if compiler doesn't exist or isn't executable
     */
    explicit CompilerInterface(const std::string& compiler_path);
    
    /**
     * Compile an Aria source file or files
     * 
     * Process:
     * 1. Build argv array from task specification
     * 2. Fork child process
     * 3. In child: redirect stdout/stderr to pipes, exec ariac
     * 4. In parent: read pipes, wait for completion
     * 5. Return result with exit code and output
     * 
     * @param task Compilation specification
     * @return CompileResult with exit code, output, and timing
     * @throws std::runtime_error on fork/exec failure (not compiler errors)
     */
    CompileResult compile(const CompileTask& task);
    
    /**
     * Get the compiler version string
     * 
     * Executes: ariac --version
     * 
     * @return Version string (e.g., "0.0.7-dev")
     * @throws std::runtime_error on execution failure
     */
    std::string get_version();
    
    /**
     * Test if compiler exists and is executable
     * 
     * @return true if compiler is ready to use
     */
    bool is_available() const;

private:
    std::string compiler_path_;  // Path to ariac binary
    
    /**
     * Build command-line arguments from CompileTask
     * 
     * Format: ariac <sources...> -o <output> [flags...] [-I <include_paths...>]
     * 
     * @param task Task specification
     * @return argv array suitable for execvp (null-terminated)
     */
    std::vector<std::string> build_command_args(const CompileTask& task) const;
    
    /**
     * Execute command and capture output
     * 
     * Platform-specific process spawning:
     * - Linux: fork() + execvp() + pipe() + waitpid()
     * - Windows: CreateProcess() with pipe redirection (future)
     * 
     * @param args Command-line arguments (first is executable)
     * @return CompileResult with captured output and timing
     * @throws std::runtime_error on process creation failure
     */
    CompileResult execute_command(const std::vector<std::string>& args);
    
    /**
     * Read all data from a file descriptor until EOF
     * 
     * Used to capture stdout/stderr from compiler process.
     * Non-blocking, buffers up to 1MB per stream.
     * 
     * @param fd File descriptor to read from
     * @return Complete output as string
     */
    std::string read_pipe(int fd);
    
    /**
     * Preserve Hex-Stream file descriptors for AGI telemetry
     * 
     * FD 3: stddbg  (structured debug logs)
     * FD 4: stddati (binary input stream)
     * FD 5: stddato (binary output stream)
     * 
     * Called before execvp() to clear FD_CLOEXEC flag.
     * Only active if FDs are open in parent process.
     */
    void preserve_hex_stream_fds();
};

} // namespace aria_make
