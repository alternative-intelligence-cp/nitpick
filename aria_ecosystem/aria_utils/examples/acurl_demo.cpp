/**
 * acurl_demo.cpp
 * Interactive demonstration of the acurl (HTTP client) utility
 *
 * Shows six-stream HTTP with TBB-safe transfer tracking
 */

#include "acurl/http_client.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>

using namespace aria::acurl;

void print_section(const std::string& title) {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::left << std::setw(56) << title << "  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
}

// Demo 1: Simple GET request
void demo_simple_get() {
    print_section("Demo 1: Simple GET Request");

    HttpRequest req;
    req.method = HttpMethod::GET;
    req.url = "https://httpbin.org/get";

    HttpClient client;
    
    // Capture response body in memory
    std::string response_body;
    client.set_data_callback([&response_body](const uint8_t* data, size_t size, void*) -> int64_t {
        response_body.append(reinterpret_cast<const char*>(data), size);
        return static_cast<int64_t>(size);
    });

    auto [response, error] = client.perform(req);

    if (error == ErrorCode::OK) {
        std::cout << "✓ Status: " << response.status_code << " " << response.status_text << "\n";
        std::cout << "  Content-Type: " << response.content_type << "\n";
        std::cout << "  Content-Length: " << response.content_length << " bytes\n";
        std::cout << "  Total Time: " << format_duration(response.total_time_us) << "\n";
        std::cout << "\nResponse preview:\n";
        std::cout << response_body.substr(0, 200) << "...\n";
    } else {
        std::cout << "✗ Error: " << error_to_string(error) << "\n";
    }
}

// Demo 2: Custom headers
void demo_custom_headers() {
    print_section("Demo 2: Custom Headers");

    HttpRequest req;
    req.url = "https://httpbin.org/headers";
    req.headers = {
        {"X-Custom-Header", "AriaValue"},
        {"Accept", "application/json"},
        {"User-Agent", "Aria-acurl/1.0"}
    };

    HttpClient client;
    std::string response_body;
    client.set_data_callback([&response_body](const uint8_t* data, size_t size, void*) -> int64_t {
        response_body.append(reinterpret_cast<const char*>(data), size);
        return static_cast<int64_t>(size);
    });

    auto [response, error] = client.perform(req);

    if (error == ErrorCode::OK) {
        std::cout << "✓ Headers sent successfully\n";
        std::cout << "\nServer echoed:\n";
        std::cout << response_body.substr(0, 300) << "\n";
    }
}

// Demo 3: POST with JSON body
void demo_post_json() {
    print_section("Demo 3: POST with JSON Body");

    HttpRequest req;
    req.method = HttpMethod::POST;
    req.url = "https://httpbin.org/post";
    req.headers = {
        {"Content-Type", "application/json"}
    };
    req.body = R"({"name":"Aria","version":"0.1.0","safe":true})";

    HttpClient client;
    std::string response_body;
    client.set_data_callback([&response_body](const uint8_t* data, size_t size, void*) -> int64_t {
        response_body.append(reinterpret_cast<const char*>(data), size);
        return static_cast<int64_t>(size);
    });

    auto [response, error] = client.perform(req);

    if (error == ErrorCode::OK) {
        std::cout << "✓ POST successful: " << response.status_code << "\n";
        std::cout << "\nResponse preview:\n";
        std::cout << response_body.substr(0, 250) << "...\n";
    }
}

// Demo 4: Progress tracking with callback
void demo_progress_tracking() {
    print_section("Demo 4: Progress Tracking");

    HttpRequest req;
    req.url = "https://httpbin.org/bytes/65536";  // Download 64KB

    HttpClient client;
    
    client.set_progress_callback([](const TransferStats& stats, void*) -> bool {
        if (stats.bytes_total > 0) {
            std::cout << "\r  Progress: " << std::fixed << std::setprecision(1) 
                      << stats.progress_percent << "% ("
                      << stats.bytes_downloaded << " / " << stats.bytes_total << " bytes)    ";
            std::cout.flush();
        }
        return true;  // Continue transfer
    });

    std::string response_body;
    client.set_data_callback([&response_body](const uint8_t* data, size_t size, void*) -> int64_t {
        response_body.append(reinterpret_cast<const char*>(data), size);
        return static_cast<int64_t>(size);
    });

    auto [response, error] = client.perform(req);

    if (error == ErrorCode::OK) {
        std::cout << "\n✓ Downloaded " << client.get_stats().bytes_downloaded << " bytes\n";
        std::cout << "  Average speed: " << format_speed(client.get_stats().speed_download) << "\n";
    }
}

// Demo 5: Timing breakdown (TBB-safe)
void demo_timing_breakdown() {
    print_section("Demo 5: Timing Breakdown (TBB-safe)");

    HttpRequest req;
    req.url = "https://httpbin.org/delay/1";  // Server delays 1 second

    HttpClient client;
    auto [response, error] = client.perform(req);

    if (error == ErrorCode::OK) {
        std::cout << "Timing breakdown:\n";
        std::cout << "  DNS lookup:     " << format_duration(response.dns_time_us) 
                  << (is_tbb_err(response.dns_time_us) ? " (ERR)" : "") << "\n";
        std::cout << "  TCP connect:    " << format_duration(response.connect_time_us)
                  << (is_tbb_err(response.connect_time_us) ? " (ERR)" : "") << "\n";
        std::cout << "  SSL handshake:  " << format_duration(response.ssl_time_us)
                  << (is_tbb_err(response.ssl_time_us) ? " (ERR)" : "") << "\n";
        std::cout << "  Time to 1st byte: " << format_duration(response.ttfb_us)
                  << (is_tbb_err(response.ttfb_us) ? " (ERR)" : "") << "\n";
        std::cout << "  Total time:     " << format_duration(response.total_time_us)
                  << (is_tbb_err(response.total_time_us) ? " (ERR)" : "") << "\n";
    }
}

// Demo 6: Response header inspection
void demo_response_headers() {
    print_section("Demo 6: Response Headers");

    HttpRequest req;
    req.url = "https://httpbin.org/response-headers?Custom-Header=CustomValue";

    HttpClient client;
    auto [response, error] = client.perform(req);

    if (error == ErrorCode::OK) {
        std::cout << "Response headers (" << response.headers.size() << " total):\n";
        for (const auto& header : response.headers) {
            std::cout << "  " << header.name << ": " << header.value << "\n";
        }

        // Use helper to get specific header
        auto server = response.get_header("Server");
        if (server) {
            std::cout << "\n✓ Server header found: " << *server << "\n";
        }
    }
}

// Demo 7: Redirect following
void demo_redirects() {
    print_section("Demo 7: Redirect Following");

    HttpRequest req;
    req.url = "https://httpbin.org/redirect/3";  // 3 redirects
    req.follow_redirects = true;
    req.max_redirects = 5;

    HttpClient client;
    auto [response, error] = client.perform(req);

    if (error == ErrorCode::OK) {
        std::cout << "✓ Final status: " << response.status_code << "\n";
        std::cout << "  (Followed up to " << req.max_redirects << " redirects)\n";
    }
}

// Demo 8: Timeout handling
void demo_timeout() {
    print_section("Demo 8: Timeout Handling");

    HttpRequest req;
    req.url = "https://httpbin.org/delay/10";  // Server delays 10 seconds
    req.transfer_timeout = 2;  // But we timeout after 2 seconds

    std::cout << "Attempting request with 2s timeout (server delays 10s)...\n";

    HttpClient client;
    auto [response, error] = client.perform(req);

    if (error == ErrorCode::TIMEOUT) {
        std::cout << "✓ Request timed out as expected\n";
    } else if (error == ErrorCode::OK) {
        std::cout << "✗ Request completed (should have timed out)\n";
    } else {
        std::cout << "✗ Unexpected error: " << error_to_string(error) << "\n";
    }
}

// Demo 9: Error handling
void demo_error_handling() {
    print_section("Demo 9: Error Handling");

    // Test 1: Invalid URL
    HttpRequest req1;
    req1.url = "not-a-valid-url";

    HttpClient client1;
    auto [resp1, err1] = client1.perform(req1);
    std::cout << "Invalid URL: " << error_to_string(err1) << "\n";

    // Test 2: Non-existent domain
    HttpRequest req2;
    req2.url = "https://this-domain-definitely-does-not-exist-12345.com";
    req2.connect_timeout = 2;

    HttpClient client2;
    auto [resp2, err2] = client2.perform(req2);
    std::cout << "Non-existent domain: " << error_to_string(err2) << "\n";
}

// Demo 10: TBB arithmetic safety
void demo_tbb_arithmetic() {
    print_section("Demo 10: TBB Arithmetic Safety");

    std::cout << "TBB-safe byte counting:\n\n";

    int64_t bytes1 = 1000000;
    int64_t bytes2 = 2000000;
    int64_t total = tbb_add(bytes1, bytes2);

    std::cout << "  " << bytes1 << " + " << bytes2 << " = " << total << "\n";
    std::cout << "  Valid: " << !is_tbb_err(total) << "\n\n";

    // Simulate overflow scenario
    int64_t huge1 = INT64_MAX - 1000;
    int64_t huge2 = 2000;
    int64_t overflow = tbb_add(huge1, huge2);

    std::cout << "  " << huge1 << " + " << huge2 << " = ";
    if (is_tbb_err(overflow)) {
        std::cout << "TBB64_ERR (overflow detected!)\n";
        std::cout << "  ✓ Safe overflow handling - sticky sentinel preserved\n";
    } else {
        std::cout << overflow << "\n";
        std::cout << "  ✗ Should have returned ERR sentinel\n";
    }
}

// Demo 11: Hex-Stream convenience API
void demo_hex_stream() {
    print_section("Demo 11: Hex-Stream Convenience API");

    std::cout << "Using hex_get() convenience function:\n\n";

    auto result = hex_get("https://httpbin.org/uuid", false);

    if (result.error == ErrorCode::OK) {
        std::cout << "✓ Transfer complete\n";
        std::cout << "  Status: " << result.response.status_code << "\n";
        std::cout << "  Bytes: " << result.stats.bytes_downloaded << "\n";
        std::cout << "  Speed: " << format_speed(result.stats.speed_download) << "\n";
        
        if (result.bytes_to_stddato > 0) {
            std::cout << "  Binary data written to stddato (FD 5)\n";
        }
    } else {
        std::cout << "✗ Error: " << error_to_string(result.error) << "\n";
    }
}

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║   acurl - Interactive Demonstration                       ║\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║  HTTP client with six-stream architecture                 ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    try {
        demo_simple_get();
        demo_custom_headers();
        demo_post_json();
        demo_progress_tracking();
        demo_timing_breakdown();
        demo_response_headers();
        demo_redirects();
        demo_timeout();
        demo_error_handling();
        demo_tbb_arithmetic();
        demo_hex_stream();

        std::cout << "\n✓ All 11 demonstrations completed successfully!\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
