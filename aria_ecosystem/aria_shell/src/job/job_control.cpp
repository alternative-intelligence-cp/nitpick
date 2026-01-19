/**
 * AriaSH Job Control Implementation
 *
 * ARIA-021: Shell Job Control State Machine Design
 *
 * Platform-specific implementations for job management.
 */

#include "job/job_control.hpp"
#include "job/stream_controller.hpp"
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <fcntl.h>
#ifdef __linux__
#include <sys/syscall.h>
#include <sys/epoll.h>
#endif
#endif

namespace ariash {
namespace job {

// =============================================================================
// ProcessHandle Implementation
// =============================================================================

bool ProcessHandle::isValid() const {
#ifdef _WIN32
    return handle != INVALID_HANDLE_VALUE;
#else
    return pidfd >= 0 || pid > 0;
#endif
}

void ProcessHandle::close() {
#ifdef _WIN32
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
#else
    if (pidfd >= 0) {
        ::close(pidfd);
        pidfd = -1;
    }
#endif
}

// =============================================================================
// JobControlBlock Implementation
// =============================================================================

JobControlBlock::~JobControlBlock() {
    // Close all process handles
    for (auto& proc : processes) {
        proc.close();
    }

#ifdef _WIN32
    if (jobObject != INVALID_HANDLE_VALUE) {
        CloseHandle(jobObject);
    }
#endif
}

// =============================================================================
// JobManager Private Implementation
// =============================================================================

struct JobManager::Impl {
#ifdef __linux__
    int epollFd = -1;
#endif
};

// =============================================================================
// JobManager Implementation
// =============================================================================

JobManager::JobManager()
    : impl(std::make_unique<Impl>()) {}

JobManager::~JobManager() {
    shutdown();
}

bool JobManager::initialize() {
#ifndef _WIN32
    // Get TTY file descriptor
    ttyFd = open("/dev/tty", O_RDWR);
    if (ttyFd < 0) {
        // Fallback to stdin
        ttyFd = STDIN_FILENO;
    }

    // Save shell's process group
    shellPgid = getpgrp();

    // Try to save current terminal modes - determines if we have a TTY
    if (tcgetattr(ttyFd, &shellModes) == 0) {
        hasTty = true;

        // Make shell the foreground process group (only if we have TTY)
        tcsetpgrp(ttyFd, shellPgid);
    } else {
        // No TTY available - shell runs in non-interactive mode
        // Job control features will be limited but shell is still functional
        hasTty = false;
    }

    // Ignore job control signals in shell
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

#ifdef __linux__
    // Create epoll instance for async event handling
    impl->epollFd = epoll_create1(EPOLL_CLOEXEC);
    // epoll failure is also non-fatal - we can fall back to polling
#endif
#endif

    return true;
}

void JobManager::shutdown() {
    // Terminate all jobs
    std::lock_guard<std::mutex> lock(jobsMutex);

    for (auto& pair : jobs) {
        if (pair.second->state != JobState::TERMINATED) {
            terminate(pair.first, true);  // Force kill
        }
    }

    jobs.clear();

#ifndef _WIN32
    // Restore terminal modes (only if we have a TTY)
    if (hasTty && inRawMode) {
        exitRawMode();
    }

#ifdef __linux__
    if (impl->epollFd >= 0) {
        ::close(impl->epollFd);
        impl->epollFd = -1;
    }
#endif
#endif
}

uint32_t JobManager::spawn(const SpawnOptions& options) {
    std::lock_guard<std::mutex> lock(jobsMutex);

    uint32_t jobId = nextJobId++;

    auto jcb = std::make_unique<JobControlBlock>();
    jcb->jobId = jobId;
    jcb->command = options.command;
    jcb->state = options.background ? JobState::BACKGROUND : JobState::FOREGROUND;
    jcb->startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // Create stream controller
    jcb->streams = std::make_unique<StreamController>();
    if (!jcb->streams->createPipes()) {
        return 0;
    }

#ifndef _WIN32
    pid_t pid = fork();
    if (pid < 0) {
        return 0;  // Fork failed
    }

    if (pid == 0) {
        // Child process

        // Create new process group
        if (options.createPipeGroup) {
            setpgid(0, 0);
        }

        // If foreground, take control of TTY
        if (!options.background) {
            tcsetpgrp(ttyFd, getpid());
        }

        // Restore default signal handlers
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        // Setup pipes for child
        jcb->streams->setupChild();

        // Build argv
        std::vector<const char*> argv;
        argv.push_back(options.command.c_str());
        for (const auto& arg : options.args) {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        // Execute
        execvp(options.command.c_str(), const_cast<char**>(argv.data()));

        // If exec fails
        _exit(127);
    }

    // Parent process
    ProcessHandle ph;
    ph.pid = pid;

#ifdef __linux__
    // Get pidfd for race-free process management
    ph.pidfd = syscall(SYS_pidfd_open, pid, 0);

    // Register with epoll
    if (ph.pidfd >= 0 && impl->epollFd >= 0) {
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.u32 = jobId;
        epoll_ctl(impl->epollFd, EPOLL_CTL_ADD, ph.pidfd, &ev);
    }
#endif

    jcb->processes.push_back(ph);
    jcb->pgid = pid;

    // Setup parent side of pipes
    jcb->streams->setupParent();
    jcb->streams->startDraining();

    // If foreground and we have a TTY, save terminal modes
    if (!options.background && hasTty) {
        tcgetattr(ttyFd, &jcb->savedModes);
        jcb->hasSavedModes = true;
        tcsetpgrp(ttyFd, pid);
    }

    jcb->streams->setForegroundMode(!options.background);

#endif  // !_WIN32

    jobs[jobId] = std::move(jcb);
    return jobId;
}

JobControlBlock* JobManager::getJob(uint32_t jobId) {
    std::lock_guard<std::mutex> lock(jobsMutex);
    auto it = jobs.find(jobId);
    return (it != jobs.end()) ? it->second.get() : nullptr;
}

std::vector<uint32_t> JobManager::getActiveJobs() const {
    std::lock_guard<std::mutex> lock(jobsMutex);
    std::vector<uint32_t> result;
    for (const auto& pair : jobs) {
        if (pair.second->state != JobState::TERMINATED) {
            result.push_back(pair.first);
        }
    }
    return result;
}

JobControlBlock* JobManager::getForegroundJob() {
    std::lock_guard<std::mutex> lock(jobsMutex);
    for (auto& pair : jobs) {
        if (pair.second->state == JobState::FOREGROUND) {
            return pair.second.get();
        }
    }
    return nullptr;
}

bool JobManager::foreground(uint32_t jobId) {
    auto* job = getJob(jobId);
    if (!job) return false;

    JobState current = job->state.load();
    auto result = StateMachine::transition(current, JobEvent::FG_CMD);
    if (!result.valid) return false;

#ifndef _WIN32
    // Resume if stopped
    if (current == JobState::STOPPED) {
        kill(-job->pgid, SIGCONT);
    }

    // Terminal control operations only if we have a TTY
    if (hasTty) {
        // Give terminal control to job
        tcsetpgrp(ttyFd, job->pgid);

        // Restore job's terminal modes
        if (job->hasSavedModes) {
            tcsetattr(ttyFd, TCSADRAIN, &job->savedModes);
        }
    }
#endif

    job->streams->setForegroundMode(true);

    JobState oldState = job->state.exchange(result.newState);
    notifyStatusChange(jobId, oldState, result.newState);

    return true;
}

bool JobManager::background(uint32_t jobId, bool resume) {
    auto* job = getJob(jobId);
    if (!job) return false;

    JobState current = job->state.load();
    auto result = StateMachine::transition(current, JobEvent::BG_CMD);
    if (!result.valid) return false;

#ifndef _WIN32
    // Resume if stopped
    if (resume && current == JobState::STOPPED) {
        kill(-job->pgid, SIGCONT);
    }

    // Shell takes terminal control (only if we have a TTY)
    if (hasTty) {
        tcsetpgrp(ttyFd, shellPgid);
    }
#endif

    job->streams->setForegroundMode(false);

    JobState oldState = job->state.exchange(result.newState);
    notifyStatusChange(jobId, oldState, result.newState);

    return true;
}

bool JobManager::stop(uint32_t jobId) {
    auto* job = getJob(jobId);
    if (!job) return false;

#ifndef _WIN32
    return sendSignal(job, SIGTSTP);
#else
    return false;  // TODO: Windows implementation
#endif
}

bool JobManager::terminate(uint32_t jobId, bool force) {
    auto* job = getJob(jobId);
    if (!job) return false;

#ifndef _WIN32
    int sig = force ? SIGKILL : SIGTERM;
    return sendSignal(job, sig);
#else
    if (job->jobObject != INVALID_HANDLE_VALUE) {
        TerminateJobObject(job->jobObject, 1);
        return true;
    }
    return false;
#endif
}

int JobManager::wait(uint32_t jobId, uint32_t timeout_ms) {
    auto* job = getJob(jobId);
    if (!job) return -1;

    auto start = std::chrono::steady_clock::now();

    while (job->state != JobState::TERMINATED) {
        processEvents(100);

        if (timeout_ms > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();
            if (elapsed >= timeout_ms) {
                return -1;  // Timeout
            }
        }
    }

    return job->exitCode;
}

void JobManager::handleCtrlC() {
    auto* job = getForegroundJob();
    if (job) {
#ifndef _WIN32
        kill(-job->pgid, SIGINT);
#endif
    }
}

void JobManager::handleCtrlZ() {
    auto* job = getForegroundJob();
    if (job) {
#ifndef _WIN32
        kill(-job->pgid, SIGTSTP);

        // Terminal control operations only if we have a TTY
        if (hasTty) {
            // Shell reclaims terminal
            tcsetpgrp(ttyFd, shellPgid);

            // Save job's terminal modes
            tcgetattr(ttyFd, &job->savedModes);
            job->hasSavedModes = true;

            // Restore shell's terminal modes
            tcsetattr(ttyFd, TCSADRAIN, &shellModes);
        }
#endif

        JobState oldState = job->state.exchange(JobState::STOPPED);
        notifyStatusChange(job->jobId, oldState, JobState::STOPPED);
    }
}

void JobManager::handleCtrlD() {
    // EOF on stdin - typically handled by the shell
}

int JobManager::processEvents(uint32_t timeout_ms) {
    int count = 0;

#ifdef __linux__
    if (impl->epollFd >= 0) {
        struct epoll_event events[16];
        int n = epoll_wait(impl->epollFd, events, 16, timeout_ms);

        for (int i = 0; i < n; ++i) {
            uint32_t jobId = events[i].data.u32;
            auto* job = getJob(jobId);
            if (job) {
                reapJob(job);
                ++count;
            }
        }
    }
#else
    // Fallback: poll all jobs with waitpid
    std::lock_guard<std::mutex> lock(jobsMutex);

    for (auto& pair : jobs) {
        auto* job = pair.second.get();
        if (job->state == JobState::TERMINATED) continue;

#ifndef _WIN32
        for (auto& proc : job->processes) {
            if (proc.pid > 0) {
                int status;
                pid_t result = waitpid(proc.pid, &status, WNOHANG | WUNTRACED);

                if (result > 0) {
                    if (WIFEXITED(status)) {
                        job->exitCode = WEXITSTATUS(status);
                        job->exitedNormally = true;

                        JobState oldState = job->state.exchange(JobState::TERMINATED);
                        notifyStatusChange(job->jobId, oldState, JobState::TERMINATED);
                        ++count;
                    } else if (WIFSTOPPED(status)) {
                        job->stopSignal = WSTOPSIG(status);
                        job->stoppedBySignal = true;

                        JobState oldState = job->state.exchange(JobState::STOPPED);
                        notifyStatusChange(job->jobId, oldState, JobState::STOPPED);
                        ++count;
                    }
                }
            }
        }
#endif
    }
#endif

    return count;
}

void JobManager::onStatusChange(JobStatusCallback callback) {
    statusCallbacks.push_back(callback);
}

bool JobManager::saveTerminalModes() {
#ifndef _WIN32
    if (!hasTty) return true;  // No-op without TTY
    return tcgetattr(ttyFd, &shellModes) == 0;
#else
    return true;
#endif
}

bool JobManager::restoreTerminalModes() {
#ifndef _WIN32
    if (!hasTty) return true;  // No-op without TTY
    return tcsetattr(ttyFd, TCSADRAIN, &shellModes) == 0;
#else
    return true;
#endif
}

bool JobManager::enterRawMode() {
#ifndef _WIN32
    if (!hasTty) return true;  // No-op without TTY

    struct termios raw = shellModes;

    // Disable canonical mode and echo
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);

    // Disable special character processing
    raw.c_iflag &= ~(IXON | ICRNL);

    // Set minimum input and timeout
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(ttyFd, TCSAFLUSH, &raw) < 0) {
        return false;
    }

    inRawMode = true;
#endif
    return true;
}

bool JobManager::exitRawMode() {
#ifndef _WIN32
    if (!hasTty) return true;  // No-op without TTY
    if (tcsetattr(ttyFd, TCSAFLUSH, &shellModes) < 0) {
        return false;
    }
    inRawMode = false;
#endif
    return true;
}

void JobManager::notifyStatusChange(uint32_t jobId, JobState oldState,
                                     JobState newState) {
    for (auto& cb : statusCallbacks) {
        cb(jobId, oldState, newState);
    }
}

bool JobManager::sendSignal(JobControlBlock* job, int signal) {
#ifndef _WIN32
    // Send to process group (negative PGID)
    return kill(-job->pgid, signal) == 0;
#else
    return false;
#endif
}

void JobManager::reapJob(JobControlBlock* job) {
#ifndef _WIN32
    for (auto& proc : job->processes) {
        if (proc.pid > 0) {
            int status;
            if (waitpid(proc.pid, &status, WNOHANG) > 0) {
                if (WIFEXITED(status)) {
                    job->exitCode = WEXITSTATUS(status);
                    job->exitedNormally = true;
                } else if (WIFSIGNALED(status)) {
                    job->exitCode = 128 + WTERMSIG(status);
                    job->exitedNormally = false;
                }
            }
        }
    }

    job->endTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    JobState oldState = job->state.exchange(JobState::TERMINATED);
    notifyStatusChange(job->jobId, oldState, JobState::TERMINATED);
#endif
}

void JobManager::cleanupJob(uint32_t jobId) {
    std::lock_guard<std::mutex> lock(jobsMutex);
    jobs.erase(jobId);
}

// =============================================================================
// Global Job Manager
// =============================================================================

static std::unique_ptr<JobManager> globalJobManager;

JobManager& getJobManager() {
    if (!globalJobManager) {
        globalJobManager = std::make_unique<JobManager>();
    }
    return *globalJobManager;
}

} // namespace job
} // namespace ariash
