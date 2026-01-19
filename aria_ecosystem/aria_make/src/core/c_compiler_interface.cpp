#include "core/c_compiler_interface.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace aria_make {

CCompilerInterface::CCompilerInterface(const std::string& compiler_path, bool is_cpp)
    : compiler_path_(compiler_path), is_cpp_(is_cpp)
{
    if (!is_available()) {
        throw std::runtime_error(
            "C/C++ compiler not found or not executable: " + compiler_path_
        );
    }
}

bool CCompilerInterface::is_available() const {
    struct stat st;
    if (stat(compiler_path_.c_str(), &st) != 0) {
        return false;
    }
    
    if (!S_ISREG(st.st_mode)) {
        return false;
    }
    
    if (access(compiler_path_.c_str(), X_OK) != 0) {
        return false;
    }
    
    return true;
}

std::vector<std::string> CCompilerInterface::build_compile_args(
    const CompileTask& task
) const {
    std::vector<std::string> args;
    
    // Compiler
    args.push_back(compiler_path_);
    
    // Compile only flag
    if (task.compile_only) {
        args.push_back("-c");
    }
    
    // Position independent code for shared libraries
    if (task.position_independent) {
        args.push_back("-fPIC");
    }
    
    // Add source files
    for (const auto& source : task.sources) {
        args.push_back(source);
    }
    
    // Output file
    if (!task.output.empty()) {
        args.push_back("-o");
        args.push_back(task.output);
    }
    
    // Include paths
    for (const auto& include : task.include_paths) {
        args.push_back("-I");
        args.push_back(include);
    }
    
    // Preprocessor defines
    for (const auto& define : task.defines) {
        args.push_back("-D");
        args.push_back(define);
    }
    
    // Additional flags
    for (const auto& flag : task.flags) {
        args.push_back(flag);
    }
    
    return args;
}

std::vector<std::string> CCompilerInterface::build_archive_args(
    const LibraryTask& task
) const {
    std::vector<std::string> args;
    
    // ar command
    args.push_back("ar");
    
    // Standard flags: r = insert/replace, c = create, s = index
    args.push_back("rcs");
    
    // Output library
    args.push_back(task.output);
    
    // Object files
    for (const auto& obj : task.objects) {
        args.push_back(obj);
    }
    
    return args;
}

std::vector<std::string> CCompilerInterface::build_shared_args(
    const LibraryTask& task
) const {
    std::vector<std::string> args;
    
    // Compiler
    args.push_back(compiler_path_);
    
    // Shared library flag
    args.push_back("-shared");
    
    // Output file
    args.push_back("-o");
    args.push_back(task.output);
    
    // Object files
    for (const auto& obj : task.objects) {
        args.push_back(obj);
    }
    
    // Library search paths
    for (const auto& lib_path : task.library_paths) {
        args.push_back("-L");
        args.push_back(lib_path);
    }
    
    // Libraries to link
    for (const auto& lib : task.link_libraries) {
        args.push_back("-l");
        args.push_back(lib);
    }
    
    return args;
}

std::string CCompilerInterface::read_pipe(int fd) {
    std::string result;
    char buffer[4096];
    ssize_t bytes_read;
    
    // Set non-blocking mode
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    while (true) {
        bytes_read = read(fd, buffer, sizeof(buffer));
        
        if (bytes_read > 0) {
            result.append(buffer, bytes_read);
        } else if (bytes_read == 0) {
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
                bytes_read = read(fd, buffer, sizeof(buffer));
                if (bytes_read <= 0) {
                    break;
                }
                result.append(buffer, bytes_read);
            } else {
                break;
            }
        }
        
        // Safety limit: 1MB
        if (result.size() > 1024 * 1024) {
            result += "\n[... output truncated at 1MB ...]";
            break;
        }
    }
    
    return result;
}

CCompilerInterface::CompileResult CCompilerInterface::execute_command(
    const std::vector<std::string>& args
) {
    if (args.empty()) {
        throw std::runtime_error("Cannot execute empty command");
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Create pipes for stdout and stderr
    int stdout_pipe[2];
    int stderr_pipe[2];
    
    if (pipe(stdout_pipe) != 0) {
        throw std::runtime_error(
            std::string("Failed to create stdout pipe: ") + strerror(errno)
        );
    }
    
    if (pipe(stderr_pipe) != 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        throw std::runtime_error(
            std::string("Failed to create stderr pipe: ") + strerror(errno)
        );
    }
    
    // Fork child process
    pid_t pid = fork();
    
    if (pid < 0) {
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        throw std::runtime_error(
            std::string("Failed to fork process: ") + strerror(errno)
        );
    }
    
    if (pid == 0) {
        // Child process
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        
        // Build argv array
        std::vector<char*> argv;
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        
        // Execute compiler
        execvp(argv[0], argv.data());
        
        // If we get here, execvp failed
        std::cerr << "Failed to execute " << args[0] << ": " 
                  << strerror(errno) << std::endl;
        _exit(127);
    }
    
    // Parent process
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    
    std::string stdout_output = read_pipe(stdout_pipe[0]);
    std::string stderr_output = read_pipe(stderr_pipe[0]);
    
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);
    
    // Wait for child
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        throw std::runtime_error(
            std::string("Failed to wait for child process: ") + strerror(errno)
        );
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    );
    
    int exit_code;
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        exit_code = 128 + WTERMSIG(status);
    } else {
        exit_code = -1;
    }
    
    return CompileResult{
        exit_code,
        stdout_output,
        stderr_output,
        duration
    };
}

CCompilerInterface::CompileResult CCompilerInterface::compile(
    const CompileTask& task
) {
    if (task.sources.empty()) {
        throw std::runtime_error("CompileTask must have at least one source file");
    }
    
    if (task.output.empty()) {
        throw std::runtime_error("CompileTask must specify output file");
    }
    
    std::vector<std::string> args = build_compile_args(task);
    return execute_command(args);
}

CCompilerInterface::CompileResult CCompilerInterface::create_static_library(
    const LibraryTask& task
) {
    if (task.objects.empty()) {
        throw std::runtime_error("LibraryTask must have at least one object file");
    }
    
    if (task.output.empty()) {
        throw std::runtime_error("LibraryTask must specify output file");
    }
    
    std::vector<std::string> args = build_archive_args(task);
    return execute_command(args);
}

CCompilerInterface::CompileResult CCompilerInterface::create_shared_library(
    const LibraryTask& task
) {
    if (task.objects.empty()) {
        throw std::runtime_error("LibraryTask must have at least one object file");
    }
    
    if (task.output.empty()) {
        throw std::runtime_error("LibraryTask must specify output file");
    }
    
    std::vector<std::string> args = build_shared_args(task);
    return execute_command(args);
}

std::string CCompilerInterface::get_version() {
    std::vector<std::string> args = {compiler_path_, "--version"};
    
    CompileResult result = execute_command(args);
    
    if (result.exit_code != 0) {
        throw std::runtime_error(
            "Failed to get compiler version: " + result.stderr_output
        );
    }
    
    std::string version = result.stdout_output;
    version.erase(0, version.find_first_not_of(" \t\n\r"));
    version.erase(version.find_last_not_of(" \t\n\r") + 1);
    
    return version;
}

} // namespace aria_make
