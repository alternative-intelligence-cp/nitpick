// Event Loop for async I/O operations
// Linux epoll-based implementation for true non-blocking I/O

#ifndef ARIA_RUNTIME_ASYNC_EVENT_LOOP_H
#define ARIA_RUNTIME_ASYNC_EVENT_LOOP_H

#include <cstdint>
#include <functional>
#include <vector>
#include <queue>
#include <map>
#include <sys/epoll.h>
#include <sys/timerfd.h>

namespace npk {
namespace runtime {

// Forward declarations
class Future;
class Executor;

/**
 * Event types for I/O operations
 */
enum class EventType {
    READ,       // Socket/file ready for reading
    WRITE,      // Socket/file ready for writing
    ACCEPT,     // Socket ready to accept connection
    CONNECT,    // Socket connection completed
    TIMER,      // Timer expired
    SIGNAL      // Signal received
};

/**
 * I/O Event handler
 */
struct IOEvent {
    int fd;                                 // File descriptor
    EventType type;                         // Event type
    std::function<void()> callback;         // Handler function
    void* user_data;                        // User context
    uint64_t timeout_ms;                    // Timeout (0 = no timeout)
    
    IOEvent(int fd, EventType type, std::function<void()> cb, void* data = nullptr)
        : fd(fd), type(type), callback(cb), user_data(data), timeout_ms(0) {}
};

/**
 * Timer event
 */
struct TimerEvent {
    uint64_t id;                            // Unique timer ID
    uint64_t delay_ms;                      // Delay in milliseconds
    bool repeating;                         // One-shot or repeating
    std::function<void()> callback;         // Timer callback
    uint64_t next_fire;                     // Absolute time of next firing
    
    TimerEvent(uint64_t id, uint64_t delay, bool repeat, std::function<void()> cb)
        : id(id), delay_ms(delay), repeating(repeat), callback(cb), next_fire(0) {}
};

/**
 * Event Loop - Core async I/O scheduler
 * 
 * Provides:
 * - Non-blocking I/O with epoll
 * - Timer scheduling
 * - Task queuing
 * - Integration with async executor
 */
class EventLoop {
private:
    int epoll_fd;                           // epoll file descriptor
    int timer_fd;                           // timerfd for timer events
    bool running;                           // Loop state
    uint64_t next_timer_id;                 // Timer ID counter
    
    // Event tracking
    std::map<int, IOEvent*> io_events;      // FD -> IOEvent mapping
    std::map<uint64_t, TimerEvent*> timers; // Timer ID -> TimerEvent
    std::priority_queue<TimerEvent*> timer_queue; // Sorted by next_fire
    
    // Task queue for deferred execution
    std::queue<std::function<void()>> task_queue;
    
    // Integration with executor
    Executor* executor;
    
    // Statistics
    uint64_t events_processed;
    uint64_t timers_fired;
    
public:
    EventLoop();
    ~EventLoop();
    
    /**
     * Start the event loop
     * Runs until stop() is called
     */
    void run();
    
    /**
     * Stop the event loop
     */
    void stop();
    
    /**
     * Process events for specified timeout
     * @param timeout_ms Timeout in milliseconds (-1 = block forever)
     * @return Number of events processed
     */
    int poll(int timeout_ms = -1);
    
    /**
     * Add I/O event handler
     * @param fd File descriptor to monitor
     * @param type Event type (READ/WRITE/etc)
     * @param callback Handler function
     * @param user_data Optional user context
     * @return true on success
     */
    bool add_io_event(int fd, EventType type, std::function<void()> callback, void* user_data = nullptr);
    
    /**
     * Remove I/O event handler
     * @param fd File descriptor
     * @return true if removed
     */
    bool remove_io_event(int fd);
    
    /**
     * Schedule timer
     * @param delay_ms Delay in milliseconds
     * @param callback Timer callback
     * @param repeating One-shot or repeating
     * @return Timer ID (0 on failure)
     */
    uint64_t add_timer(uint64_t delay_ms, std::function<void()> callback, bool repeating = false);
    
    /**
     * Cancel timer
     * @param timer_id Timer ID from add_timer()
     * @return true if cancelled
     */
    bool cancel_timer(uint64_t timer_id);
    
    /**
     * Queue task for execution on next iteration
     * Thread-safe for posting tasks from other threads
     * @param task Task function
     */
    void post_task(std::function<void()> task);
    
    /**
     * Set executor for coroutine integration
     * @param exec Executor instance
     */
    void set_executor(Executor* exec);
    
    /**
     * Get statistics
     */
    uint64_t get_events_processed() const { return events_processed; }
    uint64_t get_timers_fired() const { return timers_fired; }
    bool is_running() const { return running; }
    
private:
    /**
     * Process pending timers
     * @return Number of timers fired
     */
    int process_timers();
    
    /**
     * Process task queue
     * @return Number of tasks executed
     */
    int process_tasks();
    
    /**
     * Update epoll interest list
     */
    bool update_epoll(int fd, uint32_t events, int op);
    
    /**
     * Get current timestamp in milliseconds
     */
    uint64_t get_timestamp_ms() const;
};

} // namespace runtime
} // namespace npk

#endif // ARIA_RUNTIME_ASYNC_EVENT_LOOP_H
