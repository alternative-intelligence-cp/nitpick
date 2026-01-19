// ============================================================================
// ATEST - Test Framework CLI with Six-Stream Topology
// ============================================================================
// Purpose: Test runner for Aria test suites
//
// Six-Stream Architecture:
//   FD 0 (stdin):   Not used
//   FD 1 (stdout):  Test results, progress
//   FD 2 (stderr):  Error messages
//   FD 3 (stddbg):  Debug telemetry (NDJSON)
//   FD 4 (stddati): Not used
//   FD 5 (stddato): Binary test results (future)
//
// Usage:
//   atest [OPTIONS]
//
// Options:
//   -l, --list     List all tests
//   -f FILTER      Run tests matching filter
//   -g GROUP       Run tests in group
//   -v, --verbose  Verbose output
//   --debug        Enable debug telemetry on FD 3
//   --help         Show this help message
// ============================================================================

#include "test_framework.hpp"
#include <iostream>
#include <cstring>
#include <unistd.h>

// FD constants
#ifndef STDDBG_FILENO
#define STDDBG_FILENO 3
#endif

using namespace aria::test;

// ============================================================================
// Options
// ============================================================================
struct Options {
    bool list_tests = false;
    bool verbose = false;
    bool debug = false;
    std::string filter;
    std::string group;
};

// ============================================================================
// Main Processing
// ============================================================================

int run_tests(const Options& opts) {
    TestRunner runner;
    
    if (opts.list_tests) {
        runner.list_tests();
        return 0;
    }
    
    if (opts.debug) {
        dprintf(STDDBG_FILENO,
            "{\"level\":\"debug\",\"component\":\"atest\","
            "\"message\":\"Running tests\"}\n"
        );
    }
    
    TestStats stats;
    
    if (!opts.filter.empty()) {
        stats = runner.run_filtered(opts.filter, opts.verbose);
    } else if (!opts.group.empty()) {
        stats = runner.run_group(opts.group, opts.verbose);
    } else {
        stats = runner.run_all(opts.verbose);
    }
    
    // Print summary
    std::cout << "\n";
    std::cout << "Tests run: " << stats.total << "\n";
    std::cout << "  Passed: " << stats.passed << "\n";
    std::cout << "  Failed: " << stats.failed << "\n";
    std::cout << "  Skipped: " << stats.skipped << "\n";
    std::cout << "  Duration: " << (stats.duration_ns / 1000000.0) << " ms\n";
    
    if (opts.debug) {
        dprintf(STDDBG_FILENO,
            "{\"level\":\"telemetry\",\"component\":\"atest\","
            "\"tests_total\":%d,\"tests_passed\":%d,"
            "\"tests_failed\":%d,\"tests_skipped\":%d,"
            "\"duration_ns\":%lu}\n",
            stats.total, stats.passed, stats.failed, stats.skipped,
            stats.duration_ns
        );
    }
    
    return stats.failed > 0 ? 1 : 0;
}

// ============================================================================
// Help and Main
// ============================================================================

void print_help(const char* program) {
    std::cout << "Usage: " << program << " [OPTIONS]\n\n"
              << "Test framework runner\n\n"
              << "Options:\n"
              << "  -l, --list     List all tests\n"
              << "  -f FILTER      Run tests matching filter\n"
              << "  -g GROUP       Run tests in group\n"
              << "  -v, --verbose  Verbose output\n"
              << "  --debug        Enable debug telemetry on FD 3\n"
              << "  --help         Show this help message\n\n"
              << "Examples:\n"
              << "  atest\n"
              << "  atest -l\n"
              << "  atest -f network\n"
              << "  atest -g integration\n";
}

int main(int argc, char* argv[]) {
    Options opts;
    
    // Filter long options
    std::vector<char*> filtered_argv;
    filtered_argv.push_back(argv[0]);
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            opts.debug = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--list") == 0) {
            opts.list_tests = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            opts.verbose = true;
        } else {
            filtered_argv.push_back(argv[i]);
        }
    }
    
    argc = filtered_argv.size();
    argv = filtered_argv.data();
    
    // Parse options
    int opt;
    while ((opt = getopt(argc, argv, "lf:g:v")) != -1) {
        switch (opt) {
            case 'l':
                opts.list_tests = true;
                break;
            case 'f':
                opts.filter = optarg;
                break;
            case 'g':
                opts.group = optarg;
                break;
            case 'v':
                opts.verbose = true;
                break;
            default:
                print_help(filtered_argv[0]);
                return 1;
        }
    }
    
    return run_tests(opts);
}
