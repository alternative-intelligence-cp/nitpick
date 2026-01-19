/**
 * AriaSH - Aria Interactive Shell
 *
 * ARIA-021: Shell Job Control State Machine Design
 *
 * Simple REPL (Read-Eval-Print Loop) for the Aria shell.
 * Provides command execution with job control support.
 */

#include "job/job_control.hpp"
#include "job/job_state.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <csignal>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#endif

using namespace ariash::job;

// Global job manager reference for signal handlers
static JobManager* g_jobManager = nullptr;

// Signal handler for SIGINT (Ctrl+C)
#ifndef _WIN32
static void sigint_handler(int) {
    if (g_jobManager) {
        g_jobManager->handleCtrlC();
    }
}

// Signal handler for SIGTSTP (Ctrl+Z)
static void sigtstp_handler(int) {
    if (g_jobManager) {
        g_jobManager->handleCtrlZ();
    }
}

// Signal handler for SIGCHLD
static void sigchld_handler(int) {
    if (g_jobManager) {
        g_jobManager->processEvents(0);
    }
}
#endif

// Parse command line into command and arguments
std::pair<std::string, std::vector<std::string>> parseCommand(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    std::vector<std::string> args;

    iss >> cmd;
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }

    return {cmd, args};
}

// Check if command should run in background
bool isBackgroundCommand(std::string& line) {
    if (!line.empty() && line.back() == '&') {
        line.pop_back();
        // Trim trailing whitespace
        while (!line.empty() && std::isspace(line.back())) {
            line.pop_back();
        }
        return true;
    }
    return false;
}

// Built-in commands
bool handleBuiltin(JobManager& jm, const std::string& cmd,
                   const std::vector<std::string>& args) {
    if (cmd == "exit" || cmd == "quit") {
        std::cout << "Goodbye!\n";
        exit(0);
    }

    if (cmd == "jobs") {
        auto jobs = jm.getActiveJobs();
        if (jobs.empty()) {
            std::cout << "No active jobs\n";
        } else {
            for (uint32_t jobId : jobs) {
                auto* job = jm.getJob(jobId);
                if (job) {
                    const char* stateStr = "unknown";
                    switch (job->state.load()) {
                        case JobState::FOREGROUND: stateStr = "Running (fg)"; break;
                        case JobState::BACKGROUND: stateStr = "Running (bg)"; break;
                        case JobState::STOPPED:    stateStr = "Stopped"; break;
                        case JobState::TERMINATED: stateStr = "Done"; break;
                        default: break;
                    }
                    std::cout << "[" << jobId << "] " << stateStr
                              << " " << job->command << "\n";
                }
            }
        }
        return true;
    }

    if (cmd == "fg") {
        if (args.empty()) {
            // Foreground most recent job
            auto jobs = jm.getActiveJobs();
            if (!jobs.empty()) {
                jm.foreground(jobs.back());
                jm.wait(jobs.back(), 0);
            } else {
                std::cerr << "fg: no current job\n";
            }
        } else {
            uint32_t jobId = std::stoul(args[0]);
            if (!jm.foreground(jobId)) {
                std::cerr << "fg: job not found: " << jobId << "\n";
            } else {
                jm.wait(jobId, 0);
            }
        }
        return true;
    }

    if (cmd == "bg") {
        if (args.empty()) {
            auto jobs = jm.getActiveJobs();
            for (uint32_t jobId : jobs) {
                auto* job = jm.getJob(jobId);
                if (job && job->state == JobState::STOPPED) {
                    jm.background(jobId, true);
                    std::cout << "[" << jobId << "] " << job->command << " &\n";
                    break;
                }
            }
        } else {
            uint32_t jobId = std::stoul(args[0]);
            if (!jm.background(jobId, true)) {
                std::cerr << "bg: job not found: " << jobId << "\n";
            }
        }
        return true;
    }

    if (cmd == "cd") {
        const char* dir = args.empty() ? getenv("HOME") : args[0].c_str();
        if (dir && chdir(dir) != 0) {
            std::cerr << "cd: " << strerror(errno) << ": " << dir << "\n";
        }
        return true;
    }

    if (cmd == "help") {
        std::cout << "AriaSH - Aria Interactive Shell\n\n";
        std::cout << "Built-in commands:\n";
        std::cout << "  jobs        List active jobs\n";
        std::cout << "  fg [n]      Bring job n to foreground\n";
        std::cout << "  bg [n]      Resume job n in background\n";
        std::cout << "  cd [dir]    Change directory\n";
        std::cout << "  exit/quit   Exit the shell\n";
        std::cout << "  help        Show this help\n\n";
        std::cout << "Job control:\n";
        std::cout << "  Ctrl+C      Interrupt foreground job\n";
        std::cout << "  Ctrl+Z      Suspend foreground job\n";
        std::cout << "  command &   Run command in background\n";
        return true;
    }

    return false;
}

void printPrompt() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        // Shorten home directory to ~
        const char* home = getenv("HOME");
        if (home && strncmp(cwd, home, strlen(home)) == 0) {
            std::cout << "~" << (cwd + strlen(home));
        } else {
            std::cout << cwd;
        }
    }
    std::cout << " $ " << std::flush;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "AriaSH - Aria Interactive Shell v0.1.0\n";
    std::cout << "Type 'help' for available commands.\n\n";

    // Initialize job manager
    JobManager& jm = getJobManager();
    g_jobManager = &jm;

    if (!jm.initialize()) {
        std::cerr << "Failed to initialize job manager\n";
        return 1;
    }

    // Register status change callback
    jm.onStatusChange([](uint32_t jobId, JobState oldState, JobState newState) {
        if (newState == JobState::TERMINATED && oldState != JobState::TERMINATED) {
            auto* job = g_jobManager->getJob(jobId);
            if (job) {
                std::cout << "\n[" << jobId << "] Done: " << job->command << "\n";
            }
        } else if (newState == JobState::STOPPED && oldState != JobState::STOPPED) {
            auto* job = g_jobManager->getJob(jobId);
            if (job) {
                std::cout << "\n[" << jobId << "] Stopped: " << job->command << "\n";
            }
        }
    });

#ifndef _WIN32
    // Install signal handlers
    struct sigaction sa;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, nullptr);

    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, nullptr);

    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, nullptr);
#endif

    // Main REPL loop
    std::string line;
    while (true) {
        // Process any pending events
        jm.processEvents(0);

        printPrompt();

        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;  // EOF
        }

        // Skip empty lines
        if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) {
            continue;
        }

        // Check for background command
        bool background = isBackgroundCommand(line);

        // Parse command
        auto [cmd, args] = parseCommand(line);

        if (cmd.empty()) {
            continue;
        }

        // Try built-in commands first
        if (handleBuiltin(jm, cmd, args)) {
            continue;
        }

        // External command - spawn via job control
        SpawnOptions opts;
        opts.command = cmd;
        opts.args = args;
        opts.background = background;
        opts.createPipeGroup = true;

        uint32_t jobId = jm.spawn(opts);
        if (jobId == 0) {
            std::cerr << "ariash: command not found: " << cmd << "\n";
            continue;
        }

        if (background) {
            std::cout << "[" << jobId << "] " << cmd << " &\n";
        } else {
            // Wait for foreground job
            jm.wait(jobId, 0);

            // Restore shell terminal control
            jm.restoreTerminalModes();
        }
    }

    jm.shutdown();
    return 0;
}
