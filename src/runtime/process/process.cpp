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
    #include <spawn.h>
    #include <sys/syscall.h>  // For close_range syscall

    // close_range syscall wrapper (Linux 5.9+)
    #ifdef __linux__
        #ifndef SYS_close_range
            #define SYS_close_range 436  // x86_64
        #endif

        static int close_range_wrapper(unsigned int first, unsigned int last, unsigned int flags) {
            return syscall(SYS_close_range, first, last, flags);
        }
    #endif
#endif

// Aria six-stream FD constants
#define ARIA_FD_STDIN   0
#define ARIA_FD_STDOUT  1
#define ARIA_FD_STDERR  2
#define ARIA_FD_STDDBG  3
#define ARIA_FD_STDDATI 4
#define ARIA_FD_STDDATO 5
#define ARIA_FD_FIRST_FREE 6

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

// ============================================================================
// Fork Safety — Atfork Callback Registration
// ============================================================================

#ifndef _WIN32

#include <pthread.h>

#define ARIA_MAX_ATFORK_HANDLERS 16

struct AriaAtforkEntry {
    AriaAtforkCallback pre_fork;
    AriaAtforkCallback parent_post;
    AriaAtforkCallback child_post;
};

static AriaAtforkEntry g_atfork_handlers[ARIA_MAX_ATFORK_HANDLERS];
static int g_atfork_count = 0;
static bool g_atfork_installed = false;

static void npk_atfork_pre(void) {
    // Acquire locks in registration order
    for (int i = 0; i < g_atfork_count; i++) {
        if (g_atfork_handlers[i].pre_fork) {
            g_atfork_handlers[i].pre_fork();
        }
    }
}

static void npk_atfork_parent(void) {
    // Release locks in reverse order
    for (int i = g_atfork_count - 1; i >= 0; i--) {
        if (g_atfork_handlers[i].parent_post) {
            g_atfork_handlers[i].parent_post();
        }
    }
}

static void npk_atfork_child(void) {
    // Release/reinitialize in reverse order
    for (int i = g_atfork_count - 1; i >= 0; i--) {
        if (g_atfork_handlers[i].child_post) {
            g_atfork_handlers[i].child_post();
        }
    }
}

int npk_atfork_register(AriaAtforkCallback pre_fork,
                         AriaAtforkCallback parent_post,
                         AriaAtforkCallback child_post) {
    if (g_atfork_count >= ARIA_MAX_ATFORK_HANDLERS) {
        return -1;
    }

    // Install the pthread_atfork handler on first registration
    if (!g_atfork_installed) {
        if (pthread_atfork(npk_atfork_pre, npk_atfork_parent, npk_atfork_child) != 0) {
            return -1;
        }
        g_atfork_installed = true;
    }

    g_atfork_handlers[g_atfork_count].pre_fork = pre_fork;
    g_atfork_handlers[g_atfork_count].parent_post = parent_post;
    g_atfork_handlers[g_atfork_count].child_post = child_post;
    g_atfork_count++;
    return 0;
}

#else

int npk_atfork_register(AriaAtforkCallback pre_fork,
                         AriaAtforkCallback parent_post,
                         AriaAtforkCallback child_post) {
    (void)pre_fork; (void)parent_post; (void)child_post;
    return 0;  // No-op on Windows (no fork)
}

#endif

// ============================================================================
// Spawn Options
// ============================================================================

AriaSpawnOptions* npk_spawn_options_create(void) {
    AriaSpawnOptions* options = (AriaSpawnOptions*)calloc(1, sizeof(AriaSpawnOptions));
    if (!options) return NULL;

    // Default: close extra FDs for security (FDs >= 6)
    // This prevents leaking parent's file descriptors to child
    options->close_extra_fds = true;

    // All other fields default to NULL/false (calloc)
    return options;
}

static void npk_spawn_options_free(AriaSpawnOptions* options) {
    if (!options) return;
    free(options);
}

// ============================================================================
// Process Spawning (Unix/POSIX)
// ============================================================================

#ifndef _WIN32

AriaResult* npk_spawn(const char* command, const char** args, AriaSpawnOptions* options) {
    if (!command) {
        return npk_result_err("Command is NULL");
    }
    
    // Create process structure
    AriaProcess* proc = (AriaProcess*)calloc(1, sizeof(AriaProcess));
    if (!proc) {
        return npk_result_err("Out of memory");
    }
    
    // Build argv array before spawn
    int argc = 0;
    if (args) {
        while (args[argc] != NULL) argc++;
    }
    
    char** argv = (char**)malloc((argc + 2) * sizeof(char*));
    if (!argv) {
        free(proc);
        return npk_result_err("Out of memory");
    }
    
    argv[0] = (char*)command;
    for (int i = 0; i < argc; i++) {
        argv[i + 1] = (char*)args[i];
    }
    argv[argc + 1] = NULL;
    
    // Set up posix_spawn file actions for FD redirections
    posix_spawn_file_actions_t file_actions;
    posix_spawn_file_actions_init(&file_actions);
    
    if (options) {
        // Aria Six-Stream I/O Topology (ARIA-020):
        //   FD 0: stdin   - Standard input
        //   FD 1: stdout  - Standard output
        //   FD 2: stderr  - Standard error
        //   FD 3: stddbg  - Debug output (structured diagnostics)
        //   FD 4: stddati - Data input (binary/structured data)
        //   FD 5: stddato - Data output (binary/structured data)
        
        // FD 0: stdin (read from pipe)
        if (options->redirect_stdin && options->stdin_pipe) {
            int fd = npk_pipe_get_read_fd(options->stdin_pipe);
            if (fd >= 0 && fd != ARIA_FD_STDIN) {
                posix_spawn_file_actions_adddup2(&file_actions, fd, ARIA_FD_STDIN);
            }
        }

        // FD 1: stdout (write to pipe)
        if (options->redirect_stdout && options->stdout_pipe) {
            int fd = npk_pipe_get_write_fd(options->stdout_pipe);
            if (fd >= 0 && fd != ARIA_FD_STDOUT) {
                posix_spawn_file_actions_adddup2(&file_actions, fd, ARIA_FD_STDOUT);
            }
        }

        // FD 2: stderr (write to pipe)
        if (options->redirect_stderr && options->stderr_pipe) {
            int fd = npk_pipe_get_write_fd(options->stderr_pipe);
            if (fd >= 0 && fd != ARIA_FD_STDERR) {
                posix_spawn_file_actions_adddup2(&file_actions, fd, ARIA_FD_STDERR);
            }
        }

        // FD 3: stddbg - debug output (write to pipe)
        if (options->redirect_stddbg && options->stddbg_pipe) {
            int fd = npk_pipe_get_write_fd(options->stddbg_pipe);
            if (fd >= 0 && fd != ARIA_FD_STDDBG) {
                posix_spawn_file_actions_adddup2(&file_actions, fd, ARIA_FD_STDDBG);
            }
        }

        // FD 4: stddati - data input (read from pipe)
        if (options->redirect_stddati && options->stddati_pipe) {
            int fd = npk_pipe_get_read_fd(options->stddati_pipe);
            if (fd >= 0 && fd != ARIA_FD_STDDATI) {
                posix_spawn_file_actions_adddup2(&file_actions, fd, ARIA_FD_STDDATI);
            }
        }

        // FD 5: stddato - data output (write to pipe)
        if (options->redirect_stddato && options->stddato_pipe) {
            int fd = npk_pipe_get_write_fd(options->stddato_pipe);
            if (fd >= 0 && fd != ARIA_FD_STDDATO) {
                posix_spawn_file_actions_adddup2(&file_actions, fd, ARIA_FD_STDDATO);
            }
        }

        // Security: Close all FDs >= 6 to prevent leaking parent's FDs
        if (options->close_extra_fds) {
            posix_spawn_file_actions_addclosefrom_np(&file_actions, ARIA_FD_FIRST_FREE);
        }

        // Change working directory if specified
        if (options->cwd) {
            posix_spawn_file_actions_addchdir_np(&file_actions, options->cwd);
        }
    }
    
    // Set up spawn attributes
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    
    // Spawn the process — no fork(), no deadlock risk from inherited mutexes
    pid_t pid;
    int spawn_err;
    if (options && options->env) {
        spawn_err = posix_spawn(&pid, command, &file_actions, &attr,
                                argv, (char**)options->env);
    } else {
        extern char **environ;
        spawn_err = posix_spawn(&pid, command, &file_actions, &attr,
                                argv, environ);
    }
    
    // Clean up spawn resources
    posix_spawnattr_destroy(&attr);
    posix_spawn_file_actions_destroy(&file_actions);
    free(argv);
    
    if (spawn_err != 0) {
        free(proc);
        errno = spawn_err;
        return npk_result_err(get_error_message("posix_spawn failed"));
    }
    
    // Parent process
    proc->pid = pid;
    proc->has_exited = false;
    proc->exit_code = 0;
    
    // Create process info
    AriaProcessInfo* info = (AriaProcessInfo*)malloc(sizeof(AriaProcessInfo));
    if (!info) {
        free(proc);
        return npk_result_err("Out of memory");
    }
    
    info->pid = (int64_t)pid;
    info->handle = proc;
    
    return npk_result_ok(info, sizeof(AriaProcessInfo));
}

#else

// Windows implementation (basic stub)
AriaResult* npk_spawn(const char* command, const char** args, AriaSpawnOptions* options) {
    return npk_result_err("npk_spawn not yet implemented on Windows");
}

#endif

// ============================================================================
// Process Control
// ============================================================================

#ifndef _WIN32

int npk_process_wait(AriaProcess* process) {
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

bool npk_process_is_running(AriaProcess* process) {
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

bool npk_process_get_exit_code(AriaProcess* process, int* exit_code) {
    if (!process || !exit_code) return false;
    
    if (!process->has_exited) {
        // Check if it has exited without blocking
        npk_process_is_running(process);
    }
    
    if (process->has_exited) {
        *exit_code = process->exit_code;
        return true;
    }
    
    return false;
}

int npk_process_kill(AriaProcess* process, int signal) {
    if (!process) return -1;
    
    if (signal == 0) {
        signal = SIGTERM;
    }
    
    return kill(process->pid, signal);
}

int64_t npk_process_get_pid(AriaProcess* process) {
    if (!process) return -1;
    return (int64_t)process->pid;
}

#else

// Windows stubs
int npk_process_wait(AriaProcess* process) {
    return -1;
}

bool npk_process_is_running(AriaProcess* process) {
    return false;
}

bool npk_process_get_exit_code(AriaProcess* process, int* exit_code) {
    return false;
}

int npk_process_kill(AriaProcess* process, int signal) {
    return -1;
}

int64_t npk_process_get_pid(AriaProcess* process) {
    return -1;
}

#endif

static void npk_process_free(AriaProcess* process) {
    if (!process) return;
    free(process);
}

// ============================================================================
// Fork and Exec
// ============================================================================

#ifndef _WIN32

// Static storage for fork result to avoid malloc() after fork()
// This prevents deadlock in multi-threaded programs where another
// thread might hold malloc's lock when we fork
static __thread AriaForkInfo fork_result_buffer;

AriaResult* npk_fork(void) {
    // Pre-allocate result structure BEFORE fork to avoid malloc deadlock.
    // Runtime mutex safety is handled by pthread_atfork handlers registered
    // via npk_atfork_register() — GC, thread pools, etc. acquire their locks
    // before fork and release them in both parent and child afterward.
    fork_result_buffer.is_child = false;
    fork_result_buffer.pid = 0;
    fork_result_buffer.parent_pid = 0;
    
    pid_t pid = fork();
    
    if (pid < 0) {
        return npk_result_err(get_error_message("fork failed"));
    }
    
    if (pid == 0) {
        // Child process - populate result
        fork_result_buffer.is_child = true;
        fork_result_buffer.pid = 0;
        fork_result_buffer.parent_pid = (int64_t)getppid();
    } else {
        // Parent process - populate result
        fork_result_buffer.is_child = false;
        fork_result_buffer.pid = (int64_t)pid;
        fork_result_buffer.parent_pid = (int64_t)getpid();
    }
    
    // Return pointer to thread-local buffer
    // Caller must copy this before next fork() call
    return npk_result_ok(&fork_result_buffer, sizeof(AriaForkInfo));
}

int npk_exec(const char* command, const char** args) {
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

int npk_execve(const char* command, const char** args, const char** env) {
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
AriaResult* npk_fork(void) {
    return npk_result_err("fork not available on Windows");
}

int npk_exec(const char* command, const char** args) {
    return -1;
}

int npk_execve(const char* command, const char** args, const char** env) {
    return -1;
}

#endif

// ============================================================================
// Pipe Communication
// ============================================================================

#ifndef _WIN32

AriaResult* npk_pipe_create(void) {
    AriaPipe* npk_pipe = (AriaPipe*)calloc(1, sizeof(AriaPipe));
    if (!npk_pipe) {
        return npk_result_err("Out of memory");
    }

    int fds[2];

    // Use pipe2() with O_CLOEXEC for thread-safe pipe creation
    // This ensures pipes are atomically created with close-on-exec flag,
    // preventing FD leaks to forked children in multithreaded programs
    #ifdef __linux__
        if (pipe2(fds, O_CLOEXEC) < 0) {
            free(npk_pipe);
            return npk_result_err(get_error_message("pipe2 creation failed"));
        }
    #else
        // Fallback for non-Linux: create pipe then set close-on-exec
        if (::pipe(fds) < 0) {
            free(npk_pipe);
            return npk_result_err(get_error_message("pipe creation failed"));
        }
        // Set close-on-exec flags (not atomic, but best we can do)
        fcntl(fds[0], F_SETFD, FD_CLOEXEC);
        fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    #endif

    npk_pipe->read_fd = fds[0];
    npk_pipe->write_fd = fds[1];
    npk_pipe->read_closed = false;
    npk_pipe->write_closed = false;

    return npk_result_ok(npk_pipe, sizeof(AriaPipe));
}

int64_t npk_pipe_write(AriaPipe* pipe, const void* data, size_t size) {
    if (!pipe || !data || pipe->write_closed) return -1;
    
    ssize_t written = write(pipe->write_fd, data, size);
    return (int64_t)written;
}

int64_t npk_pipe_read(AriaPipe* pipe, void* buffer, size_t size) {
    if (!pipe || !buffer || pipe->read_closed) return -1;
    
    ssize_t bytes_read = read(pipe->read_fd, buffer, size);
    return (int64_t)bytes_read;
}

int npk_pipe_close_write(AriaPipe* pipe) {
    if (!pipe || pipe->write_closed) return -1;
    
    int result = close(pipe->write_fd);
    if (result == 0) {
        pipe->write_closed = true;
    }
    return result;
}

int npk_pipe_close_read(AriaPipe* pipe) {
    if (!pipe || pipe->read_closed) return -1;
    
    int result = close(pipe->read_fd);
    if (result == 0) {
        pipe->read_closed = true;
    }
    return result;
}

int npk_pipe_get_read_fd(AriaPipe* pipe) {
    if (!pipe) return -1;
    return pipe->read_fd;
}

int npk_pipe_get_write_fd(AriaPipe* pipe) {
    if (!pipe) return -1;
    return pipe->write_fd;
}

static void npk_pipe_free(AriaPipe* pipe) {
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
AriaResult* npk_pipe_create(void) {
    return npk_result_err("Pipes not yet implemented on Windows");
}

int64_t npk_pipe_write(AriaPipe* pipe, const void* data, size_t size) {
    return -1;
}

int64_t npk_pipe_read(AriaPipe* pipe, void* buffer, size_t size) {
    return -1;
}

int npk_pipe_close_write(AriaPipe* pipe) {
    return -1;
}

int npk_pipe_close_read(AriaPipe* pipe) {
    return -1;
}

int npk_pipe_get_read_fd(AriaPipe* pipe) {
    return -1;
}

int npk_pipe_get_write_fd(AriaPipe* pipe) {
    return -1;
}

static void npk_pipe_free(AriaPipe* pipe) {
    if (pipe) free(pipe);
}

#endif

// ============================================================================
// Process Information
// ============================================================================

#ifndef _WIN32

int64_t npk_get_current_pid(void) {
    return (int64_t)getpid();
}

int64_t npk_get_parent_pid(void) {
    return (int64_t)getppid();
}

#else

int64_t npk_get_current_pid(void) {
    return (int64_t)GetCurrentProcessId();
}

int64_t npk_get_parent_pid(void) {
    // Windows doesn't have a direct equivalent
    return -1;
}

#endif

static void npk_process_info_free(AriaProcessInfo* info) {
    if (!info) return;
    free(info);
}

static void npk_fork_info_free(AriaForkInfo* info) {
    if (!info) return;
    free(info);
}
