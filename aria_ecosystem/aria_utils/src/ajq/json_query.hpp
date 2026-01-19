/**
 * @file json_query.hpp
 * @brief JSON Query Utility - Six-stream JSON processing with binary protocol
 * 
 * Implements modern JSON query tool with Aria's architectural patterns:
 * - Six-stream topology (stdout/UI, stddbg/telemetry, stddato/binary)
 * - Binary JSON protocol support
 * - TBB-safe value handling
 * - Zero-copy query execution
 * - Streaming JSON parser
 * 
 * Reference: sysUtils_11_jq.txt (429 lines)
 */

#ifndef ARIA_AJQ_JSON_QUERY_HPP
#define ARIA_AJQ_JSON_QUERY_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <functional>

namespace aria {
namespace ajq {

// ============================================================================
// TBB Type Definitions
// ============================================================================

using tbb8 = int8_t;
using tbb32 = int32_t;
using tbb64 = int64_t;

// TBB Sentinels - sticky error values
constexpr tbb64 TBB64_ERR = INT64_MIN;
constexpr tbb32 TBB32_ERR = INT32_MIN;
constexpr tbb8 TBB8_ERR = INT8_MIN;

// ============================================================================
// JSON Value Types
// ============================================================================

class JsonValue;

using JsonNull = std::nullptr_t;
using JsonBool = bool;
using JsonNumber = double;
using JsonString = std::string;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

// Variant holding all possible JSON types
using JsonVariant = std::variant<
    JsonNull,
    JsonBool,
    JsonNumber,
    JsonString,
    JsonArray,
    JsonObject
>;

class JsonValue {
public:
    JsonValue() : data_(nullptr) {}
    
    // Explicit constructors for each type
    explicit JsonValue(JsonNull) : data_(nullptr) {}
    explicit JsonValue(bool b) : data_(b) {}
    explicit JsonValue(double d) : data_(d) {}
    explicit JsonValue(const std::string& s) : data_(s) {}
    explicit JsonValue(std::string&& s) : data_(std::move(s)) {}
    explicit JsonValue(const JsonArray& arr) : data_(arr) {}
    explicit JsonValue(JsonArray&& arr) : data_(std::move(arr)) {}
    explicit JsonValue(const JsonObject& obj) : data_(obj) {}
    explicit JsonValue(JsonObject&& obj) : data_(std::move(obj)) {}
    
    // Type checking
    bool is_null() const { return std::holds_alternative<JsonNull>(data_); }
    bool is_bool() const { return std::holds_alternative<JsonBool>(data_); }
    bool is_number() const { return std::holds_alternative<JsonNumber>(data_); }
    bool is_string() const { return std::holds_alternative<JsonString>(data_); }
    bool is_array() const { return std::holds_alternative<JsonArray>(data_); }
    bool is_object() const { return std::holds_alternative<JsonObject>(data_); }
    
    // Value extraction
    const JsonBool& as_bool() const { return std::get<JsonBool>(data_); }
    const JsonNumber& as_number() const { return std::get<JsonNumber>(data_); }
    const JsonString& as_string() const { return std::get<JsonString>(data_); }
    const JsonArray& as_array() const { return std::get<JsonArray>(data_); }
    const JsonObject& as_object() const { return std::get<JsonObject>(data_); }
    
    JsonArray& as_array() { return std::get<JsonArray>(data_); }
    JsonObject& as_object() { return std::get<JsonObject>(data_); }
    
    const JsonVariant& data() const { return data_; }
    
private:
    JsonVariant data_;
};

// ============================================================================
// Query Path - Simple dot-notation queries
// ============================================================================

struct QueryPath {
    std::vector<std::string> keys;
    
    static QueryPath parse(const std::string& query);
    std::string to_string() const;
};

// ============================================================================
// JSON Parser
// ============================================================================

class JsonParser {
public:
    explicit JsonParser(const std::string& json);
    
    // Parse JSON string into value
    JsonValue parse();
    
    // Get parse error if any
    bool has_error() const { return !error_msg_.empty(); }
    const std::string& error() const { return error_msg_; }
    
private:
    void skip_whitespace();
    JsonValue parse_value();
    JsonValue parse_object();
    JsonValue parse_array();
    JsonValue parse_string();
    JsonValue parse_number();
    JsonValue parse_literal();
    
    char peek() const;
    char consume();
    bool expect(char c);
    
    std::string json_;
    size_t pos_;
    std::string error_msg_;
};

// ============================================================================
// JSON Serializer
// ============================================================================

class JsonSerializer {
public:
    explicit JsonSerializer(bool pretty = false, int indent = 2);
    
    // Serialize value to JSON string
    std::string serialize(const JsonValue& value);
    
private:
    void serialize_value(const JsonValue& value, std::string& out, int depth);
    void write_indent(std::string& out, int depth);
    
    bool pretty_;
    int indent_;
};

// ============================================================================
// Query Executor
// ============================================================================

class QueryExecutor {
public:
    QueryExecutor() = default;
    
    // Execute query on JSON value
    JsonValue execute(const JsonValue& input, const QueryPath& query);
    
    // Execute with callback for each result (for streaming)
    void execute_stream(
        const JsonValue& input,
        const QueryPath& query,
        std::function<void(const JsonValue&)> callback
    );
    
    // Get last error
    bool has_error() const { return error_ == TBB64_ERR; }
    tbb64 error_code() const { return error_; }
    const std::string& error_message() const { return error_msg_; }
    
private:
    tbb64 error_;
    std::string error_msg_;
    
    void set_error(const std::string& msg);
};

// ============================================================================
// Six-Stream Writers
// ============================================================================

namespace streams {
    // Write telemetry to stddbg (FD 3)
    void write_telemetry(const std::string& op, const std::string& data);
    
    // Write JSON result to stddato (FD 5) - binary protocol
    void write_json_binary(const JsonValue& value);
    
    // Check if stream is active
    bool is_stream_active(int fd);
}

// ============================================================================
// Query Options
// ============================================================================

struct QueryOptions {
    bool pretty;           // Pretty-print output
    bool compact;          // Compact output (no whitespace)
    bool raw_output;       // Raw string output (no quotes)
    bool binary_input;     // Expect binary JSON input
    bool binary_output;    // Output binary JSON
    
    QueryOptions()
        : pretty(false)
        , compact(false)
        , raw_output(false)
        , binary_input(false)
        , binary_output(false)
    {}
};

// ============================================================================
// Query Processor
// ============================================================================

class QueryProcessor {
public:
    explicit QueryProcessor(const QueryOptions& opts = QueryOptions());
    
    // Process JSON with query
    std::string process(const std::string& json, const std::string& query);
    
    // Process streaming JSON (one object per line)
    void process_stream(
        const std::string& input,
        const std::string& query,
        std::function<void(const std::string&)> callback
    );
    
    // Get processing statistics
    struct Stats {
        tbb64 objects_processed;
        tbb64 objects_matched;
        tbb64 bytes_read;
        tbb64 bytes_written;
        tbb64 errors_count;
        
        Stats()
            : objects_processed(0)
            , objects_matched(0)
            , bytes_read(0)
            , bytes_written(0)
            , errors_count(0)
        {}
    };
    
    const Stats& stats() const { return stats_; }
    
private:
    QueryOptions opts_;
    Stats stats_;
};

// ============================================================================
// Utility Functions
// ============================================================================

// Escape JSON string
std::string escape_json_string(const std::string& str);

// Unescape JSON string
std::string unescape_json_string(const std::string& str);

// Check if string is valid JSON
bool is_valid_json(const std::string& json);

// ============================================================================
// FFI C API
// ============================================================================

extern "C" {
    // Query JSON string
    const char* aria_json_query(
        const char* json,
        const char* query,
        int pretty
    );
    
    // Parse and validate JSON
    int aria_json_validate(const char* json);
    
    // Get JSON value at path
    const char* aria_json_get(
        const char* json,
        const char* path
    );
    
    // Format JSON (pretty-print)
    const char* aria_json_format(
        const char* json,
        int indent
    );
    
    // Free result string
    void aria_json_free(const char* str);
}

} // namespace ajq
} // namespace aria

#endif // ARIA_AJQ_JSON_QUERY_HPP
