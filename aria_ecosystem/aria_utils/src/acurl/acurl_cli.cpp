// ============================================================================
// ACURL - HTTP Client CLI with Six-Stream Topology
// ============================================================================
// Purpose: HTTP/HTTPS client utility similar to curl
//
// Six-Stream Architecture:
//   FD 0 (stdin):   Request body (for POST/PUT)
//   FD 1 (stdout):  Response body
//   FD 2 (stderr):  Error messages, progress
//   FD 3 (stddbg):  Debug telemetry (NDJSON)
//   FD 4 (stddati): Not used
//   FD 5 (stddato): Binary response data (alternative)
//
// Usage:
//   acurl [OPTIONS] URL
//
// Options:
//   -X METHOD      HTTP method (GET, POST, PUT, DELETE, etc.)
//   -H HEADER      Add header (format: "Name: Value")
//   -d DATA        Request body data
//   -o FILE        Write output to file instead of stdout
//   -i, --include  Include response headers in output
//   -s, --silent   Silent mode (no progress)
//   -v, --verbose  Verbose output
//   --debug        Enable debug telemetry on FD 3
//   --help         Show this help message
// ============================================================================

#include "acurl/http_client.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <chrono>

using namespace aria::acurl;

// ============================================================================
// Options
// ============================================================================
struct Options {
    HttpMethod method = HttpMethod::GET;
    std::string url;
    std::vector<HttpHeader> headers;
    std::string data;
    std::string output_file;
    bool include_headers = false;
    bool silent = false;
    bool verbose = false;
    bool debug = false;
};

// ============================================================================
// Helper Functions
// ============================================================================

HttpMethod parse_method(const std::string& method_str) {
    if (method_str == "GET") return HttpMethod::GET;
    if (method_str == "HEAD") return HttpMethod::HEAD;
    if (method_str == "POST") return HttpMethod::POST;
    if (method_str == "PUT") return HttpMethod::PUT;
    if (method_str == "DELETE") return HttpMethod::DELETE;
    if (method_str == "PATCH") return HttpMethod::PATCH;
    if (method_str == "OPTIONS") return HttpMethod::OPTIONS;
    return HttpMethod::GET;
}

HttpHeader parse_header(const std::string& header_str) {
    size_t colon = header_str.find(':');
    if (colon == std::string::npos) {
        return HttpHeader{"", ""};
    }
    
    std::string name = header_str.substr(0, colon);
    std::string value = header_str.substr(colon + 1);
    
    // Trim leading whitespace from value
    size_t start = value.find_first_not_of(" \t");
    if (start != std::string::npos) {
        value = value.substr(start);
    }
    
    return HttpHeader{name, value};
}

// ============================================================================
// Progress Callback
// ============================================================================

struct ResponseData {
    std::string body;
    bool silent;
};

bool progress_callback(const TransferStats& stats, void* user_data) {
    ResponseData* data = static_cast<ResponseData*>(user_data);
    if (data->silent) return true;
    
    if (stats.bytes_total > 0) {
        double percent = (100.0 * stats.bytes_downloaded) / stats.bytes_total;
        std::cerr << "\rDownloading: " << stats.bytes_downloaded << " / " 
                  << stats.bytes_total 
                  << " (" << static_cast<int>(percent) << "%)    ";
        std::cerr.flush();
    } else {
        std::cerr << "\rDownloading: " << stats.bytes_downloaded << " bytes    ";
        std::cerr.flush();
    }
    return true;  // Continue
}

int64_t data_callback(const uint8_t* data, size_t size, void* user_data) {
    ResponseData* resp_data = static_cast<ResponseData*>(user_data);
    resp_data->body.append(reinterpret_cast<const char*>(data), size);
    return static_cast<int64_t>(size);
}

// ============================================================================
// Main Processing
// ============================================================================

int perform_request(const Options& opts) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (opts.debug) {
        dprintf(FD_STDDBG,
            "{\"level\":\"debug\",\"component\":\"acurl\","
            "\"message\":\"Requesting: %s\",\"method\":\"%s\"}\n",
            opts.url.c_str(),
            method_to_string(opts.method)
        );
    }
    
    // Create HTTP request
    HttpRequest request;
    request.method = opts.method;
    request.url = opts.url;
    request.headers = opts.headers;
    request.body = opts.data;
    
    // Create HTTP client and response data
    HttpClient client;
    ResponseData resp_data;
    resp_data.silent = opts.silent;
    
    // Set callbacks to collect data in memory
    client.set_data_callback(data_callback, &resp_data);
    client.set_progress_callback(progress_callback, &resp_data);
    
    // Perform request
    auto [response, error] = client.perform(request);
    
    if (!opts.silent && resp_data.body.size() > 0) {
        std::cerr << "\n";  // New line after progress
    }
    
    if (error != ErrorCode::OK) {
        std::cerr << "acurl: " << error_to_string(error) << "\n";
        
        if (opts.debug) {
            dprintf(FD_STDDBG,
                "{\"level\":\"error\",\"component\":\"acurl\","
                "\"message\":\"Request failed\",\"error\":\"%s\"}\n",
                error_to_string(error)
            );
        }
        
        return 1;
    }
    
    // Output headers if requested
    if (opts.include_headers) {
        std::cout << "HTTP/1.1 " << response.status_code << " "
                  << response.status_text << "\n";
        for (const auto& header : response.headers) {
            std::cout << header.name << ": " << header.value << "\n";
        }
        std::cout << "\n";
    }
    
    // Output body
    if (opts.output_file.empty()) {
        // Write to stdout
        std::cout << resp_data.body;
        std::cout.flush();
    } else {
        // Write to file
        std::ofstream outfile(opts.output_file, std::ios::binary);
        if (!outfile) {
            std::cerr << "acurl: cannot write to '" << opts.output_file << "'\n";
            return 1;
        }
        outfile << resp_data.body;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Calculate throughput
    double throughput_mbps = 0.0;
    if (elapsed_ms > 0) {
        double bytes_per_second = (resp_data.body.size() * 1000.0) / elapsed_ms;
        throughput_mbps = (bytes_per_second * 8.0) / (1024.0 * 1024.0);
    }
    
    if (opts.debug) {
        dprintf(FD_STDDBG,
            "{\"level\":\"telemetry\",\"component\":\"acurl\","
            "\"status_code\":%d,\"bytes_received\":%zu,"
            "\"elapsed_ms\":%.6f,\"throughput_mbps\":%.2f}\n",
            response.status_code,
            resp_data.body.size(),
            elapsed_ms,
            throughput_mbps
        );
    }
    
    if (opts.verbose) {
        std::cerr << "Status: " << response.status_code << " " << response.status_text << "\n";
        std::cerr << "Bytes: " << resp_data.body.size() << "\n";
        std::cerr << "Time: " << elapsed_ms << " ms\n";
        std::cerr << "Throughput: " << throughput_mbps << " Mbps\n";
    }
    
    return 0;
}

// ============================================================================
// Help and Main
// ============================================================================

void print_help(const char* program) {
    std::cout << "Usage: " << program << " [OPTIONS] URL\n\n"
              << "HTTP/HTTPS client utility\n\n"
              << "Options:\n"
              << "  -X METHOD      HTTP method (GET, POST, PUT, DELETE)\n"
              << "  -H HEADER      Add header (format: \"Name: Value\")\n"
              << "  -d DATA        Request body data\n"
              << "  -o FILE        Write output to file\n"
              << "  -i, --include  Include response headers\n"
              << "  -s, --silent   Silent mode (no progress)\n"
              << "  -v, --verbose  Verbose output\n"
              << "  --debug        Enable debug telemetry on FD 3\n"
              << "  --help         Show this help message\n\n"
              << "Examples:\n"
              << "  acurl https://example.com\n"
              << "  acurl -X POST -H \"Content-Type: application/json\" -d '{\"key\":\"value\"}' https://api.example.com\n"
              << "  acurl --debug https://example.com 3> telemetry.log\n";
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
        } else if (strcmp(argv[i], "--include") == 0) {
            opts.include_headers = true;
        } else if (strcmp(argv[i], "--silent") == 0) {
            opts.silent = true;
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
    while ((opt = getopt(argc, argv, "X:H:d:o:isv")) != -1) {
        switch (opt) {
            case 'X':
                opts.method = parse_method(optarg);
                break;
            case 'H':
                opts.headers.push_back(parse_header(optarg));
                break;
            case 'd':
                opts.data = optarg;
                break;
            case 'o':
                opts.output_file = optarg;
                break;
            case 'i':
                opts.include_headers = true;
                break;
            case 's':
                opts.silent = true;
                break;
            case 'v':
                opts.verbose = true;
                break;
            default:
                print_help(filtered_argv[0]);
                return 1;
        }
    }
    
    // Get URL
    if (optind >= argc) {
        std::cerr << "acurl: missing URL\n";
        print_help(filtered_argv[0]);
        return 1;
    }
    
    opts.url = argv[optind];
    
    return perform_request(opts);
}
