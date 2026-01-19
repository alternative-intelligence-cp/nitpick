/**
 * AriaSH Job State Machine Definitions
 *
 * ARIA-021: Shell Job Control State Machine Design
 *
 * Defines the states and transitions for process lifecycle management.
 * Implements a rigorous Finite State Machine (FSM) for job control.
 */

#ifndef ARIASH_JOB_STATE_HPP
#define ARIASH_JOB_STATE_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace ariash {
namespace job {

/**
 * Job States
 *
 * FOREGROUND (FG): Job owns the terminal, receives input
 * BACKGROUND (BG): Job runs async, output buffered
 * STOPPED (STP):   Job suspended, waiting for resume
 * TERMINATED:      Job has exited, ready for cleanup
 */
enum class JobState : uint8_t {
    NONE = 0,       // Initial/invalid state
    FOREGROUND,     // FG - owns terminal
    BACKGROUND,     // BG - async execution
    STOPPED,        // STP - suspended
    TERMINATED      // TERM - exited
};

/**
 * Get string representation of job state
 */
inline const char* jobStateName(JobState state) {
    switch (state) {
        case JobState::NONE:        return "NONE";
        case JobState::FOREGROUND:  return "FOREGROUND";
        case JobState::BACKGROUND:  return "BACKGROUND";
        case JobState::STOPPED:     return "STOPPED";
        case JobState::TERMINATED:  return "TERMINATED";
    }
    return "UNKNOWN";
}

/**
 * Job state change events
 *
 * These events trigger state transitions in the FSM.
 */
enum class JobEvent : uint8_t {
    // User input events
    SPAWN,          // spawn() - new process
    SPAWN_BG,       // spawn() & - new background process
    CTRL_C,         // Ctrl+C (0x03) - interrupt
    CTRL_Z,         // Ctrl+Z (0x1A) - suspend
    FG_CMD,         // fg %n - foreground command
    BG_CMD,         // bg %n - background command

    // OS/kernel events
    CHILD_EXIT,     // Process exited (pidfd readable)
    CHILD_STOP,     // Process stopped (SIGTSTP/SIGTTIN)
    TTY_READ,       // Background process tried to read TTY

    // Internal events
    TIMEOUT,        // Operation timed out
    ERROR           // Error occurred
};

/**
 * Get string representation of job event
 */
inline const char* jobEventName(JobEvent event) {
    switch (event) {
        case JobEvent::SPAWN:       return "SPAWN";
        case JobEvent::SPAWN_BG:    return "SPAWN_BG";
        case JobEvent::CTRL_C:      return "CTRL_C";
        case JobEvent::CTRL_Z:      return "CTRL_Z";
        case JobEvent::FG_CMD:      return "FG_CMD";
        case JobEvent::BG_CMD:      return "BG_CMD";
        case JobEvent::CHILD_EXIT:  return "CHILD_EXIT";
        case JobEvent::CHILD_STOP:  return "CHILD_STOP";
        case JobEvent::TTY_READ:    return "TTY_READ";
        case JobEvent::TIMEOUT:     return "TIMEOUT";
        case JobEvent::ERROR:       return "ERROR";
    }
    return "UNKNOWN";
}

/**
 * State transition result
 */
struct TransitionResult {
    bool valid;             // Was transition valid?
    JobState newState;      // Target state (if valid)
    std::string error;      // Error message (if invalid)

    static TransitionResult ok(JobState state) {
        return {true, state, ""};
    }

    static TransitionResult fail(const std::string& msg) {
        return {false, JobState::NONE, msg};
    }
};

/**
 * State Machine Transition Table
 *
 * Implements the deterministic transitions from the ARIA-021 spec.
 */
class StateMachine {
public:
    /**
     * Compute next state given current state and event
     *
     * @param current Current job state
     * @param event   Incoming event
     * @return Transition result with new state or error
     */
    static TransitionResult transition(JobState current, JobEvent event);

    /**
     * Check if transition is valid without executing it
     */
    static bool canTransition(JobState current, JobEvent event);

    /**
     * Get all valid events for a given state
     */
    static std::vector<JobEvent> validEvents(JobState state);
};

} // namespace job
} // namespace ariash

#endif // ARIASH_JOB_STATE_HPP
