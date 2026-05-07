// Event Loop implementation
// epoll-based async I/O for Linux

#include "runtime/async/event_loop.h"
#include "runtime/async/executor.h"
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <chrono>
#include <stdexcept>
#include <algorithm>

namespace npk {
namespace runtime {

EventLoop::EventLoop() 
    : epoll_fd(-1), timer_fd(-1), running(false), next_timer_id(1),
      executor(nullptr), events_processed(0), timers_fired(0) {
    
    // Create epoll instance
    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        throw std::runtime_error("Failed to create epoll instance");
    }
    
    // Create timerfd for timer events
    timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd < 0) {
        close(epoll_fd);
        throw std::runtime_error("Failed to create timerfd");
    }
}

EventLoop::~EventLoop() {
    stop();
    
    // Clean up I/O events
    for (auto& pair : io_events) {
        delete pair.second;
    }
    io_events.clear();
    
    // Clean up timers
    for (auto& pair : timers) {
        delete pair.second;
    }
    timers.clear();
    
    // Close file descriptors
    if (timer_fd >= 0) close(timer_fd);
    if (epoll_fd >= 0) close(epoll_fd);
}

void EventLoop::run() {
    running = true;
    
    while (running) {
        // Poll for events with 100ms timeout
        poll(100);
        
        // Process timers
        process_timers();
        
        // Process task queue
        process_tasks();
    }
}

void EventLoop::stop() {
    running = false;
}

int EventLoop::poll(int timeout_ms) {
    const int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];
    
    // Wait for events
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout_ms);
    
    if (nfds < 0) {
        if (errno == EINTR) {
            return 0;  // Interrupted, try again
        }
        return -1;  // Error
    }
    
    // Process each event
    for (int i = 0; i < nfds; i++) {
        int fd = events[i].data.fd;
        
        auto it = io_events.find(fd);
        if (it != io_events.end()) {
            IOEvent* evt = it->second;
            
            // Call handler
            if (evt->callback) {
                evt->callback();
            }
            
            events_processed++;
        }
    }
    
    return nfds;
}

bool EventLoop::add_io_event(int fd, EventType type, std::function<void()> callback, void* user_data) {
    // Create event
    IOEvent* evt = new IOEvent(fd, type, callback, user_data);
    
    // Determine epoll events
    uint32_t epoll_events = EPOLLET;  // Edge-triggered
    
    switch (type) {
        case EventType::READ:
        case EventType::ACCEPT:
            epoll_events |= EPOLLIN;
            break;
        case EventType::WRITE:
        case EventType::CONNECT:
            epoll_events |= EPOLLOUT;
            break;
        default:
            epoll_events |= EPOLLIN | EPOLLOUT;
            break;
    }
    
    // Add to epoll
    struct epoll_event ev;
    ev.events = epoll_events;
    ev.data.fd = fd;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        delete evt;
        return false;
    }
    
    // Store event
    io_events[fd] = evt;
    
    return true;
}

bool EventLoop::remove_io_event(int fd) {
    auto it = io_events.find(fd);
    if (it == io_events.end()) {
        return false;
    }
    
    // Remove from epoll
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    
    // Delete event
    delete it->second;
    io_events.erase(it);
    
    return true;
}

uint64_t EventLoop::add_timer(uint64_t delay_ms, std::function<void()> callback, bool repeating) {
    uint64_t timer_id = next_timer_id++;
    
    TimerEvent* timer = new TimerEvent(timer_id, delay_ms, repeating, callback);
    timer->next_fire = get_timestamp_ms() + delay_ms;
    
    timers[timer_id] = timer;
    
    return timer_id;
}

bool EventLoop::cancel_timer(uint64_t timer_id) {
    auto it = timers.find(timer_id);
    if (it == timers.end()) {
        return false;
    }
    
    delete it->second;
    timers.erase(it);
    
    return true;
}

void EventLoop::post_task(std::function<void()> task) {
    task_queue.push(task);
}

void EventLoop::set_executor(Executor* exec) {
    executor = exec;
}

int EventLoop::process_timers() {
    uint64_t now = get_timestamp_ms();
    int fired = 0;
    
    std::vector<uint64_t> to_remove;
    
    for (auto& pair : timers) {
        TimerEvent* timer = pair.second;
        
        if (timer->next_fire <= now) {
            // Fire timer
            if (timer->callback) {
                timer->callback();
            }
            
            fired++;
            timers_fired++;
            
            if (timer->repeating) {
                // Reschedule
                timer->next_fire = now + timer->delay_ms;
            } else {
                // Remove one-shot timer
                to_remove.push_back(timer->id);
            }
        }
    }
    
    // Remove one-shot timers
    for (uint64_t id : to_remove) {
        cancel_timer(id);
    }
    
    return fired;
}

int EventLoop::process_tasks() {
    int processed = 0;
    
    // Process all queued tasks
    while (!task_queue.empty()) {
        auto task = task_queue.front();
        task_queue.pop();
        
        if (task) {
            task();
        }
        
        processed++;
    }
    
    return processed;
}

bool EventLoop::update_epoll(int fd, uint32_t events, int op) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    
    return epoll_ctl(epoll_fd, op, fd, &ev) == 0;
}

uint64_t EventLoop::get_timestamp_ms() const {
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch());
    return ms.count();
}

} // namespace runtime
} // namespace npk
