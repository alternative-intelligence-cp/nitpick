/**
 * Aria Runtime - Process Management Library
 * 
 * Implements process creation, forking, execution, and inter-process communication.
 * Provides cross-platform abstractions for Unix (fork/exec) and Windows (CreateProcess).
 */

#ifndef ARIA_RUNTIME_PROCESS_H
#define ARIA_RUNTIME_PROCESS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "runtime/io.h"  // For AriaResult

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct AriaProcess AriaProcess;
typedef struct AriaPipe AriaPipe;

// ============================================================================
// Process Types and Structures
// ============================================================================

/**
 * Process information returned by spawn
 */
typedef struct {
    int64_t pid;           // Process ID
    AriaProcess* handle;   // Opaque process handle
} AriaProcessInfo;

/**
 * Fork result information
 */
typedef struct {
    bool is_child;         // true if this is the child process
    int64_t pid;           // Child PID (in parent), 0 (in child)
    int64_t parent_pid;    // Parent PID (in child), own PID (in parent)
} AriaForkInfo;

/**
 * Pipe ends for communication
 */
typedef struct {
    int read_fd;           // Read file descriptor
    int write_fd;          // Write file descriptor
} AriaPipeEnds;

/**
 * Spawn options for controlling child process behavior
 *
 * Supports the Aria six-stream I/O topology:
 *   FD 0: stdin   - Standard input
 *   FD 1: stdout  - Standard output
 *   FD 2: stderr  - Standard error
 *   FD 3: stddbg  - Debug output (structured diagnostics)
 *   FD 4: stddati - Data input (binary/structured data)
 *   FD 5: stddato - Data output (binary/structured data)
 */
typedef struct {
    const char** env;      // Environment variables (NULL-terminated array, NULL for inherit)
    const char* cwd;       // Working directory (NULL for inherit)

    // Standard streams (FD 0-2)
    bool redirect_stdin;   // Redirect stdin from pipe
    bool redirect_stdout;  // Redirect stdout to pipe
    bool redirect_stderr;  // Redirect stderr to pipe
    AriaPipe* stdin_pipe;  // Pipe to use for stdin (if redirect_stdin)
    AriaPipe* stdout_pipe; // Pipe to use for stdout (if redirect_stdout)
    AriaPipe* stderr_pipe; // Pipe to use for stderr (if redirect_stderr)

    // Extended streams (FD 3-5) - Aria six-stream topology
    bool redirect_stddbg;  // Redirect debug output to pipe
    bool redirect_stddati; // Redirect data input from pipe
    bool redirect_stddato; // Redirect data output to pipe
    AriaPipe* stddbg_pipe; // Pipe for debug output (FD 3)
    AriaPipe* stddati_pipe;// Pipe for data input (FD 4)
    AriaPipe* stddato_pipe;// Pipe for data output (FD 5)

    // Security options
    bool close_extra_fds;  // Close FDs >= 6 in child (recommended: true)
} AriaSpawnOptions;

// ============================================================================
// Process Spawning
// ============================================================================

/**
 * Spawn a new process
 * 
 * Creates a new process executing the specified command with arguments.
 * Returns a result containing AriaProcessInfo on success, or error on failure.
 * 
 * Example:
 *   const char* args[] = {"--input", "data.txt", NULL};
 *   AriaResult* r = npk_spawn("./worker", args, NULL);
 *   if (r->err == NULL) {
 *       AriaProcessInfo* info = (AriaProcessInfo*)r->val;
 *       int exit_code = npk_process_wait(info->handle);
 *   }
 * 
 * @param command Path to executable
 * @param args NULL-terminated array of arguments (first arg is typically program name)
 * @param options Spawn options (NULL for defaults)
 * @return Result<AriaProcessInfo*> - caller must free with npk_result_free after using handle
 */
AriaResult* npk_spawn(const char* command, const char** args, AriaSpawnOptions* options);

/**
 * Create default spawn options
 * 
 * @return Newly allocated options with sensible defaults (caller must free)
 */
AriaSpawnOptions* npk_spawn_options_create(void);

// npk_spawn_options_free is internal to the runtime (static in process.cpp)

// ============================================================================
// Process Control
// ============================================================================

/**
 * Wait for a process to exit and return its exit code
 * 
 * This is a blocking call that waits for the process to terminate.
 * 
 * @param process The process to wait for
 * @return Exit code of the process, or -1 on error
 */
int npk_process_wait(AriaProcess* process);

/**
 * Check if a process is still running (non-blocking)
 * 
 * @param process The process to check
 * @return true if running, false if exited or error
 */
bool npk_process_is_running(AriaProcess* process);

/**
 * Get the exit code of a process (if it has exited)
 * 
 * @param process The process to check
 * @param exit_code Pointer to store exit code
 * @return true if process has exited and exit_code is valid, false otherwise
 */
bool npk_process_get_exit_code(AriaProcess* process, int* exit_code);

/**
 * Send a signal to a process (Unix) or terminate (Windows)
 * 
 * @param process The process to signal
 * @param signal Signal number (Unix SIGTERM, SIGKILL, etc.) or 0 for terminate (Windows)
 * @return 0 on success, -1 on error
 */
int npk_process_kill(AriaProcess* process, int signal);

/**
 * Get the process ID
 * 
 * @param process The process
 * @return Process ID, or -1 on error
 */
int64_t npk_process_get_pid(AriaProcess* process);

// npk_process_free is internal to the runtime (static in process.cpp)

// ============================================================================
// Fork and Exec (Unix-style)
// ============================================================================

/**
 * Fork the current process
 * 
 * Creates a copy of the current process. Returns a result containing
 * AriaForkInfo which indicates whether this is the parent or child.
 * 
 * Example:
 *   AriaResult* r = npk_fork();
 *   if (r->err == NULL) {
 *       AriaForkInfo* info = (AriaForkInfo*)r->val;
 *       if (info->is_child) {
 *           // Child process code
 *           npk_exec("./child_program", args);
 *       } else {
 *           // Parent process code
 *           printf("Child PID: %lld\n", info->pid);
 *       }
 *   }
 * 
 * @return Result<AriaForkInfo*> - caller must free with npk_result_free
 */
AriaResult* npk_fork(void);

/**
 * Replace current process with a new program
 * 
 * This function does not return on success (current process is replaced).
 * On error, it returns -1 and the current process continues.
 * 
 * @param command Path to executable
 * @param args NULL-terminated array of arguments
 * @return -1 on error (does not return on success)
 */
int npk_exec(const char* command, const char** args);

/**
 * Replace current process with a new program using environment
 * 
 * @param command Path to executable
 * @param args NULL-terminated array of arguments
 * @param env NULL-terminated array of environment variables
 * @return -1 on error (does not return on success)
 */
int npk_execve(const char* command, const char** args, const char** env);

// ============================================================================
// Pipe Communication
// ============================================================================

/**
 * Create a pipe for inter-process communication
 * 
 * Creates a unidirectional pipe with read and write ends.
 * 
 * Example:
 *   AriaResult* r = npk_pipe_create();
 *   if (r->err == NULL) {
 *       AriaPipe* pipe = (AriaPipe*)r->val;
 *       // Use pipe with spawn or manually
 *       npk_pipe_write(pipe, "data", 4);
 *       char buffer[100];
 *       npk_pipe_read(pipe, buffer, 100);
 *       npk_pipe_free(pipe);
 *   }
 * 
 * @return Result<AriaPipe*> - caller must free with npk_pipe_free
 */
AriaResult* npk_pipe_create(void);

/**
 * Write data to a pipe
 * 
 * @param pipe The pipe to write to
 * @param data Data buffer to write
 * @param size Number of bytes to write
 * @return Number of bytes written, or -1 on error
 */
int64_t npk_pipe_write(AriaPipe* pipe, const void* data, size_t size);

/**
 * Read data from a pipe
 * 
 * @param pipe The pipe to read from
 * @param buffer Buffer to read into
 * @param size Maximum number of bytes to read
 * @return Number of bytes read, 0 on EOF, or -1 on error
 */
int64_t npk_pipe_read(AriaPipe* pipe, void* buffer, size_t size);

/**
 * Close the write end of a pipe
 * 
 * This signals EOF to the reader.
 * 
 * @param pipe The pipe
 * @return 0 on success, -1 on error
 */
int npk_pipe_close_write(AriaPipe* pipe);

/**
 * Close the read end of a pipe
 * 
 * @param pipe The pipe
 * @return 0 on success, -1 on error
 */
int npk_pipe_close_read(AriaPipe* pipe);

/**
 * Get the read file descriptor of a pipe
 * 
 * @param pipe The pipe
 * @return Read file descriptor, or -1 on error
 */
int npk_pipe_get_read_fd(AriaPipe* pipe);

/**
 * Get the write file descriptor of a pipe
 * 
 * @param pipe The pipe
 * @return Write file descriptor, or -1 on error
 */
int npk_pipe_get_write_fd(AriaPipe* pipe);

// npk_pipe_free is internal to the runtime (static in process.cpp)

// ============================================================================
// Process Information
// ============================================================================

/**
 * Get the current process ID
 * 
 * @return Current process ID
 */
int64_t npk_get_current_pid(void);

/**
 * Get the parent process ID
 * 
 * @return Parent process ID
 */
int64_t npk_get_parent_pid(void);

// npk_process_info_free is internal to the runtime (static in process.cpp)

// npk_fork_info_free is internal to the runtime (static in process.cpp)

// ============================================================================
// Fork Safety — Atfork Callback Registration
// ============================================================================

/**
 * Callback type for fork safety handlers.
 */
typedef void (*AriaAtforkCallback)(void);

/**
 * Register pre-fork and post-fork callbacks for fork safety.
 *
 * Runtime subsystems (GC, thread pools, telemetry, etc.) should call this
 * during their initialization to register mutex acquire/release callbacks
 * that prevent deadlocks when npk_fork() is used.
 *
 * - pre_fork:     Called in parent BEFORE fork — acquire subsystem locks
 * - parent_post:  Called in parent AFTER fork — release subsystem locks
 * - child_post:   Called in child AFTER fork — release/reinitialize locks
 *
 * @return 0 on success, -1 if registration table is full
 */
int npk_atfork_register(AriaAtforkCallback pre_fork,
                         AriaAtforkCallback parent_post,
                         AriaAtforkCallback child_post);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_PROCESS_H
