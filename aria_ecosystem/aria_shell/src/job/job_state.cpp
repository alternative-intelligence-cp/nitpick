/**
 * AriaSH Job State Machine Implementation
 *
 * ARIA-021: Shell Job Control State Machine Design
 */

#include "job/job_state.hpp"
#include <vector>

namespace ariash {
namespace job {

TransitionResult StateMachine::transition(JobState current, JobEvent event) {
    // State transition table from ARIA-021 spec
    switch (current) {
        case JobState::NONE:
            switch (event) {
                case JobEvent::SPAWN:
                    return TransitionResult::ok(JobState::FOREGROUND);
                case JobEvent::SPAWN_BG:
                    return TransitionResult::ok(JobState::BACKGROUND);
                default:
                    return TransitionResult::fail("Invalid event for NONE state");
            }

        case JobState::FOREGROUND:
            switch (event) {
                case JobEvent::CTRL_Z:
                    // User suspend - transition to STOPPED
                    return TransitionResult::ok(JobState::STOPPED);
                case JobEvent::CTRL_C:
                    // User interrupt - usually terminates
                    return TransitionResult::ok(JobState::TERMINATED);
                case JobEvent::CHILD_EXIT:
                    // Process exited normally
                    return TransitionResult::ok(JobState::TERMINATED);
                case JobEvent::CHILD_STOP:
                    // Process stopped by signal
                    return TransitionResult::ok(JobState::STOPPED);
                case JobEvent::ERROR:
                    return TransitionResult::ok(JobState::TERMINATED);
                default:
                    return TransitionResult::fail("Invalid event for FOREGROUND state");
            }

        case JobState::BACKGROUND:
            switch (event) {
                case JobEvent::FG_CMD:
                    // fg %n - bring to foreground
                    return TransitionResult::ok(JobState::FOREGROUND);
                case JobEvent::BG_CMD:
                    // bg %n - resume if stopped (no-op if running)
                    return TransitionResult::ok(JobState::BACKGROUND);
                case JobEvent::CHILD_EXIT:
                    // Process exited
                    return TransitionResult::ok(JobState::TERMINATED);
                case JobEvent::CHILD_STOP:
                    // Stopped by signal (SIGTTIN/SIGTTOU)
                    return TransitionResult::ok(JobState::STOPPED);
                case JobEvent::TTY_READ:
                    // Background process tried to read TTY
                    // Kernel sends SIGTTIN, process stops
                    return TransitionResult::ok(JobState::STOPPED);
                case JobEvent::ERROR:
                    return TransitionResult::ok(JobState::TERMINATED);
                default:
                    return TransitionResult::fail("Invalid event for BACKGROUND state");
            }

        case JobState::STOPPED:
            switch (event) {
                case JobEvent::FG_CMD:
                    // fg %n - resume in foreground
                    return TransitionResult::ok(JobState::FOREGROUND);
                case JobEvent::BG_CMD:
                    // bg %n - resume in background
                    return TransitionResult::ok(JobState::BACKGROUND);
                case JobEvent::CTRL_C:
                    // Terminate stopped job
                    return TransitionResult::ok(JobState::TERMINATED);
                case JobEvent::CHILD_EXIT:
                    // Stopped process terminated (e.g., SIGKILL)
                    return TransitionResult::ok(JobState::TERMINATED);
                case JobEvent::ERROR:
                    return TransitionResult::ok(JobState::TERMINATED);
                default:
                    return TransitionResult::fail("Invalid event for STOPPED state");
            }

        case JobState::TERMINATED:
            // Terminal state - no transitions out
            return TransitionResult::fail("Job already terminated");
    }

    return TransitionResult::fail("Unknown state");
}

bool StateMachine::canTransition(JobState current, JobEvent event) {
    return transition(current, event).valid;
}

std::vector<JobEvent> StateMachine::validEvents(JobState state) {
    std::vector<JobEvent> events;

    // Check each event
    const JobEvent allEvents[] = {
        JobEvent::SPAWN,
        JobEvent::SPAWN_BG,
        JobEvent::CTRL_C,
        JobEvent::CTRL_Z,
        JobEvent::FG_CMD,
        JobEvent::BG_CMD,
        JobEvent::CHILD_EXIT,
        JobEvent::CHILD_STOP,
        JobEvent::TTY_READ,
        JobEvent::TIMEOUT,
        JobEvent::ERROR
    };

    for (JobEvent event : allEvents) {
        if (canTransition(state, event)) {
            events.push_back(event);
        }
    }

    return events;
}

} // namespace job
} // namespace ariash
