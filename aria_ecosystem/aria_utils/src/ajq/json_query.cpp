/**
 * @file json_query.cpp
 * @brief JSON Query Utility - Implementation
 * 
 * Implements JSON parser, query executor, and six-stream processing.
 */

#include "json_query.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <iostream>
#include <sstream>
#include <cctype>
#include <cmath>

namespace aria {
namespace ajq {

// ============================================================================
// Query Path Implementation
// ============================================================================

QueryPath QueryPath::parse(const std::string& query) {
    QueryPath path;
    
    if (query.empty() || query == ".") {
        return path; // Root query
    }
    
    // Parse dot-separated path like ".users.name" or ".users[0].name"
    std::string current;
    bool in_bracket = false;
    
    for (size_t i = 0; i < query.length(); ++i) {
        char c = query[i];
        
        if (c == '.' && !in_bracket) {
            if (!current.empty()) {
                path.keys.push_back(current);
                current.clear();
            }
        } else if (c == '[') {
            if (!current.empty()) {
                path.keys.push_back(current);
                current.clear();
            }
            in_bracket = true;
        } else if (c == ']') {
            if (!current.empty()) {
                path.keys.push_back(current);
                current.clear();
            }
            in_bracket = false;
        } else {
            current += c;
        }
    }
    
    if (!current.empty()) {
        path.keys.push_back(current);
    }
    
    return path;
}

std::string QueryPath::to_string() const {
    if (keys.empty()) return ".";
    
    std::string result;
    for (const auto& key : keys) {
        // Check if key is numeric (array index)
        bool is_numeric = !key.empty() && std::all_of(key.begin(), key.end(), ::isdigit);
        
        if (is_numeric) {
            result += "[" + key + "]";
        } else {
            result += "." + key;
        }
    }
    return result;
}

// ============================================================================
// JSON Parser Implementation
// ============================================================================

JsonParser::JsonParser(const std::string& json)
    : json_(json), pos_(0)
{}

char JsonParser::peek() const {
    return pos_ < json_.length() ? json_[pos_] : '\0';
}

char JsonParser::consume() {
    return pos_ < json_.length() ? json_[pos_++] : '\0';
}

bool JsonParser::expect(char c) {
    skip_whitespace();
    if (peek() == c) {
        consume();
        return true;
    }
    return false;
}

void JsonParser::skip_whitespace() {
    while (pos_ < json_.length() && std::isspace(json_[pos_])) {
        pos_++;
    }
}

JsonValue JsonParser::parse() {
    skip_whitespace();
    
    if (pos_ >= json_.length()) {
        error_msg_ = "Unexpected end of input";
        return JsonValue(nullptr);
    }
    
    return parse_value();
}

JsonValue JsonParser::parse_value() {
    skip_whitespace();
    char c = peek();
    
    if (c == '{') return parse_object();
    if (c == '[') return parse_array();
    if (c == '"') return parse_string();
    if (c == 't' || c == 'f' || c == 'n') return parse_literal();
    if (c == '-' || std::isdigit(c)) return parse_number();
    
    error_msg_ = std::string("Unexpected character: ") + c;
    return JsonValue(nullptr);
}

JsonValue JsonParser::parse_object() {
    if (!expect('{')) {
        error_msg_ = "Expected '{'";
        return JsonValue(nullptr);
    }
    
    JsonObject obj;
    skip_whitespace();
    
    if (peek() == '}') {
        consume();
        return JsonValue(std::move(obj));
    }
    
    while (true) {
        skip_whitespace();
        
        // Parse key
        if (peek() != '"') {
            error_msg_ = "Expected string key";
            return JsonValue(nullptr);
        }
        
        JsonValue key_val = parse_string();
        if (!key_val.is_string()) {
            return JsonValue(nullptr);
        }
        std::string key = key_val.as_string();
        
        // Expect colon
        skip_whitespace();
        if (!expect(':')) {
            error_msg_ = "Expected ':'";
            return JsonValue(nullptr);
        }
        
        // Parse value
        JsonValue value = parse_value();
        if (has_error()) {
            return JsonValue(nullptr);
        }
        
        obj[key] = std::move(value);
        
        skip_whitespace();
        if (peek() == ',') {
            consume();
            continue;
        }
        if (peek() == '}') {
            consume();
            break;
        }
        
        error_msg_ = "Expected ',' or '}'";
        return JsonValue(nullptr);
    }
    
    return JsonValue(std::move(obj));
}

JsonValue JsonParser::parse_array() {
    if (!expect('[')) {
        error_msg_ = "Expected '['";
        return JsonValue(nullptr);
    }
    
    JsonArray arr;
    skip_whitespace();
    
    if (peek() == ']') {
        consume();
        return JsonValue(std::move(arr));
    }
    
    while (true) {
        JsonValue value = parse_value();
        if (has_error()) {
            return JsonValue(nullptr);
        }
        
        arr.push_back(std::move(value));
        
        skip_whitespace();
        if (peek() == ',') {
            consume();
            continue;
        }
        if (peek() == ']') {
            consume();
            break;
        }
        
        error_msg_ = "Expected ',' or ']'";
        return JsonValue(nullptr);
    }
    
    return JsonValue(std::move(arr));
}

JsonValue JsonParser::parse_string() {
    if (!expect('"')) {
        error_msg_ = "Expected '\"'";
        return JsonValue(nullptr);
    }
    
    std::string str;
    while (true) {
        char c = peek();
        
        if (c == '\0') {
            error_msg_ = "Unterminated string";
            return JsonValue(nullptr);
        }
        
        if (c == '"') {
            consume();
            break;
        }
        
        if (c == '\\') {
            consume();
            char next = consume();
            switch (next) {
                case '"': str += '"'; break;
                case '\\': str += '\\'; break;
                case '/': str += '/'; break;
                case 'b': str += '\b'; break;
                case 'f': str += '\f'; break;
                case 'n': str += '\n'; break;
                case 'r': str += '\r'; break;
                case 't': str += '\t'; break;
                default: str += next;
            }
        } else {
            str += consume();
        }
    }
    
    return JsonValue(std::move(str));
}

JsonValue JsonParser::parse_number() {
    size_t start = pos_;
    
    if (peek() == '-') consume();
    
    if (peek() == '0') {
        consume();
    } else if (std::isdigit(peek())) {
        while (std::isdigit(peek())) consume();
    } else {
        error_msg_ = "Invalid number";
        return JsonValue(nullptr);
    }
    
    // Fractional part
    if (peek() == '.') {
        consume();
        if (!std::isdigit(peek())) {
            error_msg_ = "Invalid number";
            return JsonValue(nullptr);
        }
        while (std::isdigit(peek())) consume();
    }
    
    // Exponent
    if (peek() == 'e' || peek() == 'E') {
        consume();
        if (peek() == '+' || peek() == '-') consume();
        if (!std::isdigit(peek())) {
            error_msg_ = "Invalid number";
            return JsonValue(nullptr);
        }
        while (std::isdigit(peek())) consume();
    }
    
    std::string num_str = json_.substr(start, pos_ - start);
    double value = std::stod(num_str);
    
    return JsonValue(value);
}

JsonValue JsonParser::parse_literal() {
    if (json_.substr(pos_, 4) == "true") {
        pos_ += 4;
        return JsonValue(true);
    }
    if (json_.substr(pos_, 5) == "false") {
        pos_ += 5;
        return JsonValue(false);
    }
    if (json_.substr(pos_, 4) == "null") {
        pos_ += 4;
        return JsonValue(nullptr);
    }
    
    error_msg_ = "Invalid literal";
    return JsonValue(nullptr);
}

// ============================================================================
// JSON Serializer Implementation
// ============================================================================

JsonSerializer::JsonSerializer(bool pretty, int indent)
    : pretty_(pretty), indent_(indent)
{}

void JsonSerializer::write_indent(std::string& out, int depth) {
    if (pretty_) {
        out += '\n';
        for (int i = 0; i < depth * indent_; ++i) {
            out += ' ';
        }
    }
}

std::string JsonSerializer::serialize(const JsonValue& value) {
    std::string result;
    serialize_value(value, result, 0);
    return result;
}

void JsonSerializer::serialize_value(const JsonValue& value, std::string& out, int depth) {
    if (value.is_null()) {
        out += "null";
    } else if (value.is_bool()) {
        out += value.as_bool() ? "true" : "false";
    } else if (value.is_number()) {
        double num = value.as_number();
        if (std::floor(num) == num && num >= INT64_MIN && num <= INT64_MAX) {
            out += std::to_string(static_cast<int64_t>(num));
        } else {
            out += std::to_string(num);
        }
    } else if (value.is_string()) {
        out += '"';
        out += escape_json_string(value.as_string());
        out += '"';
    } else if (value.is_array()) {
        const auto& arr = value.as_array();
        out += '[';
        
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) out += ',';
            if (pretty_) write_indent(out, depth + 1);
            serialize_value(arr[i], out, depth + 1);
        }
        
        if (pretty_ && !arr.empty()) write_indent(out, depth);
        out += ']';
    } else if (value.is_object()) {
        const auto& obj = value.as_object();
        out += '{';
        
        size_t i = 0;
        for (const auto& [key, val] : obj) {
            if (i > 0) out += ',';
            write_indent(out, depth + 1);
            out += '"';
            out += escape_json_string(key);
            out += pretty_ ? "\": " : "\":";
            serialize_value(val, out, depth + 1);
            ++i;
        }
        
        if (pretty_ && !obj.empty()) write_indent(out, depth);
        out += '}';
    }
}

// ============================================================================
// Query Executor Implementation
// ============================================================================

void QueryExecutor::set_error(const std::string& msg) {
    error_ = TBB64_ERR;
    error_msg_ = msg;
}

JsonValue QueryExecutor::execute(const JsonValue& input, const QueryPath& query) {
    error_ = 0;
    error_msg_.clear();
    
    // Root query - return whole input
    if (query.keys.empty()) {
        return input;
    }
    
    // Navigate through the path
    const JsonValue* current = &input;
    
    for (const auto& key : query.keys) {
        // Check if key is numeric (array index)
        bool is_numeric = !key.empty() && std::all_of(key.begin(), key.end(), ::isdigit);
        
        if (is_numeric) {
            // Array access
            if (!current->is_array()) {
                set_error("Cannot index non-array with numeric key");
                return JsonValue(nullptr);
            }
            
            size_t index = std::stoull(key);
            const auto& arr = current->as_array();
            
            if (index >= arr.size()) {
                set_error("Array index out of bounds");
                return JsonValue(nullptr);
            }
            
            current = &arr[index];
        } else {
            // Object access
            if (!current->is_object()) {
                set_error("Cannot access property of non-object");
                return JsonValue(nullptr);
            }
            
            const auto& obj = current->as_object();
            auto it = obj.find(key);
            
            if (it == obj.end()) {
                // Key not found - return null (jq behavior)
                return JsonValue(nullptr);
            }
            
            current = &(it->second);
        }
    }
    
    return *current;
}

void QueryExecutor::execute_stream(
    const JsonValue& input,
    const QueryPath& query,
    std::function<void(const JsonValue&)> callback)
{
    // If input is array and query targets elements, stream each
    if (input.is_array() && !query.keys.empty()) {
        const auto& arr = input.as_array();
        for (const auto& element : arr) {
            JsonValue result = execute(element, query);
            if (!has_error()) {
                callback(result);
            }
        }
    } else {
        JsonValue result = execute(input, query);
        if (!has_error()) {
            callback(result);
        }
    }
}

// ============================================================================
// Six-Stream Writers
// ============================================================================

namespace streams {

void write_telemetry(const std::string& op, const std::string& data) {
    static const int STDDBG_FD = 3;
    
    static bool checked = false;
    static bool active = false;
    
    if (!checked) {
        struct stat st;
        if (fstat(STDDBG_FD, &st) == 0) {
            active = !S_ISCHR(st.st_mode) || st.st_rdev != makedev(1, 3);
        }
        checked = true;
    }
    
    if (!active) return;
    
    std::ostringstream oss;
    oss << "{\"op\":\"" << op << "\",\"data\":" << data << "}\n";
    std::string msg = oss.str();
    
    write(STDDBG_FD, msg.c_str(), msg.length());
}

void write_json_binary(const JsonValue& value) {
    static const int STDDATO_FD = 5;
    
    struct stat st;
    if (fstat(STDDATO_FD, &st) != 0) return;
    
    // Simple binary protocol: [magic:4][length:8][json_text]
    uint32_t magic = 0x4A534F4E; // "JSON"
    
    JsonSerializer ser(false);
    std::string json = ser.serialize(value);
    uint64_t length = json.length();
    
    write(STDDATO_FD, &magic, sizeof(magic));
    write(STDDATO_FD, &length, sizeof(length));
    write(STDDATO_FD, json.c_str(), json.length());
}

bool is_stream_active(int fd) {
    struct stat st;
    return fstat(fd, &st) == 0;
}

} // namespace streams

// ============================================================================
// Query Processor Implementation
// ============================================================================

QueryProcessor::QueryProcessor(const QueryOptions& opts)
    : opts_(opts)
{}

std::string QueryProcessor::process(const std::string& json, const std::string& query) {
    stats_.bytes_read += json.length();
    
    // Parse JSON
    JsonParser parser(json);
    JsonValue value = parser.parse();
    
    if (parser.has_error()) {
        stats_.errors_count++;
        streams::write_telemetry("parse_error", "{\"error\":\"" + parser.error() + "\"}");
        return "";
    }
    
    stats_.objects_processed++;
    
    // Execute query
    QueryPath path = QueryPath::parse(query);
    QueryExecutor executor;
    JsonValue result = executor.execute(value, path);
    
    if (executor.has_error()) {
        stats_.errors_count++;
        streams::write_telemetry("query_error", "{\"error\":\"" + executor.error_message() + "\"}");
        return "";
    }
    
    stats_.objects_matched++;
    
    // Serialize result
    JsonSerializer ser(opts_.pretty, 2);
    std::string output = ser.serialize(result);
    
    stats_.bytes_written += output.length();
    
    // Write to stddato if binary output enabled
    if (opts_.binary_output) {
        streams::write_json_binary(result);
    }
    
    return output;
}

void QueryProcessor::process_stream(
    const std::string& input,
    const std::string& query,
    std::function<void(const std::string&)> callback)
{
    // Process line by line (JSONL format)
    std::istringstream iss(input);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        std::string result = process(line, query);
        if (!result.empty()) {
            callback(result);
        }
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string escape_json_string(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

std::string unescape_json_string(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case '"': result += '"'; ++i; break;
                case '\\': result += '\\'; ++i; break;
                case 'b': result += '\b'; ++i; break;
                case 'f': result += '\f'; ++i; break;
                case 'n': result += '\n'; ++i; break;
                case 'r': result += '\r'; ++i; break;
                case 't': result += '\t'; ++i; break;
                default: result += str[i];
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

bool is_valid_json(const std::string& json) {
    JsonParser parser(json);
    parser.parse();
    return !parser.has_error();
}

// ============================================================================
// FFI C API
// ============================================================================

static std::string g_result_buffer;

extern "C" {

const char* aria_json_query(const char* json, const char* query, int pretty) {
    QueryOptions opts;
    opts.pretty = (pretty != 0);
    
    QueryProcessor processor(opts);
    g_result_buffer = processor.process(json ? json : "", query ? query : ".");
    
    return g_result_buffer.c_str();
}

int aria_json_validate(const char* json) {
    return is_valid_json(json ? json : "") ? 1 : 0;
}

const char* aria_json_get(const char* json, const char* path) {
    return aria_json_query(json, path, 0);
}

const char* aria_json_format(const char* json, int indent) {
    if (!json) return "";
    
    JsonParser parser(json);
    JsonValue value = parser.parse();
    
    if (parser.has_error()) {
        return "";
    }
    
    JsonSerializer ser(true, indent);
    g_result_buffer = ser.serialize(value);
    
    return g_result_buffer.c_str();
}

void aria_json_free(const char* str) {
    // In this simple implementation, we use a static buffer
    // In production, would use proper memory management
}

} // extern "C"

} // namespace ajq
} // namespace aria
