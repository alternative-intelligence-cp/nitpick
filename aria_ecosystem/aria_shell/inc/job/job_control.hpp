/**
 * AriaSH Job Control System
 *
 * ARIA-021: Shell Job Control State Machine Design
 *
 * Manages process lifecycle using handle-based identity (pidfd/Windows handles)
 * and asynchronous event-driven architecture.
 *
 * Features:
 * - JobControlBlock (JCB) for each job
 * - Hex-Stream integration (6 I/O channels)
 * - Raw mode signal mediation
 * - Platform-specific backends (Linux pidfd, Windows Job Objects)
 */

#ifndef ARIASH_JOB_CONTROL_HPP
#define ARIASH_JOB_CONTROL_HPP

#include "job/job_state.hpp"
#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <termios.h>
#endif

namespace ariash {
namespace job {

// Forward declarations
class StreamController;
class JobManager;

/**
 * Process Handle - Platform-independent process identifier
 *
 * Uses pidfd on Linux 5.3+ for race-condition-free process management.
 * Uses HANDLE on Windows.
 */
struct ProcessHandle {
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
    DWORD processId = 0;
#else
    int pidfd = -1;     // File descriptor for process (Linux 5.3+)
    pid_t pid = -1;     // Traditional PID (fallback)
#endif

    bool isValid() const;
    void close();
};

/**
 * Job Control Block (JCB)
 *
 * The central data structure for job management.
 * Contains process handles, state, and stream controller.
 */
struct JobControlBlock {
    // Identity
    uint32_t jobId;                         // Shell-assigned ID (%1, %2, etc.)
    std::string command;                    // Original command string

    // Process Group
#ifdef _WIN32
    HANDLE jobObject = INVALID_HANDLE_VALUE; // Windows Job Object
#else
    pid_t pgid = -1;                        // Process Group ID
#endif
    std::vector<ProcessHandle> processes;   // All processes in pipeline

    // State Machine
    std::atomic<JobState> state{JobState::NONE};
    std::atomic<bool> notified{false};      // Status change notification flag

    // Terminal State
#ifndef _WIN32
    struct termios savedModes;              // Terminal modes for this job
    bool hasSavedModes = false;
#endif

    // Exit Information
    int exitCode = 0;                       // Aggregated exit code
    bool exitedNormally = false;
    bool stoppedBySignal = false;
    int stopSignal = 0;

    // Stream Controller (Hex-Stream)
    std::unique_ptr<StreamController> streams;

    // Timestamps
    uint64_t startTime = 0;
    uint64_t endTime = 0;

    JobControlBlock() = default;
    ~JobControlBlock();

    // Non-copyable
    JobControlBlock(const JobControlBlock&) = delete;
    JobControlBlock& operator=(const JobControlBlock&) = delete;

    // Movable
    JobControlBlock(JobControlBlock&&) = default;
    JobControlBlock& operator=(JobControlBlock&&) = default;
};

/**
 * Job status callback
 */
using JobStatusCallback = std::function<void(uint32_t jobId, JobState oldState,
                                              JobState newState)>;

/**
 * Spawn options for new processes
 */
struct SpawnOptions {
    std::string command;                    // Command to execute
    std::vector<std::string> args;          // Arguments
    std::string workingDir;                 // Working directory
    std::unordered_map<std::string, std::string> env; // Environment variables
    bool background = false;                // Start in background
    bool createPipeGroup = true;            // Create new process group

    // Hex-Stream configuration
    bool captureStdout = true;
    bool captureStderr = true;
    bool captureStddbg = true;              // FD 3 - telemetry
    bool captureStddati = false;            // FD 4 - data input
    bool captureStddato = false;            // FD 5 - data output
};

/**
 * Job Manager
 *
 * Central controller for all jobs. Implements:
 * - Process spawning with Hex-Stream setup
 * - State machine transitions
 * - Signal handling (Raw Mode mediation)
 * - Asynchronous event processing
 */
class JobManager {
public:
    JobManager();
    ~JobManager();

    // Non-copyable, non-movable (singleton-like)
    JobManager(const JobManager&) = delete;
    JobManager& operator=(const JobManager&) = delete;

    /**
     * Initialize job manager
     *
     * Sets up terminal, signal handlers, and event loop.
     */
    bool initialize();

    /**
     * Shutdown job manager
     *
     * Terminates all jobs and cleans up resources.
     */
    void shutdown();

    // =========================================================================
    // Job Lifecycle
    // =========================================================================

    /**
     * Spawn a new job
     *
     * @param options Spawn configuration
     * @return Job ID on success, 0 on failure
     */
    uint32_t spawn(const SpawnOptions& options);

    /**
     * Get job by ID
     */
    JobControlBlock* getJob(uint32_t jobId);

    /**
     * Get all active jobs
     */
    std::vector<uint32_t> getActiveJobs() const;

    /**
     * Get foreground job (if any)
     */
    JobControlBlock* getForegroundJob();

    // =========================================================================
    // Job Control Commands
    // =========================================================================

    /**
     * Bring job to foreground
     *
     * @param jobId Job to foreground
     * @return true if successful
     */
    bool foreground(uint32_t jobId);

    /**
     * Send job to background
     *
     * @param jobId Job to background
     * @param resume Resume if stopped
     * @return true if successful
     */
    bool background(uint32_t jobId, bool resume = true);

    /**
     * Stop (suspend) a job
     *
     * @param jobId Job to stop
     * @return true if successful
     */
    bool stop(uint32_t jobId);

    /**
     * Terminate a job
     *
     * @param jobId Job to terminate
     * @param force Use SIGKILL instead of SIGTERM
     * @return true if successful
     */
    bool terminate(uint32_t jobId, bool force = false);

    /**
     * Wait for a job to complete
     *
     * @param jobId Job to wait for
     * @param timeout_ms Timeout in milliseconds (0 = infinite)
     * @return Exit code, or -1 on error/timeout
     */
    int wait(uint32_t jobId, uint32_t timeout_ms = 0);

    // =========================================================================
    // Signal Handling (Raw Mode)
    // =========================================================================

    /**
     * Handle Ctrl+C input
     *
     * Sends SIGINT to foreground job if present.
     */
    void handleCtrlC();

    /**
     * Handle Ctrl+Z input
     *
     * Sends SIGTSTP to foreground job if present.
     */
    void handleCtrlZ();

    /**
     * Handle Ctrl+D input (EOF)
     */
    void handleCtrlD();

    // =========================================================================
    // Event Processing
    // =========================================================================

    /**
     * Process pending events
     *
     * Call from main event loop. Handles:
     * - Child exits (pidfd readable)
     * - State changes
     * - Stream data
     *
     * @param timeout_ms Max wait time
     * @return Number of events processed
     */
    int processEvents(uint32_t timeout_ms = 100);

    /**
     * Register callback for job status changes
     */
    void onStatusChange(JobStatusCallback callback);

    // =========================================================================
    // Terminal Control
    // =========================================================================

    /**
     * Save shell's terminal modes
     */
    bool saveTerminalModes();

    /**
     * Restore shell's terminal modes
     */
    bool restoreTerminalModes();

    /**
     * Enter raw mode (disable ISIG, etc.)
     */
    bool enterRawMode();

    /**
     * Exit raw mode
     */
    bool exitRawMode();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    // Job storage
    mutable std::mutex jobsMutex;
    std::unordered_map<uint32_t, std::unique_ptr<JobControlBlock>> jobs;
    uint32_t nextJobId = 1;

    // Status callbacks
    std::vector<JobStatusCallback> statusCallbacks;

    // Internal methods
    void notifyStatusChange(uint32_t jobId, JobState oldState, JobState newState);
    bool sendSignal(JobControlBlock* job, int signal);
    void reapJob(JobControlBlock* job);
    void cleanupJob(uint32_t jobId);

#ifndef _WIN32
    // Linux-specific
    int ttyFd = -1;
    pid_t shellPgid = -1;
    struct termios shellModes;
    bool inRawMode = false;
    bool hasTty = false;  // True if we have a controlling terminal
#endif
};

/**
 * Global job manager instance
 */
JobManager& getJobManager();

} // namespace job
} // namespace ariash

#endif // ARIASH_JOB_CONTROL_HPP
