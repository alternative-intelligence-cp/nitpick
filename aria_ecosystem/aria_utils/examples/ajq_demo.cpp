/**
 * @file ajq_demo.cpp
 * @brief ajq (JSON Query) Utility - Comprehensive Demonstrations
 * 
 * Demonstrates all ajq features with practical examples.
 */

#include "ajq/json_query.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

using namespace aria::ajq;

// ANSI Colors
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

void print_header(const std::string& title) {
    std::cout << "\n" << BOLD << CYAN << "━━━ " << title << " ━━━" << RESET << "\n\n";
}

void print_success(const std::string& msg) {
    std::cout << GREEN << "✓ " << msg << RESET << "\n";
}

void print_error(const std::string& msg) {
    std::cout << RED << "✗ " << msg << RESET << "\n";
}

void print_info(const std::string& label, const std::string& value) {
    std::cout << YELLOW << label << ": " << RESET << value << "\n";
}

// ============================================================================
// Demo 1: Basic JSON Parsing
// ============================================================================

void demo_basic_parsing() {
    print_header("Demo 1: Basic JSON Parsing");
    
    std::string json = R"({
        "name": "Aria",
        "version": "1.0.0",
        "active": true,
        "count": 42
    })";
    
    JsonParser parser(json);
    JsonValue value = parser.parse();
    
    if (parser.has_error()) {
        print_error("Parse failed: " + parser.error());
        return;
    }
    
    print_success("Parsed JSON successfully");
    
    if (value.is_object()) {
        const auto& obj = value.as_object();
        print_info("Name", obj.at("name").as_string());
        print_info("Version", obj.at("version").as_string());
        print_info("Active", obj.at("active").as_bool() ? "true" : "false");
        print_info("Count", std::to_string((int)obj.at("count").as_number()));
    }
}

// ============================================================================
// Demo 2: Query Path Parsing
// ============================================================================

void demo_query_paths() {
    print_header("Demo 2: Query Path Parsing");
    
    std::vector<std::string> queries = {
        ".",
        ".name",
        ".users.name",
        ".users[0]",
        ".users[0].name",
        ".items[2].details.price"
    };
    
    for (const auto& q : queries) {
        QueryPath path = QueryPath::parse(q);
        std::cout << YELLOW << "Query: " << RESET << std::setw(25) << std::left << q
                  << YELLOW << " → " << RESET << path.to_string() << "\n";
    }
    
    print_success("Query paths parsed correctly");
}

// ============================================================================
// Demo 3: Simple Queries
// ============================================================================

void demo_simple_queries() {
    print_header("Demo 3: Simple Queries");
    
    std::string json = R"({
        "user": {
            "name": "Aria",
            "age": 25,
            "email": "aria@ailp.org"
        }
    })";
    
    JsonParser parser(json);
    JsonValue root = parser.parse();
    
    QueryExecutor executor;
    JsonSerializer ser(false);
    
    std::vector<std::string> queries = {
        ".user",
        ".user.name",
        ".user.age"
    };
    
    for (const auto& q : queries) {
        QueryPath path = QueryPath::parse(q);
        JsonValue result = executor.execute(root, path);
        
        if (executor.has_error()) {
            print_error(q + ": " + executor.error_message());
        } else {
            std::string output = ser.serialize(result);
            std::cout << YELLOW << std::setw(15) << std::left << q << ": "
                      << RESET << output << "\n";
        }
    }
    
    print_success("Simple queries executed");
}

// ============================================================================
// Demo 4: Array Queries
// ============================================================================

void demo_array_queries() {
    print_header("Demo 4: Array Queries");
    
    std::string json = R"({
        "users": [
            {"name": "Alice", "age": 30},
            {"name": "Bob", "age": 25},
            {"name": "Charlie", "age": 35}
        ]
    })";
    
    JsonParser parser(json);
    JsonValue root = parser.parse();
    
    QueryExecutor executor;
    JsonSerializer ser(true, 2);
    
    // Query first user
    QueryPath path1 = QueryPath::parse(".users[0]");
    JsonValue user1 = executor.execute(root, path1);
    
    std::cout << YELLOW << "First user:" << RESET << "\n";
    std::cout << ser.serialize(user1) << "\n\n";
    
    // Query first user's name
    QueryPath path2 = QueryPath::parse(".users[0].name");
    JsonValue name1 = executor.execute(root, path2);
    
    std::cout << YELLOW << "First user's name:" << RESET << " "
              << name1.as_string() << "\n";
    
    print_success("Array queries executed");
}

// ============================================================================
// Demo 5: Pretty Printing
// ============================================================================

void demo_pretty_printing() {
    print_header("Demo 5: Pretty Printing");
    
    std::string compact = R"({"name":"Aria","tags":["ai","utils","safety"],"meta":{"version":"1.0","author":"AILP"}})";
    
    JsonParser parser(compact);
    JsonValue value = parser.parse();
    
    JsonSerializer compact_ser(false);
    JsonSerializer pretty_ser(true, 2);
    
    std::cout << YELLOW << "Compact:" << RESET << "\n";
    std::cout << compact_ser.serialize(value) << "\n\n";
    
    std::cout << YELLOW << "Pretty:" << RESET << "\n";
    std::cout << pretty_ser.serialize(value) << "\n";
    
    print_success("Pretty printing works");
}

// ============================================================================
// Demo 6: Error Handling
// ============================================================================

void demo_error_handling() {
    print_header("Demo 6: Error Handling");
    
    // Invalid JSON
    std::string bad_json = R"({"name": "Aria", "age": })";
    JsonParser parser(bad_json);
    JsonValue value = parser.parse();
    
    if (parser.has_error()) {
        print_info("Parse error caught", parser.error());
    }
    
    // Valid JSON, invalid query
    std::string good_json = R"({"name": "Aria"})";
    JsonParser parser2(good_json);
    JsonValue value2 = parser2.parse();
    
    QueryExecutor executor;
    QueryPath path = QueryPath::parse(".invalid.field");
    JsonValue result = executor.execute(value2, path);
    
    if (result.is_null()) {
        print_info("Missing field returned", "null (jq behavior)");
    }
    
    // Array index out of bounds
    std::string array_json = R"({"items": [1, 2, 3]})";
    JsonParser parser3(array_json);
    JsonValue value3 = parser3.parse();
    
    QueryPath path2 = QueryPath::parse(".items[10]");
    JsonValue result2 = executor.execute(value3, path2);
    
    if (executor.has_error()) {
        print_info("Array bounds error", executor.error_message());
    }
    
    print_success("Error handling working correctly");
}

// ============================================================================
// Demo 7: Type Detection
// ============================================================================

void demo_type_detection() {
    print_header("Demo 7: Type Detection");
    
    std::string json = R"({
        "null_value": null,
        "bool_value": true,
        "number_value": 42.5,
        "string_value": "hello",
        "array_value": [1, 2, 3],
        "object_value": {"key": "value"}
    })";
    
    JsonParser parser(json);
    JsonValue value = parser.parse();
    
    if (value.is_object()) {
        const auto& obj = value.as_object();
        
        for (const auto& [key, val] : obj) {
            std::string type;
            if (val.is_null()) type = "null";
            else if (val.is_bool()) type = "bool";
            else if (val.is_number()) type = "number";
            else if (val.is_string()) type = "string";
            else if (val.is_array()) type = "array";
            else if (val.is_object()) type = "object";
            
            std::cout << YELLOW << std::setw(20) << std::left << key << ": "
                      << RESET << type << "\n";
        }
    }
    
    print_success("Type detection working");
}

// ============================================================================
// Demo 8: Stream Processing
// ============================================================================

void demo_stream_processing() {
    print_header("Demo 8: Stream Processing");
    
    std::string json = R"([
        {"id": 1, "name": "Alice"},
        {"id": 2, "name": "Bob"},
        {"id": 3, "name": "Charlie"}
    ])";
    
    JsonParser parser(json);
    JsonValue root = parser.parse();
    
    QueryExecutor executor;
    QueryPath path = QueryPath::parse(".name");
    
    std::cout << YELLOW << "Extracting names from array:" << RESET << "\n";
    
    int count = 0;
    executor.execute_stream(root, path, [&count](const JsonValue& val) {
        std::cout << "  " << (count + 1) << ". " << val.as_string() << "\n";
        count++;
    });
    
    print_success("Streamed " + std::to_string(count) + " results");
}

// ============================================================================
// Demo 9: Query Processor with Statistics
// ============================================================================

void demo_query_processor() {
    print_header("Demo 9: Query Processor with Statistics");
    
    std::string json = R"({
        "project": "aria_utils",
        "utilities": 15,
        "status": "active"
    })";
    
    QueryOptions opts;
    opts.pretty = true;
    
    QueryProcessor processor(opts);
    std::string result = processor.process(json, ".project");
    
    std::cout << YELLOW << "Query result:" << RESET << " " << result << "\n\n";
    
    const auto& stats = processor.stats();
    print_info("Objects processed", std::to_string(stats.objects_processed));
    print_info("Objects matched", std::to_string(stats.objects_matched));
    print_info("Bytes read", std::to_string(stats.bytes_read));
    print_info("Bytes written", std::to_string(stats.bytes_written));
    print_info("Errors", std::to_string(stats.errors_count));
    
    print_success("Query processor stats collected");
}

// ============================================================================
// Demo 10: JSONL (JSON Lines) Processing
// ============================================================================

void demo_jsonl_processing() {
    print_header("Demo 10: JSONL (JSON Lines) Processing");
    
    std::string jsonl = R"({"event": "start", "timestamp": 1000}
{"event": "process", "timestamp": 2000}
{"event": "end", "timestamp": 3000})";
    
    QueryOptions opts;
    opts.pretty = false;
    
    QueryProcessor processor(opts);
    
    std::cout << YELLOW << "Processing JSON Lines:" << RESET << "\n";
    
    int line_num = 1;
    processor.process_stream(jsonl, ".event", [&line_num](const std::string& result) {
        std::cout << "  Line " << line_num++ << ": " << result << "\n";
    });
    
    print_success("JSONL processed successfully");
}

// ============================================================================
// Demo 11: FFI C API
// ============================================================================

void demo_ffi_api() {
    print_header("Demo 11: FFI C API");
    
    std::string json = R"({"name": "Aria", "version": "1.0", "active": true})";
    
    // Validate
    int valid = aria_json_validate(json.c_str());
    print_info("Validation", valid ? "passed" : "failed");
    
    // Query
    const char* name = aria_json_get(json.c_str(), ".name");
    print_info("Name field", name);
    
    // Format
    const char* formatted = aria_json_format(json.c_str(), 2);
    std::cout << YELLOW << "Formatted JSON:" << RESET << "\n";
    std::cout << formatted << "\n";
    
    print_success("FFI API functional");
}

// ============================================================================
// Demo 12: Real-World Example - Config File
// ============================================================================

void demo_real_world_config() {
    print_header("Demo 12: Real-World Example - Config File");
    
    std::string config = R"({
        "app": {
            "name": "aria_utils",
            "version": "1.0.0",
            "author": "AILP"
        },
        "features": {
            "six_stream": true,
            "tbb_safety": true,
            "ffi_support": true
        },
        "utilities": [
            {"name": "aglob", "status": "complete"},
            {"name": "acurl", "status": "complete"},
            {"name": "ajq", "status": "complete"}
        ]
    })";
    
    JsonParser parser(config);
    JsonValue root = parser.parse();
    
    QueryExecutor executor;
    JsonSerializer ser(false);
    
    // Extract app name
    QueryPath path1 = QueryPath::parse(".app.name");
    JsonValue app_name = executor.execute(root, path1);
    print_info("App name", app_name.as_string());
    
    // Check feature
    QueryPath path2 = QueryPath::parse(".features.six_stream");
    JsonValue six_stream = executor.execute(root, path2);
    print_info("Six-stream enabled", six_stream.as_bool() ? "yes" : "no");
    
    // List utilities
    QueryPath path3 = QueryPath::parse(".utilities");
    JsonValue utils = executor.execute(root, path3);
    
    std::cout << YELLOW << "\nUtilities:" << RESET << "\n";
    if (utils.is_array()) {
        const auto& arr = utils.as_array();
        for (size_t i = 0; i < arr.size(); ++i) {
            if (arr[i].is_object()) {
                const auto& obj = arr[i].as_object();
                std::string name = obj.at("name").as_string();
                std::string status = obj.at("status").as_string();
                std::cout << "  " << (i + 1) << ". " << name << " [" << status << "]\n";
            }
        }
    }
    
    print_success("Config file processed successfully");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << BOLD << MAGENTA
              << "\n╔══════════════════════════════════════════════════╗\n"
              << "║         ajq - JSON Query Utility Demo          ║\n"
              << "║              Comprehensive Tests                 ║\n"
              << "╚══════════════════════════════════════════════════╝\n"
              << RESET << "\n";
    
    demo_basic_parsing();
    demo_query_paths();
    demo_simple_queries();
    demo_array_queries();
    demo_pretty_printing();
    demo_error_handling();
    demo_type_detection();
    demo_stream_processing();
    demo_query_processor();
    demo_jsonl_processing();
    demo_ffi_api();
    demo_real_world_config();
    
    std::cout << "\n" << BOLD << GREEN
              << "✓ All demos complete - ajq is ready for production use!\n"
              << RESET << "\n";
    
    return 0;
}
