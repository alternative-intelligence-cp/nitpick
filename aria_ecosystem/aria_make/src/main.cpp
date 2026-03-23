/**
 * main.cpp
 * aria_make - Aria Build System CLI
 *
 * Usage:
 *   aria_make [command] [options]
 *
 * Commands:
 *   build       Build the project (default)
 *   clean       Remove build artifacts
 *   rebuild     Clean and rebuild
 *   check       Show what would be built (dry run)
 *   targets     List all targets
 *   deps        Show dependency graph
 *   test        Build and run test files (test_*.aria, *_test.aria)
 *
 * Options:
 *   -C <dir>    Change to directory before building
 *   -f <file>   Use specified build file (default: build.abc)
 *   -j <N>      Use N parallel jobs (default: auto)
 *   -v          Verbose output
 *   -q          Quiet mode
 *   --force     Force rebuild all targets
 *   --dry-run   Print commands without executing
 *   --help      Show this help
 *   --version   Show version
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "core/build_orchestrator.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <sys/wait.h>

using namespace aria::make;

// -----------------------------------------------------------------------------
// Version and Help
// -----------------------------------------------------------------------------

void print_version() {
    std::cout << "aria_make 0.1.0\n";
    std::cout << "Aria Build System\n";
    std::cout << "Copyright (c) 2025 Aria Language Project\n";
}

void print_help() {
    std::cout << R"(
aria_make - Aria Build System

USAGE:
    aria_make [COMMAND] [OPTIONS] [TARGETS...]

COMMANDS:
    build       Build the project (default if no command given)
    clean       Remove build artifacts and state
    rebuild     Clean and rebuild from scratch
    check       Show what would be built (dry run)
    targets     List all available targets
    deps        Show dependency graph in DOT format
    test        Build and run test files (test_*.aria, *_test.aria)

OPTIONS:
    -C <dir>        Change to directory before building
    -f <file>       Use specified build file (default: build.abc)
    -j, --jobs <N>  Use N parallel jobs (default: number of CPU cores)
    -v, --verbose   Verbose output (show all commands)
    -q, --quiet     Quiet mode (minimal output)
    --force         Force rebuild all targets (ignore state)
    --dry-run       Print commands without executing
    --fail-fast     Stop on first error (default)
    --keep-going    Continue building as much as possible after errors

    -h, --help      Show this help message
    --version       Show version information

EXAMPLES:
    aria_make                       Build all targets
    aria_make build                 Same as above
    aria_make -j4                   Build with 4 parallel jobs
    aria_make -C /path/to/project   Build project in another directory
    aria_make --force               Rebuild everything
    aria_make clean                 Remove build artifacts
    aria_make targets               List all build targets
    aria_make deps > graph.dot      Export dependency graph

BUILD FILE FORMAT (build.abc):
    [project]
    name = "my_project"
    version = "0.1.0"

    [target.main]
    type = "binary"
    sources = ["src/*.aria"]
    deps = []
    flags = ["-O2"]

For more information, see: https://aria-lang.org/docs/build-system

)";
}

// -----------------------------------------------------------------------------
// Progress Reporter
// -----------------------------------------------------------------------------

class ConsoleProgress {
public:
    explicit ConsoleProgress(bool verbose = false, bool quiet = false)
        : verbose_(verbose), quiet_(quiet) {}

    void operator()(const BuildProgress& progress) {
        if (quiet_) return;

        switch (progress.phase) {
            case BuildPhase::PARSING:
                if (verbose_) {
                    std::cout << "[1/6] Parsing build configuration...\n";
                }
                break;

            case BuildPhase::LOADING_STATE:
                if (verbose_) {
                    std::cout << "[2/6] Loading build state...\n";
                }
                break;

            case BuildPhase::ANALYZING:
                if (verbose_) {
                    std::cout << "[3/6] Analyzing dependencies...\n";
                }
                break;

            case BuildPhase::CHECKING_DIRTY:
                if (verbose_) {
                    std::cout << "[4/6] Checking for changes...\n";
                }
                break;

            case BuildPhase::COMPILING:
                if (!progress.current_target.empty()) {
                    std::cout << "[" << (progress.current + 1) << "/"
                              << progress.total << "] Building "
                              << progress.current_target << "...\n";
                }
                break;

            case BuildPhase::SAVING_STATE:
                if (verbose_) {
                    std::cout << "[5/6] Saving build state...\n";
                }
                break;

            case BuildPhase::COMPLETE:
                // Reported separately
                break;
        }
    }

private:
    bool verbose_;
    bool quiet_;
};

// -----------------------------------------------------------------------------
// Argument Parsing
// -----------------------------------------------------------------------------

enum class Command {
    BUILD,
    CLEAN,
    REBUILD,
    CHECK,
    TARGETS,
    DEPS,
    TEST
};

struct Options {
    Command command = Command::BUILD;
    BuildConfig config;
    std::vector<std::string> targets;
    bool show_help = false;
    bool show_version = false;
};

bool parse_args(int argc, char* argv[], Options& opts) {
    opts.config.project_root = fs::current_path();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // Help and version
        if (arg == "-h" || arg == "--help") {
            opts.show_help = true;
            return true;
        }
        if (arg == "--version") {
            opts.show_version = true;
            return true;
        }

        // Commands
        if (arg == "build") {
            opts.command = Command::BUILD;
            continue;
        }
        if (arg == "clean") {
            opts.command = Command::CLEAN;
            continue;
        }
        if (arg == "rebuild") {
            opts.command = Command::REBUILD;
            continue;
        }
        if (arg == "check") {
            opts.command = Command::CHECK;
            continue;
        }
        if (arg == "targets") {
            opts.command = Command::TARGETS;
            continue;
        }
        if (arg == "deps") {
            opts.command = Command::DEPS;
            continue;
        }
        if (arg == "test") {
            opts.command = Command::TEST;
            continue;
        }

        // Options with arguments
        if (arg == "-C" && i + 1 < argc) {
            opts.config.project_root = argv[++i];
            continue;
        }
        if (arg == "-f" && i + 1 < argc) {
            opts.config.build_file = argv[++i];
            continue;
        }
        if ((arg == "-j" || arg == "--jobs") && i + 1 < argc) {
            opts.config.num_threads = std::stoul(argv[++i]);
            continue;
        }

        // Boolean options
        if (arg == "-v" || arg == "--verbose") {
            opts.config.verbose = true;
            continue;
        }
        if (arg == "-q" || arg == "--quiet") {
            opts.config.quiet = true;
            continue;
        }
        if (arg == "--force") {
            opts.config.force_rebuild = true;
            continue;
        }
        if (arg == "--dry-run") {
            opts.config.dry_run = true;
            continue;
        }
        if (arg == "--fail-fast") {
            opts.config.fail_fast = true;
            opts.config.continue_on_error = false;
            continue;
        }
        if (arg == "--keep-going") {
            opts.config.fail_fast = false;
            opts.config.continue_on_error = true;
            continue;
        }

        // Unknown option
        if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Try 'aria_make --help' for more information.\n";
            return false;
        }

        // Target name
        opts.targets.push_back(arg);
    }

    opts.config.targets = opts.targets;
    return true;
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    Options opts;

    if (!parse_args(argc, argv, opts)) {
        return 1;
    }

    if (opts.show_help) {
        print_help();
        return 0;
    }

    if (opts.show_version) {
        print_version();
        return 0;
    }

    // Create orchestrator
    BuildOrchestrator orchestrator(opts.config);
    orchestrator.set_progress_callback(ConsoleProgress(opts.config.verbose, opts.config.quiet));

    // Execute command
    switch (opts.command) {
        case Command::BUILD: {
            BuildResult result = orchestrator.build();

            if (!opts.config.quiet) {
                std::cout << "\n";
                if (result.success) {
                    std::cout << "Build succeeded: "
                              << result.built_targets << " built, "
                              << result.skipped_targets << " up-to-date";
                    if (result.failed_targets > 0) {
                        std::cout << ", " << result.failed_targets << " failed";
                    }
                    std::cout << " (" << result.total_time.count() << "ms)\n";
                } else {
                    std::cout << "Build failed: "
                              << result.failed_targets << " targets failed\n";
                    for (const auto& err : result.errors) {
                        std::cerr << "  Error: " << err << "\n";
                    }
                }
            }

            return result.success ? 0 : 1;
        }

        case Command::CLEAN: {
            bool success = orchestrator.clean();
            if (!opts.config.quiet) {
                if (success) {
                    std::cout << "Clean complete.\n";
                } else {
                    std::cerr << "Clean failed.\n";
                }
            }
            return success ? 0 : 1;
        }

        case Command::REBUILD: {
            BuildResult result = orchestrator.rebuild();

            if (!opts.config.quiet) {
                if (result.success) {
                    std::cout << "\nRebuild succeeded: "
                              << result.built_targets << " targets built"
                              << " (" << result.total_time.count() << "ms)\n";
                } else {
                    std::cout << "\nRebuild failed.\n";
                    for (const auto& err : result.errors) {
                        std::cerr << "  Error: " << err << "\n";
                    }
                }
            }

            return result.success ? 0 : 1;
        }

        case Command::CHECK: {
            BuildResult result = orchestrator.check();

            std::cout << "Would build " << result.built_targets << " targets:\n";
            for (const auto& target : orchestrator.list_targets()) {
                std::cout << "  " << target.name << " (" << target.type << ")\n";
            }

            return 0;
        }

        case Command::TARGETS: {
            // Parse to get targets
            BuildResult dummy = orchestrator.check();

            std::cout << "Available targets:\n";
            for (const auto& target : orchestrator.list_targets()) {
                std::cout << "  " << target.name;
                std::cout << " [" << target.type << "]";
                if (!target.sources.empty()) {
                    std::cout << " (" << target.sources.size() << " sources)";
                }
                std::cout << "\n";
            }

            return 0;
        }

        case Command::DEPS: {
            // Parse to build graph
            BuildResult dummy = orchestrator.check();

            std::cout << orchestrator.dependency_graph_dot();
            return 0;
        }

        case Command::TEST: {
            namespace fs = std::filesystem;
            std::string dir = opts.config.project_root.empty() ? "." : opts.config.project_root;
            std::vector<fs::path> test_files;

            for (const auto& entry : fs::directory_iterator(dir)) {
                if (!entry.is_regular_file()) continue;
                std::string name = entry.path().filename().string();
                if ((name.size() > 5 && name.substr(0, 5) == "test_" && name.substr(name.size() - 5) == ".aria") ||
                    (name.size() > 10 && name.substr(name.size() - 10) == "_test.aria")) {
                    test_files.push_back(entry.path());
                }
            }

            std::sort(test_files.begin(), test_files.end());

            if (test_files.empty()) {
                std::cout << "No test files found (test_*.aria or *_test.aria)\n";
                return 0;
            }

            int passed = 0, failed = 0, errors = 0;

            std::cout << "Running " << test_files.size() << " test file(s)...\n\n";

            for (const auto& tf : test_files) {
                std::string stem = tf.stem().string();
                std::string out = (fs::path(dir) / stem).string();
                std::string compile_cmd = "ariac " + tf.string() + " -o " + out + " 2>&1";

                if (opts.config.verbose) {
                    std::cout << "  compile: " << compile_cmd << "\n";
                }

                int compile_rc = std::system(compile_cmd.c_str());
                if (compile_rc != 0) {
                    std::cout << "  FAIL (compile error): " << tf.filename().string() << "\n";
                    errors++;
                    continue;
                }

                std::string run_cmd = out + " 2>&1";
                int run_rc = std::system(run_cmd.c_str());

                if (run_rc == 0) {
                    std::cout << "  PASS: " << tf.filename().string() << "\n";
                    passed++;
                } else {
                    std::cout << "  FAIL: " << tf.filename().string()
                              << " (exit code " << WEXITSTATUS(run_rc) << ")\n";
                    failed++;
                }

                // Clean up compiled binary
                fs::remove(out);
            }

            std::cout << "\nResults: " << passed << " passed, "
                      << failed << " failed, " << errors << " errors"
                      << " (" << test_files.size() << " total)\n";

            return (failed + errors) > 0 ? 1 : 0;
        }
    }

    return 0;
}
