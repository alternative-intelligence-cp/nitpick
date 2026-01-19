/**
 * Hex-Stream Process Implementation
 * 
 * Complete six-stream process orchestration for Linux and Windows.
 */

#include "hexstream/process.hpp"
#include <cstring>
#include <sstream>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#else
#include <windows.h>
#endif

namespace ariash {
namespace hexstream {

// =============================================================================
// HexStreamProcess Implementation
// =============================================================================

HexStreamProcess::HexStreamProcess(const ProcessConfig& config)
    : config_(config)
    , pid_(-1)
    , exitCode_(-1)
    , running_(false)
#ifdef _WIN32
    , processHandle_(INVALID_HANDLE_VALUE)
#else
    , pidfd_(-1)
#endif
{
}

HexStreamProcess::~HexStreamProcess() {
#ifdef _WIN32
    if (processHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(processHandle_);
        processHandle_ = INVALID_HANDLE_VALUE;
    }
#else
    if (pidfd_ >= 0) {
        close(pidfd_);
        pidfd_ = -1;
    }
#endif
}

bool HexStreamProcess::spawn() {
#ifdef _WIN32
    return spawnWindows();
#else
    return spawnLinux();
#endif
}

#ifndef _WIN32
bool HexStreamProcess::spawnLinux() {
    // Create pipes for all six streams
    if (!streamController_.createPipes()) {
        return false;
    }
    
    // Fork
    pid_ = fork();
    if (pid_ < 0) {
        return false;  // Fork failed
    }
    
    if (pid_ == 0) {
        // Child process
        
        // Setup child-side pipes (redirect FD 0-5)
        if (!streamController_.setupChild()) {
            _exit(1);
        }
        
        // Build argv
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(config_.executable.c_str()));
        for (const auto& arg : config_.arguments) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        
        // Build envp (if provided)
        char** envp = nullptr;
        if (!config_.environment.empty()) {
            std::vector<char*> envVec;
            for (const auto& env : config_.environment) {
                envVec.push_back(const_cast<char*>(env.c_str()));
            }
            envVec.push_back(nullptr);
            envp = envVec.data();
        }
        
        // Exec
        if (envp) {
            execve(config_.executable.c_str(), argv.data(), envp);
        } else {
            execv(config_.executable.c_str(), argv.data());
        }
        
        // If exec fails
        _exit(127);
    }
    
    // Parent process
    
    // Setup parent-side pipes (close child ends)
    if (!streamController_.setupParent()) {
        kill(pid_, SIGKILL);
        waitpid(pid_, nullptr, 0);
        return false;
    }
    
    // Set foreground mode
    streamController_.setForegroundMode(config_.foregroundMode);
    
    // Start draining threads
    if (!streamController_.startDraining()) {
        kill(pid_, SIGKILL);
        waitpid(pid_, nullptr, 0);
        return false;
    }
    
#ifdef __linux__
    // Open pidfd for race-free management (Linux 5.3+)
    pidfd_ = syscall(SYS_pidfd_open, pid_, 0);
    if (pidfd_ < 0) {
        // Fallback: pidfd not available, use traditional waitpid
        // This is fine, just slightly less race-free
    }
#endif
    
    running_ = true;
    return true;
}
#endif // !_WIN32

#ifdef _WIN32
bool HexStreamProcess::spawnWindows() {
    // Create Windows bootstrap
    windowsBootstrap_ = std::make_unique<platform::WindowsBootstrap>();
    
    // Create pipes
    if (!windowsBootstrap_->createPipes()) {
        return false;
    }
    
    // Build command line
    std::wostringstream cmdLine;
    cmdLine << std::wstring(config_.executable.begin(), config_.executable.end());
    for (const auto& arg : config_.arguments) {
        cmdLine << L" \"" << std::wstring(arg.begin(), arg.end()) << L"\"";
    }
    
    // Launch process with handle mapping
    processHandle_ = windowsBootstrap_->launchProcess(cmdLine.str(), config_.useEnvBootstrap);
    if (processHandle_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Get PID (for compatibility)
    pid_ = GetProcessId(processHandle_);
    
    // TODO: Setup stream controller with Windows handles
    // This would require extending StreamController to work with Windows HANDLEs
    
    running_ = true;
    return true;
}
#endif // _WIN32

int HexStreamProcess::wait() {
    if (!running_) {
        return exitCode_;
    }
    
#ifdef _WIN32
    if (processHandle_ != INVALID_HANDLE_VALUE) {
        WaitForSingleObject(processHandle_, INFINITE);
        
        DWORD exitCodeWin;
        if (GetExitCodeProcess(processHandle_, &exitCodeWin)) {
            exitCode_ = static_cast<int>(exitCodeWin);
        }
    }
#else
    int status;
    
#ifdef __linux__
    if (pidfd_ >= 0) {
        // Use pidfd_wait (race-free)
        // Wait for process to become zombie
        struct pollfd pfd;
        pfd.fd = pidfd_;
        pfd.events = POLLIN;
        
        poll(&pfd, 1, -1);  // Wait indefinitely
        
        // Process is ready to reap
        waitpid(pid_, &status, WNOHANG);
    } else
#endif
    {
        // Traditional waitpid
        waitpid(pid_, &status, 0);
    }
    
    if (WIFEXITED(status)) {
        exitCode_ = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        exitCode_ = 128 + WTERMSIG(status);
    } else {
        exitCode_ = -1;
    }
#endif
    
    running_ = false;
    
    // Call exit callback
    if (exitCallback_) {
        exitCallback_(exitCode_);
    }
    
    return exitCode_;
}

bool HexStreamProcess::isRunning() const {
    return running_;
}

bool HexStreamProcess::sendSignal(int signal) {
#ifdef _WIN32
    // Windows signals are limited
    if (signal == SIGKILL && processHandle_ != INVALID_HANDLE_VALUE) {
        return TerminateProcess(processHandle_, 1) != 0;
    }
    return false;
#else
    if (pid_ > 0) {
        return kill(pid_, signal) == 0;
    }
    return false;
#endif
}

ssize_t HexStreamProcess::writeToStdin(const void* data, size_t size) {
    return streamController_.writeStdin(data, size);
}

ssize_t HexStreamProcess::writeToStdin(const std::string& str) {
    return writeToStdin(str.c_str(), str.size());
}

ssize_t HexStreamProcess::writeToStdDatI(const void* data, size_t size) {
    // TODO: Implement stddati write
    // Would use similar pattern to writeStdin but for FD 4 / stream 4
    (void)data;
    (void)size;
    return -1;
}

size_t HexStreamProcess::readFromStdout(void* buffer, size_t maxSize) {
    return streamController_.readBuffer(job::StreamIndex::STDOUT, buffer, maxSize);
}

size_t HexStreamProcess::readFromStderr(void* buffer, size_t maxSize) {
    return streamController_.readBuffer(job::StreamIndex::STDERR, buffer, maxSize);
}

size_t HexStreamProcess::readFromStdDbg(void* buffer, size_t maxSize) {
    return streamController_.readBuffer(job::StreamIndex::STDDBG, buffer, maxSize);
}

size_t HexStreamProcess::readFromStdDatO(void* buffer, size_t maxSize) {
    return streamController_.readBuffer(job::StreamIndex::STDDATO, buffer, maxSize);
}

size_t HexStreamProcess::availableData(job::StreamIndex stream) const {
    return streamController_.availableData(stream);
}

void HexStreamProcess::onData(DataCallback callback) {
    streamController_.onData(callback);
}

void HexStreamProcess::flushBuffers() {
    streamController_.flushBuffers();
}

void HexStreamProcess::onExit(ExitCallback callback) {
    exitCallback_ = callback;
}

size_t HexStreamProcess::getTotalBytesTransferred() const {
    return streamController_.getTotalBytesTransferred();
}

size_t HexStreamProcess::getActiveThreadCount() const {
    return streamController_.getActiveThreadCount();
}

// =============================================================================
// HexStreamPipeline Implementation
// =============================================================================

size_t HexStreamPipeline::addProcess(const ProcessConfig& config) {
    size_t idx = processes_.size();
    processes_.push_back(std::make_unique<HexStreamProcess>(config));
    return idx;
}

void HexStreamPipeline::connect(size_t srcIdx, size_t dstIdx, job::StreamIndex stream) {
    connections_.push_back({srcIdx, dstIdx, stream});
}

bool HexStreamPipeline::spawn() {
    // TODO: Implement pipeline connections
    // This would involve:
    // 1. Creating shared pipes between processes
    // 2. On Linux: Using splice() for zero-copy
    // 3. On Windows: Direct handle passing
    
    // For now, spawn all processes independently
    for (auto& proc : processes_) {
        if (!proc->spawn()) {
            return false;
        }
    }
    
    return true;
}

std::vector<int> HexStreamPipeline::waitAll() {
    std::vector<int> exitCodes;
    
    for (auto& proc : processes_) {
        exitCodes.push_back(proc->wait());
    }
    
    return exitCodes;
}

} // namespace hexstream
} // namespace ariash
