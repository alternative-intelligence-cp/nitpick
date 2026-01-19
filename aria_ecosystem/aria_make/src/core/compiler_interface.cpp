#include "core/compiler_interface.hpp"
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

CompilerInterface::CompilerInterface(const std::string& compiler_path)
    : compiler_path_(compiler_path)
{
    if (!is_available()) {
        throw std::runtime_error(
            "Compiler not found or not executable: " + compiler_path_
        );
    }
}

bool CompilerInterface::is_available() const {
    struct stat st;
    if (stat(compiler_path_.c_str(), &st) != 0) {
        return false;  // File doesn't exist
    }
    
    // Check if regular file and executable
    if (!S_ISREG(st.st_mode)) {
        return false;
    }
    
    // Check execute permission
    if (access(compiler_path_.c_str(), X_OK) != 0) {
        return false;
    }
    
    return true;
}

std::vector<std::string> CompilerInterface::build_command_args(
    const CompileTask& task
) const {
    std::vector<std::string> args;
    
    // Start with compiler path
    args.push_back(compiler_path_);
    
    // Add source files
    for (const auto& source : task.sources) {
        args.push_back(source);
    }
    
    // Add output flag
    if (!task.output.empty()) {
        args.push_back("-o");
        args.push_back(task.output);
    }
    
    // Detect special output types from extension
    const std::string& output = task.output;
    if (output.size() >= 3) {
        std::string ext = output.substr(output.size() - 3);
        if (ext == ".ll") {
            args.push_back("--emit-llvm");
        } else if (ext == ".bc") {
            args.push_back("--emit-llvm-bc");
        } else if (output.size() >= 2 && output.substr(output.size() - 2) == ".s") {
            args.push_back("--emit-asm");
        }
    }
    
    // Add include paths
    for (const auto& include : task.include_paths) {
        args.push_back("-I");
        args.push_back(include);
    }
    
    // Add additional flags
    for (const auto& flag : task.flags) {
        args.push_back(flag);
    }
    
    return args;
}

std::string CompilerInterface::read_pipe(int fd) {
    std::string result;
    char buffer[4096];
    ssize_t bytes_read;
    
    // Set non-blocking mode to prevent hanging
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    while (true) {
        bytes_read = read(fd, buffer, sizeof(buffer));
        
        if (bytes_read > 0) {
            result.append(buffer, bytes_read);
        } else if (bytes_read == 0) {
            // EOF reached
            break;
        } else {
            // Error or would block
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more data available right now, wait a bit
                usleep(1000);  // 1ms
                
                // Try one more time for EOF
                bytes_read = read(fd, buffer, sizeof(buffer));
                if (bytes_read <= 0) {
                    break;
                }
                result.append(buffer, bytes_read);
            } else {
                // Real error
                break;
            }
        }
        
        // Safety limit: 1MB per stream
        if (result.size() > 1024 * 1024) {
            result += "\n[... output truncated at 1MB ...]";
            break;
        }
    }
    
    return result;
}

void CompilerInterface::preserve_hex_stream_fds() {
    // Preserve FD 3-5 for Hex-Stream I/O (AGI telemetry)
    // FD 3: stddbg  (structured debug logs)
    // FD 4: stddati (binary input stream)
    // FD 5: stddato (binary output stream)
    
    for (int fd = 3; fd <= 5; fd++) {
        int flags = fcntl(fd, F_GETFD);
        if (flags != -1) {
            // Clear FD_CLOEXEC to allow inheritance across exec
            fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC);
        }
    }
}

CompilerInterface::CompileResult CompilerInterface::execute_command(
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
        // Fork failed
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        throw std::runtime_error(
            std::string("Failed to fork process: ") + strerror(errno)
        );
    }
    
    if (pid == 0) {
        // Child process
        
        // Redirect stdout to pipe
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        
        // Redirect stderr to pipe
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        
        // Preserve Hex-Stream file descriptors
        preserve_hex_stream_fds();
        
        // Build argv array for execvp
        std::vector<char*> argv;
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);  // execvp requires null terminator
        
        // Execute compiler
        execvp(argv[0], argv.data());
        
        // If we get here, execvp failed
        std::cerr << "Failed to execute " << args[0] << ": " 
                  << strerror(errno) << std::endl;
        _exit(127);  // Use _exit to avoid flushing parent's buffers
    }
    
    // Parent process
    
    // Close write ends of pipes (child is the only writer)
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    
    // Set pipes to non-blocking mode
    int stdout_flags = fcntl(stdout_pipe[0], F_GETFL, 0);
    fcntl(stdout_pipe[0], F_SETFL, stdout_flags | O_NONBLOCK);
    int stderr_flags = fcntl(stderr_pipe[0], F_GETFL, 0);
    fcntl(stderr_pipe[0], F_SETFL, stderr_flags | O_NONBLOCK);
    
    // Read from pipes while child is running
    std::string stdout_output;
    std::string stderr_output;
    char buffer[4096];
    bool stdout_open = true;
    bool stderr_open = true;
    
    while (stdout_open || stderr_open) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        int max_fd = -1;
        
        if (stdout_open) {
            FD_SET(stdout_pipe[0], &read_fds);
            max_fd = std::max(max_fd, stdout_pipe[0]);
        }
        if (stderr_open) {
            FD_SET(stderr_pipe[0], &read_fds);
            max_fd = std::max(max_fd, stderr_pipe[0]);
        }
        
        if (max_fd == -1) break;
        
        // Wait for data with timeout
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000; // 10ms
        
        int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        
        if (ready > 0) {
            // Read from stdout
            if (stdout_open && FD_ISSET(stdout_pipe[0], &read_fds)) {
                ssize_t n = read(stdout_pipe[0], buffer, sizeof(buffer));
                if (n > 0) {
                    stdout_output.append(buffer, n);
                } else {
                    stdout_open = false;
                }
            }
            
            // Read from stderr
            if (stderr_open && FD_ISSET(stderr_pipe[0], &read_fds)) {
                ssize_t n = read(stderr_pipe[0], buffer, sizeof(buffer));
                if (n > 0) {
                    stderr_output.append(buffer, n);
                } else {
                    stderr_open = false;
                }
            }
        }
        
        // Check if child has exited
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            // Child exited, do final reads and break
            ssize_t n;
            while ((n = read(stdout_pipe[0], buffer, sizeof(buffer))) > 0) {
                stdout_output.append(buffer, n);
            }
            while ((n = read(stderr_pipe[0], buffer, sizeof(buffer))) > 0) {
                stderr_output.append(buffer, n);
            }
            break;
        }
        
        // Safety limits
        if (stdout_output.size() > 10 * 1024 * 1024) {
            stdout_output += "\n[... stdout truncated at 10MB ...]";
            stdout_open = false;
        }
        if (stderr_output.size() > 10 * 1024 * 1024) {
            stderr_output += "\n[... stderr truncated at 10MB ...]";
            stderr_open = false;
        }
    }
    
    // Close read ends
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);
    
    // Get final status if we haven't already
    int status;
    waitpid(pid, &status, 0);
    
    // Calculate duration
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    );
    
    // Extract exit code
    int exit_code;
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        // Process was killed by signal
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

CompilerInterface::CompileResult CompilerInterface::compile(
    const CompileTask& task
) {
    if (task.sources.empty()) {
        throw std::runtime_error("CompileTask must have at least one source file");
    }
    
    if (task.output.empty()) {
        throw std::runtime_error("CompileTask must specify output file");
    }
    
    // Build command-line arguments
    std::vector<std::string> args = build_command_args(task);
    
    // Execute compiler
    return execute_command(args);
}

std::string CompilerInterface::get_version() {
    std::vector<std::string> args = {compiler_path_, "--version"};
    
    CompileResult result = execute_command(args);
    
    if (result.exit_code != 0) {
        throw std::runtime_error(
            "Failed to get compiler version: " + result.stderr_output
        );
    }
    
    // Return stdout (version string)
    std::string version = result.stdout_output;
    
    // Trim whitespace
    version.erase(0, version.find_first_not_of(" \t\n\r"));
    version.erase(version.find_last_not_of(" \t\n\r") + 1);
    
    return version;
}

} // namespace aria_make
