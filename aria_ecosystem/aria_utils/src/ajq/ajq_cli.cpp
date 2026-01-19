// ============================================================================
// AJQ - JSON Query CLI with Six-Stream Topology
// ============================================================================
// Purpose: JSON parsing, querying, and formatting utility
//
// Six-Stream Architecture:
//   FD 0 (stdin):   JSON input
//   FD 1 (stdout):  Formatted JSON output
//   FD 2 (stderr):  Error messages
//   FD 3 (stddbg):  Debug telemetry (NDJSON)
//   FD 4 (stddati): Not used
//   FD 5 (stddato): Binary JSON (future)
//
// Usage:
//   ajq [OPTIONS] [FILTER]
//
// Options:
//   -c, --compact  Compact output (no pretty-print)
//   -r, --raw      Raw string output (no JSON quotes)
//   -s, --slurp    Read entire input into array
//   --debug        Enable debug telemetry on FD 3
//   --help         Show this help message
//
// Examples:
//   echo '{"name":"aria"}' | ajq
//   ajq -c < input.json
//   ajq '.' < data.json
// ============================================================================

#include "json_query.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <chrono>

// FD constants
#ifndef STDDBG_FILENO
#define STDDBG_FILENO 3
#endif

using namespace aria::ajq;

// ============================================================================
// Options
// ============================================================================
struct Options {
    bool compact = false;
    bool raw_output = false;
    bool slurp = false;
    bool debug = false;
    std::string filter = ".";  // Identity filter by default
};

// ============================================================================
// Output Functions
// ============================================================================

void print_json(const JsonValue& value, bool compact, int indent = 0) {
    std::string indent_str = compact ? "" : std::string(indent * 2, ' ');
    std::string newline = compact ? "" : "\n";
    
    if (value.is_null()) {
        std::cout << "null";
    }
    else if (value.is_bool()) {
        std::cout << (value.as_bool() ? "true" : "false");
    }
    else if (value.is_number()) {
        std::cout << value.as_number();
    }
    else if (value.is_string()) {
        std::cout << "\"" << value.as_string() << "\"";
    }
    else if (value.is_array()) {
        const auto& arr = value.as_array();
        std::cout << "[" << newline;
        
        for (size_t i = 0; i < arr.size(); i++) {
            if (!compact) std::cout << std::string((indent + 1) * 2, ' ');
            print_json(arr[i], compact, indent + 1);
            if (i < arr.size() - 1) std::cout << ",";
            std::cout << newline;
        }
        
        if (!compact && !arr.empty()) std::cout << indent_str;
        std::cout << "]";
    }
    else if (value.is_object()) {
        const auto& obj = value.as_object();
        std::cout << "{" << newline;
        
        size_t i = 0;
        for (const auto& [key, val] : obj) {
            if (!compact) std::cout << std::string((indent + 1) * 2, ' ');
            std::cout << "\"" << key << "\":" << (compact ? "" : " ");
            print_json(val, compact, indent + 1);
            if (++i < obj.size()) std::cout << ",";
            std::cout << newline;
        }
        
        if (!compact && !obj.empty()) std::cout << indent_str;
        std::cout << "}";
    }
}

// ============================================================================
// Main Processing
// ============================================================================

int process_json(const Options& opts) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Read all input
    std::stringstream buffer;
    buffer << std::cin.rdbuf();
    std::string json_input = buffer.str();
    
    if (opts.debug) {
        dprintf(STDDBG_FILENO,
            "{\"level\":\"debug\",\"component\":\"ajq\","
            "\"message\":\"Processing JSON input\",\"bytes\":%zu}\n",
            json_input.size()
        );
    }
    
    // Parse JSON
    JsonParser parser(json_input);
    JsonValue value = parser.parse();
    
    if (parser.has_error()) {
        std::cerr << "ajq: parse error: " << parser.error() << "\n";
        
        if (opts.debug) {
            dprintf(STDDBG_FILENO,
                "{\"level\":\"error\",\"component\":\"ajq\","
                "\"message\":\"Parse failed\",\"error\":\"%s\"}\n",
                parser.error().c_str()
            );
        }
        
        return 1;
    }
    
    // For now, just output the parsed value (identity filter ".")
    // Future: implement actual query filtering
    
    if (opts.raw_output && value.is_string()) {
        std::cout << value.as_string() << "\n";
    } else {
        print_json(value, opts.compact);
        if (!opts.compact) std::cout << "\n";
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    if (opts.debug) {
        dprintf(STDDBG_FILENO,
            "{\"level\":\"telemetry\",\"component\":\"ajq\","
            "\"bytes_read\":%zu,\"bytes_written\":0,"
            "\"elapsed_ms\":%.6f}\n",
            json_input.size(),
            elapsed_ms
        );
    }
    
    return 0;
}

// ============================================================================
// Help and Main
// ============================================================================

void print_help(const char* program) {
    std::cout << "Usage: " << program << " [OPTIONS] [FILTER]\n\n"
              << "JSON parsing and formatting utility\n\n"
              << "Options:\n"
              << "  -c, --compact  Compact output (no pretty-print)\n"
              << "  -r, --raw      Raw string output (no JSON quotes)\n"
              << "  -s, --slurp    Read entire input into array\n"
              << "  --debug        Enable debug telemetry on FD 3\n"
              << "  --help         Show this help message\n\n"
              << "Examples:\n"
              << "  echo '{\"name\":\"aria\"}' | ajq\n"
              << "  ajq -c < input.json\n"
              << "  ajq --debug < data.json 3> telemetry.log\n";
}

int main(int argc, char* argv[]) {
    Options opts;
    
    // Filter --debug and --help before getopt
    std::vector<char*> filtered_argv;
    filtered_argv.push_back(argv[0]);
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            opts.debug = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--compact") == 0) {
            opts.compact = true;
        } else if (strcmp(argv[i], "--raw") == 0) {
            opts.raw_output = true;
        } else if (strcmp(argv[i], "--slurp") == 0) {
            opts.slurp = true;
        } else {
            filtered_argv.push_back(argv[i]);
        }
    }
    
    argc = filtered_argv.size();
    argv = filtered_argv.data();
    
    // Parse options
    int opt;
    while ((opt = getopt(argc, argv, "crs")) != -1) {
        switch (opt) {
            case 'c':
                opts.compact = true;
                break;
            case 'r':
                opts.raw_output = true;
                break;
            case 's':
                opts.slurp = true;
                break;
            default:
                print_help(filtered_argv[0]);
                return 1;
        }
    }
    
    // Get filter if provided
    if (optind < argc) {
        opts.filter = argv[optind];
    }
    
    return process_json(opts);
}
