/**
 * Aria Runtime - Process Management Implementation
 * 
 * Cross-platform process management with Unix/POSIX focus.
 */

#include "runtime/process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <signal.h>
    #include <fcntl.h>
#endif

// ============================================================================
// Internal Structures
// ============================================================================

struct AriaProcess {
#ifdef _WIN32
    HANDLE process_handle;
    HANDLE thread_handle;
    DWORD process_id;
#else
    pid_t pid;
#endif
    bool has_exited;
    int exit_code;
};

struct AriaPipe {
#ifdef _WIN32
    HANDLE read_handle;
    HANDLE write_handle;
#else
    int read_fd;
    int write_fd;
#endif
    bool read_closed;
    bool write_closed;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Get error message from errno
 */
static char* get_error_message(const char* prefix) {
    const char* err_msg = strerror(errno);
    size_t len = strlen(prefix) + strlen(err_msg) + 10;
    char* msg = (char*)malloc(len);
    if (!msg) return strdup("Out of memory");
    
    snprintf(msg, len, "%s: %s", prefix, err_msg);
    return msg;
}

// ============================================================================
// Spawn Options
// ============================================================================

AriaSpawnOptions* aria_spawn_options_create(void) {
    AriaSpawnOptions* options = (AriaSpawnOptions*)calloc(1, sizeof(AriaSpawnOptions));
    if (!options) return NULL;
    
    // All fields default to NULL/false (calloc)
    return options;
}

void aria_spawn_options_free(AriaSpawnOptions* options) {
    if (!options) return;
    free(options);
}

// ============================================================================
// Process Spawning (Unix/POSIX)
// ============================================================================

#ifndef _WIN32

AriaResult* aria_spawn(const char* command, const char** args, AriaSpawnOptions* options) {
    if (!command) {
        return aria_result_err("Command is NULL");
    }
    
    // Create process structure
    AriaProcess* proc = (AriaProcess*)calloc(1, sizeof(AriaProcess));
    if (!proc) {
        return aria_result_err("Out of memory");
    }
    
    // Fork the process
    pid_t pid = fork();
    
    if (pid < 0) {
        // Fork failed
        free(proc);
        return aria_result_err(get_error_message("fork failed"));
    }
    
    if (pid == 0) {
        // Child process
        
        // Handle pipe redirections
        if (options) {
            if (options->redirect_stdin && options->stdin_pipe) {
                int fd = aria_pipe_get_read_fd(options->stdin_pipe);
                if (fd >= 0) {
                    dup2(fd, STDIN_FILENO);
                }
            }
            
            if (options->redirect_stdout && options->stdout_pipe) {
                int fd = aria_pipe_get_write_fd(options->stdout_pipe);
                if (fd >= 0) {
                    dup2(fd, STDOUT_FILENO);
                }
            }
            
            if (options->redirect_stderr && options->stderr_pipe) {
                int fd = aria_pipe_get_write_fd(options->stderr_pipe);
                if (fd >= 0) {
                    dup2(fd, STDERR_FILENO);
                }
            }
            
            // Change working directory if specified
            if (options->cwd) {
                if (chdir(options->cwd) < 0) {
                    perror("chdir failed");
                    exit(1);
                }
            }
        }
        
        // Build argv array
        // Count arguments
        int argc = 0;
        if (args) {
            while (args[argc] != NULL) argc++;
        }
        
        // Allocate argv (command + args + NULL)
        char** argv = (char**)malloc((argc + 2) * sizeof(char*));
        if (!argv) {
            perror("malloc failed");
            exit(1);
        }
        
        argv[0] = (char*)command;
        for (int i = 0; i < argc; i++) {
            argv[i + 1] = (char*)args[i];
        }
        argv[argc + 1] = NULL;
        
        // Execute
        if (options && options->env) {
            execve(command, argv, (char**)options->env);
        } else {
            execv(command, argv);
        }
        
        // If we get here, exec failed
        perror("exec failed");
        exit(127);
    }
    
    // Parent process
    proc->pid = pid;
    proc->has_exited = false;
    proc->exit_code = 0;
    
    // Create process info
    AriaProcessInfo* info = (AriaProcessInfo*)malloc(sizeof(AriaProcessInfo));
    if (!info) {
        free(proc);
        return aria_result_err("Out of memory");
    }
    
    info->pid = (int64_t)pid;
    info->handle = proc;
    
    return aria_result_ok(info, sizeof(AriaProcessInfo));
}

#else

// Windows implementation (basic stub)
AriaResult* aria_spawn(const char* command, const char** args, AriaSpawnOptions* options) {
    return aria_result_err("aria_spawn not yet implemented on Windows");
}

#endif

// ============================================================================
// Process Control
// ============================================================================

#ifndef _WIN32

int aria_process_wait(AriaProcess* process) {
    if (!process) return -1;
    
    if (process->has_exited) {
        return process->exit_code;
    }
    
    int status;
    pid_t result = waitpid(process->pid, &status, 0);
    
    if (result < 0) {
        return -1;
    }
    
    process->has_exited = true;
    
    if (WIFEXITED(status)) {
        process->exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        process->exit_code = 128 + WTERMSIG(status);
    } else {
        process->exit_code = -1;
    }
    
    return process->exit_code;
}

bool aria_process_is_running(AriaProcess* process) {
    if (!process) return false;
    
    if (process->has_exited) {
        return false;
    }
    
    int status;
    pid_t result = waitpid(process->pid, &status, WNOHANG);
    
    if (result == 0) {
        // Still running
        return true;
    } else if (result > 0) {
        // Process exited
        process->has_exited = true;
        
        if (WIFEXITED(status)) {
            process->exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            process->exit_code = 128 + WTERMSIG(status);
        }
        
        return false;
    }
    
    // Error
    return false;
}

bool aria_process_get_exit_code(AriaProcess* process, int* exit_code) {
    if (!process || !exit_code) return false;
    
    if (!process->has_exited) {
        // Check if it has exited without blocking
        aria_process_is_running(process);
    }
    
    if (process->has_exited) {
        *exit_code = process->exit_code;
        return true;
    }
    
    return false;
}

int aria_process_kill(AriaProcess* process, int signal) {
    if (!process) return -1;
    
    if (signal == 0) {
        signal = SIGTERM;
    }
    
    return kill(process->pid, signal);
}

int64_t aria_process_get_pid(AriaProcess* process) {
    if (!process) return -1;
    return (int64_t)process->pid;
}

#else

// Windows stubs
int aria_process_wait(AriaProcess* process) {
    return -1;
}

bool aria_process_is_running(AriaProcess* process) {
    return false;
}

bool aria_process_get_exit_code(AriaProcess* process, int* exit_code) {
    return false;
}

int aria_process_kill(AriaProcess* process, int signal) {
    return -1;
}

int64_t aria_process_get_pid(AriaProcess* process) {
    return -1;
}

#endif

void aria_process_free(AriaProcess* process) {
    if (!process) return;
    free(process);
}

// ============================================================================
// Fork and Exec
// ============================================================================

#ifndef _WIN32

AriaResult* aria_fork(void) {
    pid_t pid = fork();
    
    if (pid < 0) {
        return aria_result_err(get_error_message("fork failed"));
    }
    
    AriaForkInfo* info = (AriaForkInfo*)malloc(sizeof(AriaForkInfo));
    if (!info) {
        return aria_result_err("Out of memory");
    }
    
    if (pid == 0) {
        // Child process
        info->is_child = true;
        info->pid = 0;
        info->parent_pid = (int64_t)getppid();
    } else {
        // Parent process
        info->is_child = false;
        info->pid = (int64_t)pid;
        info->parent_pid = (int64_t)getpid();
    }
    
    return aria_result_ok(info, sizeof(AriaForkInfo));
}

int aria_exec(const char* command, const char** args) {
    if (!command) return -1;
    
    // Count arguments
    int argc = 0;
    if (args) {
        while (args[argc] != NULL) argc++;
    }
    
    // Build argv
    char** argv = (char**)malloc((argc + 2) * sizeof(char*));
    if (!argv) return -1;
    
    argv[0] = (char*)command;
    for (int i = 0; i < argc; i++) {
        argv[i + 1] = (char*)args[i];
    }
    argv[argc + 1] = NULL;
    
    execv(command, argv);
    
    // If we get here, exec failed
    free(argv);
    return -1;
}

int aria_execve(const char* command, const char** args, const char** env) {
    if (!command) return -1;
    
    // Count arguments
    int argc = 0;
    if (args) {
        while (args[argc] != NULL) argc++;
    }
    
    // Build argv
    char** argv = (char**)malloc((argc + 2) * sizeof(char*));
    if (!argv) return -1;
    
    argv[0] = (char*)command;
    for (int i = 0; i < argc; i++) {
        argv[i + 1] = (char*)args[i];
    }
    argv[argc + 1] = NULL;
    
    execve(command, argv, (char**)env);
    
    // If we get here, exec failed
    free(argv);
    return -1;
}

#else

// Windows stubs
AriaResult* aria_fork(void) {
    return aria_result_err("fork not available on Windows");
}

int aria_exec(const char* command, const char** args) {
    return -1;
}

int aria_execve(const char* command, const char** args, const char** env) {
    return -1;
}

#endif

// ============================================================================
// Pipe Communication
// ============================================================================

#ifndef _WIN32

AriaResult* aria_pipe_create(void) {
    AriaPipe* aria_pipe = (AriaPipe*)calloc(1, sizeof(AriaPipe));
    if (!aria_pipe) {
        return aria_result_err("Out of memory");
    }
    
    int fds[2];
    if (::pipe(fds) < 0) {  // Use :: to call global pipe() function
        free(aria_pipe);
        return aria_result_err(get_error_message("pipe creation failed"));
    }
    
    aria_pipe->read_fd = fds[0];
    aria_pipe->write_fd = fds[1];
    aria_pipe->read_closed = false;
    aria_pipe->write_closed = false;
    
    return aria_result_ok(aria_pipe, sizeof(AriaPipe));
}

int64_t aria_pipe_write(AriaPipe* pipe, const void* data, size_t size) {
    if (!pipe || !data || pipe->write_closed) return -1;
    
    ssize_t written = write(pipe->write_fd, data, size);
    return (int64_t)written;
}

int64_t aria_pipe_read(AriaPipe* pipe, void* buffer, size_t size) {
    if (!pipe || !buffer || pipe->read_closed) return -1;
    
    ssize_t bytes_read = read(pipe->read_fd, buffer, size);
    return (int64_t)bytes_read;
}

int aria_pipe_close_write(AriaPipe* pipe) {
    if (!pipe || pipe->write_closed) return -1;
    
    int result = close(pipe->write_fd);
    if (result == 0) {
        pipe->write_closed = true;
    }
    return result;
}

int aria_pipe_close_read(AriaPipe* pipe) {
    if (!pipe || pipe->read_closed) return -1;
    
    int result = close(pipe->read_fd);
    if (result == 0) {
        pipe->read_closed = true;
    }
    return result;
}

int aria_pipe_get_read_fd(AriaPipe* pipe) {
    if (!pipe) return -1;
    return pipe->read_fd;
}

int aria_pipe_get_write_fd(AriaPipe* pipe) {
    if (!pipe) return -1;
    return pipe->write_fd;
}

void aria_pipe_free(AriaPipe* pipe) {
    if (!pipe) return;
    
    if (!pipe->read_closed) {
        close(pipe->read_fd);
    }
    
    if (!pipe->write_closed) {
        close(pipe->write_fd);
    }
    
    free(pipe);
}

#else

// Windows stubs
AriaResult* aria_pipe_create(void) {
    return aria_result_err("Pipes not yet implemented on Windows");
}

int64_t aria_pipe_write(AriaPipe* pipe, const void* data, size_t size) {
    return -1;
}

int64_t aria_pipe_read(AriaPipe* pipe, void* buffer, size_t size) {
    return -1;
}

int aria_pipe_close_write(AriaPipe* pipe) {
    return -1;
}

int aria_pipe_close_read(AriaPipe* pipe) {
    return -1;
}

int aria_pipe_get_read_fd(AriaPipe* pipe) {
    return -1;
}

int aria_pipe_get_write_fd(AriaPipe* pipe) {
    return -1;
}

void aria_pipe_free(AriaPipe* pipe) {
    if (pipe) free(pipe);
}

#endif

// ============================================================================
// Process Information
// ============================================================================

#ifndef _WIN32

int64_t aria_get_current_pid(void) {
    return (int64_t)getpid();
}

int64_t aria_get_parent_pid(void) {
    return (int64_t)getppid();
}

#else

int64_t aria_get_current_pid(void) {
    return (int64_t)GetCurrentProcessId();
}

int64_t aria_get_parent_pid(void) {
    // Windows doesn't have a direct equivalent
    return -1;
}

#endif

void aria_process_info_free(AriaProcessInfo* info) {
    if (!info) return;
    free(info);
}

void aria_fork_info_free(AriaForkInfo* info) {
    if (!info) return;
    free(info);
}
