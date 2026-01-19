/**
 * Hex-Stream Process Orchestrator
 * 
 * Integrates stream draining + Windows bootstrap to provide complete
 * six-stream topology management for child processes.
 * 
 * This is the "glue" that brings together:
 * - StreamController (stream draining, ring buffers)
 * - WindowsBootstrap (cross-platform FD mapping)
 * - Job control (process lifecycle)
 * 
 * Result: Simple API for spawning processes with full hex-stream support.
 */

#ifndef ARIASH_HEX_STREAM_PROCESS_HPP
#define ARIASH_HEX_STREAM_PROCESS_HPP

#include "job/stream_controller.hpp"
#include "job/job_control.hpp"

#ifdef _WIN32
#include "platform/windows_bootstrap.hpp"
#endif

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace ariash {
namespace hexstream {

/**
 * Hex-Stream Process Configuration
 * 
 * Specifies how to spawn a process with six-stream topology.
 */
struct ProcessConfig {
    std::string executable;              // Path to executable
    std::vector<std::string> arguments;  // Command-line arguments
    std::vector<std::string> environment;  // Environment variables (key=value)
    
    bool enableStdDbg = true;    // Enable stream 3 (stddbg)
    bool enableStdDatI = true;   // Enable stream 4 (stddati)
    bool enableStdDatO = true;   // Enable stream 5 (stddato)
    
    bool foregroundMode = false;  // Passthrough stdout/stderr to TTY
    
#ifdef _WIN32
    bool useEnvBootstrap = true;  // Use env var vs CLI flag for Windows
#endif
};

/**
 * Stream data callback
 * 
 * Called when data arrives on any stream.
 */
using DataCallback = std::function<void(job::StreamIndex stream,
                                         const void* data, size_t size)>;

/**
 * Process exit callback
 * 
 * Called when process exits.
 */
using ExitCallback = std::function<void(int exitCode)>;

/**
 * Hex-Stream Process
 * 
 * Manages a single child process with full six-stream topology.
 * 
 * Example usage:
 * 
 *   ProcessConfig config;
 *   config.executable = "/usr/bin/ariac";
 *   config.arguments = {"myfile.aria"};
 *   
 *   HexStreamProcess proc(config);
 *   
 *   proc.onData([](auto stream, auto data, auto size) {
 *       // Handle stream data
 *   });
 *   
 *   proc.spawn();
 *   proc.writeToStdin("input data\n");
 *   
 *   int exitCode = proc.wait();
 */
class HexStreamProcess {
public:
    explicit HexStreamProcess(const ProcessConfig& config);
    ~HexStreamProcess();
    
    // Non-copyable
    HexStreamProcess(const HexStreamProcess&) = delete;
    HexStreamProcess& operator=(const HexStreamProcess&) = delete;
    
    /**
     * Spawn the child process
     * 
     * Creates pipes, sets up six-stream topology, and launches process.
     * 
     * @return true if successful
     */
    bool spawn();
    
    /**
     * Wait for process to exit
     * 
     * Blocks until process terminates.
     * 
     * @return Exit code
     */
    int wait();
    
    /**
     * Check if process is running
     */
    bool isRunning() const;
    
    /**
     * Send signal to process
     * 
     * @param signal Signal number (SIGTERM, SIGKILL, etc.)
     * @return true if successful
     */
    bool sendSignal(int signal);
    
    /**
     * Write to stdin (stream 0)
     */
    ssize_t writeToStdin(const void* data, size_t size);
    ssize_t writeToStdin(const std::string& str);
    
    /**
     * Write to stddati (stream 4) - binary data input
     */
    ssize_t writeToStdDatI(const void* data, size_t size);
    
    /**
     * Read from stdout buffer (stream 1)
     */
    size_t readFromStdout(void* buffer, size_t maxSize);
    
    /**
     * Read from stderr buffer (stream 2)
     */
    size_t readFromStderr(void* buffer, size_t maxSize);
    
    /**
     * Read from stddbg buffer (stream 3)
     */
    size_t readFromStdDbg(void* buffer, size_t maxSize);
    
    /**
     * Read from stddato buffer (stream 5) - binary data output
     */
    size_t readFromStdDatO(void* buffer, size_t maxSize);
    
    /**
     * Check how much data is available in a stream buffer
     */
    size_t availableData(job::StreamIndex stream) const;
    
    /**
     * Register callback for stream data
     * 
     * Called asynchronously when data arrives.
     */
    void onData(DataCallback callback);
    
    /**
     * Flush stream buffers and invoke callbacks
     * 
     * Drains all pending data from ring buffers and calls registered callbacks.
     * Useful after process exits to get remaining output.
     */
    void flushBuffers();
    
    /**
     * Register callback for process exit
     */
    void onExit(ExitCallback callback);
    
    /**
     * Get process ID
     */
    int getPid() const { return pid_; }
    
    /**
     * Get performance metrics
     */
    size_t getTotalBytesTransferred() const;
    size_t getActiveThreadCount() const;
    
private:
    ProcessConfig config_;
    job::StreamController streamController_;
    
    int pid_;
    int exitCode_;
    bool running_;
    
#ifdef _WIN32
    std::unique_ptr<platform::WindowsBootstrap> windowsBootstrap_;
    HANDLE processHandle_;
#else
    int pidfd_;  // Linux pidfd for race-free management
#endif
    
    ExitCallback exitCallback_;
    
    // Platform-specific spawn
    bool spawnLinux();
    bool spawnWindows();
};

/**
 * Hex-Stream Pipeline
 * 
 * Manages multiple processes connected via six-stream pipes.
 * 
 * Example:
 *   Pipeline pipeline;
 *   pipeline.addProcess(producer);
 *   pipeline.addProcess(processor);
 *   pipeline.addProcess(consumer);
 *   pipeline.connect(0, 1, StreamIndex::STDDATO);  // producer → processor
 *   pipeline.connect(1, 2, StreamIndex::STDDATO);  // processor → consumer
 *   pipeline.spawn();
 */
class HexStreamPipeline {
public:
    HexStreamPipeline() = default;
    ~HexStreamPipeline() = default;
    
    /**
     * Add process to pipeline
     * 
     * @return Process index
     */
    size_t addProcess(const ProcessConfig& config);
    
    /**
     * Connect two processes via a stream
     * 
     * @param srcIdx Source process index
     * @param dstIdx Destination process index
     * @param stream Which stream to connect (STDDATO → STDDATI typical)
     */
    void connect(size_t srcIdx, size_t dstIdx, job::StreamIndex stream);
    
    /**
     * Spawn all processes in pipeline
     */
    bool spawn();
    
    /**
     * Wait for all processes to complete
     */
    std::vector<int> waitAll();
    
private:
    std::vector<std::unique_ptr<HexStreamProcess>> processes_;
    
    struct Connection {
        size_t srcIdx;
        size_t dstIdx;
        job::StreamIndex stream;
    };
    std::vector<Connection> connections_;
};

} // namespace hexstream
} // namespace ariash

#endif // ARIASH_HEX_STREAM_PROCESS_HPP
