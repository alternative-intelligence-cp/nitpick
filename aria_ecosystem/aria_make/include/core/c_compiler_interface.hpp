#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <stdexcept>

namespace aria_make {

/**
 * CCompilerInterface - Manages invocation of C/C++ compilers (gcc/clang/g++)
 * 
 * Purpose: Compile C/C++ source files for FFI libraries that Aria programs
 *          can link against via extern blocks.
 * 
 * Responsibilities:
 * - Compile .c/.cpp files to .o object files
 * - Create static libraries (.a) from object files using ar
 * - Create shared libraries (.so/.dylib/.dll) with proper linking
 * - Support mixed C/C++ compilation in same project
 * 
 * Platform Support: Linux (gcc/clang), macOS (clang), Windows (gcc/MSVC) planned
 */
class CCompilerInterface {
public:
    /**
     * Result of a compilation operation
     */
    struct CompileResult {
        int exit_code;                          // Process exit code (0 = success)
        std::string stdout_output;               // Compiler stdout
        std::string stderr_output;               // Compiler stderr (errors/warnings)
        std::chrono::milliseconds duration;      // Compilation time
        
        bool success() const { return exit_code == 0; }
    };
    
    /**
     * Compilation task specification for C/C++ source
     */
    struct CompileTask {
        std::vector<std::string> sources;        // Input .c/.cpp files
        std::string output;                      // Output .o file
        std::vector<std::string> flags;          // Compiler flags (-O2, -fPIC, etc)
        std::vector<std::string> include_paths;  // Include directories (-I flags)
        std::vector<std::string> defines;        // Preprocessor defines (-D flags)
        bool compile_only = true;                // -c flag (compile without linking)
        bool position_independent = false;       // -fPIC for shared libraries
    };
    
    /**
     * Library creation task specification
     */
    struct LibraryTask {
        std::vector<std::string> objects;        // Input .o files
        std::string output;                      // Output library file
        bool shared = false;                     // true = .so, false = .a
        std::vector<std::string> link_libraries; // Libraries to link (-l)
        std::vector<std::string> library_paths;  // Library search paths (-L)
    };
    
    /**
     * Construct interface with path to C/C++ compiler
     * 
     * @param compiler_path Path to gcc, g++, clang, or clang++
     * @param is_cpp true for C++ mode, false for C mode
     * @throws std::runtime_error if compiler doesn't exist or isn't executable
     */
    explicit CCompilerInterface(const std::string& compiler_path, bool is_cpp = false);
    
    /**
     * Compile C/C++ source file to object file
     * 
     * Typical command: gcc -c source.c -o source.o -O2 -fPIC -I/usr/include
     * 
     * @param task Compilation specification
     * @return CompileResult with exit code, output, and timing
     * @throws std::runtime_error on fork/exec failure (not compiler errors)
     */
    CompileResult compile(const CompileTask& task);
    
    /**
     * Create static library (.a) from object files
     * 
     * Uses ar (archive) tool to bundle objects:
     * ar rcs libname.a obj1.o obj2.o obj3.o
     * 
     * @param task Library creation specification
     * @return CompileResult with exit code and output
     * @throws std::runtime_error if ar tool not found or fails
     */
    CompileResult create_static_library(const LibraryTask& task);
    
    /**
     * Create shared library (.so on Linux, .dylib on macOS, .dll on Windows)
     * 
     * Uses compiler with -shared flag:
     * gcc -shared -o libname.so obj1.o obj2.o -lm -lpthread
     * 
     * @param task Library creation specification
     * @return CompileResult with exit code and output
     * @throws std::runtime_error on execution failure
     */
    CompileResult create_shared_library(const LibraryTask& task);
    
    /**
     * Get the compiler version string
     * 
     * Executes: gcc --version or clang --version
     * 
     * @return Version string
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
    std::string compiler_path_;  // Path to gcc/clang/g++/clang++
    bool is_cpp_;                // C++ mode vs C mode
    
    /**
     * Build command-line arguments from CompileTask
     * 
     * @param task Task specification
     * @return argv array suitable for execvp
     */
    std::vector<std::string> build_compile_args(const CompileTask& task) const;
    
    /**
     * Build command-line arguments for static library creation
     * 
     * @param task Library task specification
     * @return argv array for ar command
     */
    std::vector<std::string> build_archive_args(const LibraryTask& task) const;
    
    /**
     * Build command-line arguments for shared library creation
     * 
     * @param task Library task specification
     * @return argv array for compiler with -shared
     */
    std::vector<std::string> build_shared_args(const LibraryTask& task) const;
    
    /**
     * Execute command and capture output
     * 
     * Same as CompilerInterface::execute_command but for C compiler
     * Uses fork/exec/pipe pattern for robust process control
     * 
     * @param args Command-line arguments (first is executable)
     * @return CompileResult with captured output and timing
     * @throws std::runtime_error on process creation failure
     */
    CompileResult execute_command(const std::vector<std::string>& args);
    
    /**
     * Read all data from a file descriptor until EOF
     * 
     * @param fd File descriptor to read from
     * @return Complete output as string
     */
    std::string read_pipe(int fd);
};

} // namespace aria_make
