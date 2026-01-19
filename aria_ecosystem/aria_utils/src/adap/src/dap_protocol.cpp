/**
 * dap_protocol.cpp
 * Implementation of the DAP protocol library
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "adap/dap_protocol.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace aria::adap {

// =============================================================================
// JSON Value Implementation
// =============================================================================

static const JsonValue null_value;

JsonValue& JsonValue::operator[](const std::string& key) {
    if (!is_object()) {
        data_ = JsonObject{};
    }
    return std::get<JsonObject>(data_)[key];
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
    if (!is_object()) return null_value;
    auto& obj = std::get<JsonObject>(data_);
    auto it = obj.find(key);
    if (it == obj.end()) return null_value;
    return it->second;
}

bool JsonValue::has(const std::string& key) const {
    if (!is_object()) return false;
    return std::get<JsonObject>(data_).count(key) > 0;
}

JsonValue& JsonValue::operator[](size_t index) {
    if (!is_array()) {
        data_ = JsonArray{};
    }
    auto& arr = std::get<JsonArray>(data_);
    if (index >= arr.size()) {
        arr.resize(index + 1);
    }
    return arr[index];
}

const JsonValue& JsonValue::operator[](size_t index) const {
    if (!is_array()) return null_value;
    auto& arr = std::get<JsonArray>(data_);
    if (index >= arr.size()) return null_value;
    return arr[index];
}

size_t JsonValue::size() const {
    if (is_array()) return std::get<JsonArray>(data_).size();
    if (is_object()) return std::get<JsonObject>(data_).size();
    return 0;
}

template<>
bool JsonValue::get<bool>(const std::string& key, const bool& default_val) const {
    if (!has(key)) return default_val;
    const auto& v = (*this)[key];
    return v.is_bool() ? v.as_bool() : default_val;
}

template<>
int64_t JsonValue::get<int64_t>(const std::string& key, const int64_t& default_val) const {
    if (!has(key)) return default_val;
    const auto& v = (*this)[key];
    return v.is_int() ? v.as_int() : default_val;
}

template<>
std::string JsonValue::get<std::string>(const std::string& key, const std::string& default_val) const {
    if (!has(key)) return default_val;
    const auto& v = (*this)[key];
    return v.is_string() ? v.as_string() : default_val;
}

static std::string escape_json_string(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)(unsigned char)c;
                } else {
                    o << c;
                }
        }
    }
    return o.str();
}

std::string JsonValue::to_string() const {
    std::ostringstream oss;

    if (is_null()) {
        oss << "null";
    } else if (is_bool()) {
        oss << (as_bool() ? "true" : "false");
    } else if (is_int()) {
        oss << as_int();
    } else if (is_double()) {
        oss << std::setprecision(17) << as_double();
    } else if (is_string()) {
        oss << '"' << escape_json_string(as_string()) << '"';
    } else if (is_array()) {
        oss << '[';
        const auto& arr = as_array();
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) oss << ',';
            oss << arr[i].to_string();
        }
        oss << ']';
    } else if (is_object()) {
        oss << '{';
        const auto& obj = as_object();
        bool first = true;
        for (const auto& [k, v] : obj) {
            if (!first) oss << ',';
            first = false;
            oss << '"' << escape_json_string(k) << "\":" << v.to_string();
        }
        oss << '}';
    }

    return oss.str();
}

// Simple JSON parser
class JsonParser {
public:
    explicit JsonParser(const std::string& json) : json_(json), pos_(0) {}

    std::optional<JsonValue> parse() {
        skip_whitespace();
        auto val = parse_value();
        skip_whitespace();
        if (pos_ != json_.size()) return std::nullopt;  // Trailing garbage
        return val;
    }

private:
    const std::string& json_;
    size_t pos_;

    char peek() const { return pos_ < json_.size() ? json_[pos_] : '\0'; }
    char advance() { return pos_ < json_.size() ? json_[pos_++] : '\0'; }

    void skip_whitespace() {
        while (pos_ < json_.size() && std::isspace(static_cast<unsigned char>(json_[pos_]))) {
            pos_++;
        }
    }

    std::optional<JsonValue> parse_value() {
        skip_whitespace();
        char c = peek();

        if (c == '"') return parse_string();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') return parse_null();
        if (c == '-' || std::isdigit(c)) return parse_number();

        return std::nullopt;
    }

    std::optional<JsonValue> parse_string() {
        if (advance() != '"') return std::nullopt;

        std::string result;
        while (true) {
            char c = advance();
            if (c == '\0') return std::nullopt;
            if (c == '"') break;
            if (c == '\\') {
                c = advance();
                switch (c) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        // Parse \uXXXX
                        if (pos_ + 4 > json_.size()) return std::nullopt;
                        std::string hex = json_.substr(pos_, 4);
                        pos_ += 4;
                        try {
                            int codepoint = std::stoi(hex, nullptr, 16);
                            if (codepoint < 0x80) {
                                result += static_cast<char>(codepoint);
                            } else if (codepoint < 0x800) {
                                result += static_cast<char>(0xC0 | (codepoint >> 6));
                                result += static_cast<char>(0x80 | (codepoint & 0x3F));
                            } else {
                                result += static_cast<char>(0xE0 | (codepoint >> 12));
                                result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                result += static_cast<char>(0x80 | (codepoint & 0x3F));
                            }
                        } catch (...) {
                            return std::nullopt;
                        }
                        break;
                    }
                    default: return std::nullopt;
                }
            } else {
                result += c;
            }
        }
        return JsonValue(result);
    }

    std::optional<JsonValue> parse_object() {
        if (advance() != '{') return std::nullopt;
        skip_whitespace();

        JsonObject obj;
        if (peek() == '}') {
            advance();
            return JsonValue(obj);
        }

        while (true) {
            skip_whitespace();
            auto key = parse_string();
            if (!key || !key->is_string()) return std::nullopt;

            skip_whitespace();
            if (advance() != ':') return std::nullopt;

            auto value = parse_value();
            if (!value) return std::nullopt;

            obj[key->as_string()] = std::move(*value);

            skip_whitespace();
            char c = advance();
            if (c == '}') break;
            if (c != ',') return std::nullopt;
        }

        return JsonValue(obj);
    }

    std::optional<JsonValue> parse_array() {
        if (advance() != '[') return std::nullopt;
        skip_whitespace();

        JsonArray arr;
        if (peek() == ']') {
            advance();
            return JsonValue(arr);
        }

        while (true) {
            auto value = parse_value();
            if (!value) return std::nullopt;
            arr.push_back(std::move(*value));

            skip_whitespace();
            char c = advance();
            if (c == ']') break;
            if (c != ',') return std::nullopt;
        }

        return JsonValue(arr);
    }

    std::optional<JsonValue> parse_bool() {
        if (json_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            return JsonValue(true);
        }
        if (json_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            return JsonValue(false);
        }
        return std::nullopt;
    }

    std::optional<JsonValue> parse_null() {
        if (json_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
            return JsonValue(nullptr);
        }
        return std::nullopt;
    }

    std::optional<JsonValue> parse_number() {
        size_t start = pos_;
        bool is_float = false;

        if (peek() == '-') advance();

        if (peek() == '0') {
            advance();
        } else if (std::isdigit(peek())) {
            while (std::isdigit(peek())) advance();
        } else {
            return std::nullopt;
        }

        if (peek() == '.') {
            is_float = true;
            advance();
            if (!std::isdigit(peek())) return std::nullopt;
            while (std::isdigit(peek())) advance();
        }

        if (peek() == 'e' || peek() == 'E') {
            is_float = true;
            advance();
            if (peek() == '+' || peek() == '-') advance();
            if (!std::isdigit(peek())) return std::nullopt;
            while (std::isdigit(peek())) advance();
        }

        std::string num_str = json_.substr(start, pos_ - start);
        try {
            if (is_float) {
                return JsonValue(std::stod(num_str));
            } else {
                return JsonValue(static_cast<int64_t>(std::stoll(num_str)));
            }
        } catch (...) {
            return std::nullopt;
        }
    }
};

std::optional<JsonValue> JsonValue::parse(const std::string& json) {
    return JsonParser(json).parse();
}

// =============================================================================
// DAP Type Serialization
// =============================================================================

JsonObject Capabilities::to_json() const {
    JsonObject obj;
    obj["supportsConfigurationDoneRequest"] = supportsConfigurationDoneRequest;
    obj["supportsFunctionBreakpoints"] = supportsFunctionBreakpoints;
    obj["supportsConditionalBreakpoints"] = supportsConditionalBreakpoints;
    obj["supportsHitConditionalBreakpoints"] = supportsHitConditionalBreakpoints;
    obj["supportsEvaluateForHovers"] = supportsEvaluateForHovers;
    obj["supportsStepBack"] = supportsStepBack;
    obj["supportsSetVariable"] = supportsSetVariable;
    obj["supportsRestartFrame"] = supportsRestartFrame;
    obj["supportsGotoTargetsRequest"] = supportsGotoTargetsRequest;
    obj["supportsStepInTargetsRequest"] = supportsStepInTargetsRequest;
    obj["supportsCompletionsRequest"] = supportsCompletionsRequest;
    obj["supportsModulesRequest"] = supportsModulesRequest;
    obj["supportsExceptionOptions"] = supportsExceptionOptions;
    obj["supportsValueFormattingOptions"] = supportsValueFormattingOptions;
    obj["supportsExceptionInfoRequest"] = supportsExceptionInfoRequest;
    obj["supportTerminateDebuggee"] = supportTerminateDebuggee;
    obj["supportsDelayedStackTraceLoading"] = supportsDelayedStackTraceLoading;
    obj["supportsLoadedSourcesRequest"] = supportsLoadedSourcesRequest;
    obj["supportsLogPoints"] = supportsLogPoints;
    obj["supportsTerminateThreadsRequest"] = supportsTerminateThreadsRequest;
    obj["supportsSetExpression"] = supportsSetExpression;
    obj["supportsTerminateRequest"] = supportsTerminateRequest;
    obj["supportsDataBreakpoints"] = supportsDataBreakpoints;
    obj["supportsReadMemoryRequest"] = supportsReadMemoryRequest;
    obj["supportsDisassembleRequest"] = supportsDisassembleRequest;
    obj["supportsCancelRequest"] = supportsCancelRequest;
    obj["supportsBreakpointLocationsRequest"] = supportsBreakpointLocationsRequest;
    return obj;
}

JsonObject Source::to_json() const {
    JsonObject obj;
    if (!name.empty()) obj["name"] = name;
    if (!path.empty()) obj["path"] = path;
    if (sourceReference > 0) obj["sourceReference"] = sourceReference;
    return obj;
}

Source Source::from_json(const JsonObject& obj) {
    Source src;
    auto it = obj.find("name");
    if (it != obj.end() && it->second.is_string()) {
        src.name = it->second.as_string();
    }
    it = obj.find("path");
    if (it != obj.end() && it->second.is_string()) {
        src.path = it->second.as_string();
    }
    it = obj.find("sourceReference");
    if (it != obj.end() && it->second.is_int()) {
        src.sourceReference = static_cast<int>(it->second.as_int());
    }
    return src;
}

JsonObject Breakpoint::to_json() const {
    JsonObject obj;
    obj["id"] = id;
    obj["verified"] = verified;
    if (!message.empty()) obj["message"] = message;
    if (!source.path.empty()) obj["source"] = JsonValue(source.to_json());
    if (line > 0) obj["line"] = line;
    if (column > 0) obj["column"] = column;
    if (endLine > 0) obj["endLine"] = endLine;
    if (endColumn > 0) obj["endColumn"] = endColumn;
    return obj;
}

JsonObject StackFrame::to_json() const {
    JsonObject obj;
    obj["id"] = id;
    obj["name"] = name;
    if (!source.path.empty()) obj["source"] = JsonValue(source.to_json());
    obj["line"] = line;
    obj["column"] = column;
    if (endLine > 0) obj["endLine"] = endLine;
    if (endColumn > 0) obj["endColumn"] = endColumn;
    if (!moduleId.empty()) obj["moduleId"] = moduleId;
    if (!presentationHint.empty()) obj["presentationHint"] = presentationHint;
    return obj;
}

JsonObject Scope::to_json() const {
    JsonObject obj;
    obj["name"] = name;
    if (!presentationHint.empty()) obj["presentationHint"] = presentationHint;
    obj["variablesReference"] = variablesReference;
    if (namedVariables > 0) obj["namedVariables"] = namedVariables;
    if (indexedVariables > 0) obj["indexedVariables"] = indexedVariables;
    obj["expensive"] = expensive;
    return obj;
}

JsonObject Variable::to_json() const {
    JsonObject obj;
    obj["name"] = name;
    obj["value"] = value;
    if (!type.empty()) obj["type"] = type;
    if (!presentationHint.empty()) obj["presentationHint"] = presentationHint;
    if (!evaluateName.empty()) obj["evaluateName"] = evaluateName;
    obj["variablesReference"] = variablesReference;
    if (namedVariables > 0) obj["namedVariables"] = namedVariables;
    if (indexedVariables > 0) obj["indexedVariables"] = indexedVariables;
    return obj;
}

JsonObject Thread::to_json() const {
    JsonObject obj;
    obj["id"] = id;
    obj["name"] = name;
    return obj;
}

// =============================================================================
// Protocol I/O
// =============================================================================

std::optional<std::string> read_dap_message(int fd) {
    // Read headers until empty line
    std::string header_buf;
    int content_length = -1;

    while (true) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) return std::nullopt;

        header_buf += c;

        // Check for end of headers (\r\n\r\n)
        if (header_buf.size() >= 4 &&
            header_buf.substr(header_buf.size() - 4) == "\r\n\r\n") {
            break;
        }
    }

    // Parse Content-Length
    std::string cl_prefix = "Content-Length: ";
    size_t pos = header_buf.find(cl_prefix);
    if (pos == std::string::npos) return std::nullopt;

    size_t end = header_buf.find("\r\n", pos);
    if (end == std::string::npos) return std::nullopt;

    std::string cl_str = header_buf.substr(pos + cl_prefix.size(),
                                           end - pos - cl_prefix.size());
    try {
        content_length = std::stoi(cl_str);
    } catch (...) {
        return std::nullopt;
    }

    if (content_length <= 0) return std::nullopt;

    // Read body
    std::string body;
    body.resize(content_length);

    size_t read_total = 0;
    while (read_total < static_cast<size_t>(content_length)) {
        ssize_t n = read(fd, &body[read_total], content_length - read_total);
        if (n <= 0) return std::nullopt;
        read_total += n;
    }

    return body;
}

void write_dap_message(int fd, const std::string& json) {
    std::ostringstream header;
    header << "Content-Length: " << json.size() << "\r\n\r\n";
    std::string full = header.str() + json;

    size_t written = 0;
    while (written < full.size()) {
        ssize_t n = write(fd, full.c_str() + written, full.size() - written);
        if (n < 0) return;
        written += n;
    }
}

// =============================================================================
// DAPServer Implementation
// =============================================================================

DAPServer::DAPServer(int in_fd, int out_fd)
    : in_fd_(in_fd), out_fd_(out_fd) {
    // Register default handlers
    register_handler("initialize", [this](const Request& req, Response& resp) {
        handle_initialize(req, resp);
    });
    register_handler("disconnect", [this](const Request& req, Response& resp) {
        handle_disconnect(req, resp);
    });
}

DAPServer::~DAPServer() = default;

void DAPServer::register_handler(const std::string& command, RequestHandler handler) {
    handlers_[command] = std::move(handler);
}

void DAPServer::set_capabilities(const Capabilities& caps) {
    capabilities_ = caps;
}

std::optional<Request> DAPServer::read_request() {
    auto json = read_dap_message(in_fd_);
    if (!json) return std::nullopt;

    if (tracing_) {
        trace("<-", *json);
    }

    auto parsed = JsonValue::parse(*json);
    if (!parsed || !parsed->is_object()) return std::nullopt;

    const auto& obj = parsed->as_object();

    Request req;

    auto it = obj.find("seq");
    if (it != obj.end() && it->second.is_int()) {
        req.seq = static_cast<int>(it->second.as_int());
    }

    it = obj.find("type");
    if (it == obj.end() || !it->second.is_string() || it->second.as_string() != "request") {
        return std::nullopt;
    }

    it = obj.find("command");
    if (it != obj.end() && it->second.is_string()) {
        req.command = it->second.as_string();
    }

    it = obj.find("arguments");
    if (it != obj.end() && it->second.is_object()) {
        req.arguments = it->second.as_object();
    }

    return req;
}

void DAPServer::write_message(const std::string& json) {
    if (tracing_) {
        trace("->", json);
    }
    write_dap_message(out_fd_, json);
}

void DAPServer::trace(const std::string& direction, const std::string& json) {
    // Write trace to stddbg (FD 3)
    std::string msg = direction + " " + json + "\n";
    ssize_t written = write(FD_STDDBG, msg.c_str(), msg.size());
    (void)written;
}

void DAPServer::send_event(const std::string& event, const JsonObject& body) {
    JsonObject msg;
    msg["seq"] = next_seq();
    msg["type"] = std::string("event");
    msg["event"] = event;
    if (!body.empty()) {
        msg["body"] = JsonValue(body);
    }
    write_message(JsonValue(msg).to_string());
}

void DAPServer::send_response(int request_seq, const std::string& command,
                              bool success, const JsonObject& body,
                              const std::string& message) {
    JsonObject msg;
    msg["seq"] = next_seq();
    msg["type"] = std::string("response");
    msg["request_seq"] = request_seq;
    msg["command"] = command;
    msg["success"] = success;
    if (!success && !message.empty()) {
        msg["message"] = message;
    }
    if (!body.empty()) {
        msg["body"] = JsonValue(body);
    }
    write_message(JsonValue(msg).to_string());
}

void DAPServer::shutdown() {
    shutdown_ = true;
}

int DAPServer::run() {
    while (!shutdown_) {
        auto req = read_request();
        if (!req) {
            // EOF or error
            break;
        }

        Response resp;
        resp.request_seq = req->seq;
        resp.command = req->command;
        resp.success = true;

        auto it = handlers_.find(req->command);
        if (it != handlers_.end()) {
            try {
                it->second(*req, resp);
            } catch (const std::exception& e) {
                resp.success = false;
                resp.message = e.what();
            }
        } else {
            resp.success = false;
            resp.message = "Unknown command: " + req->command;
        }

        send_response(req->seq, req->command, resp.success, resp.body, resp.message);
    }

    return 0;
}

void DAPServer::handle_initialize(const Request& req, Response& resp) {
    (void)req;
    resp.body = capabilities_.to_json();
    initialized_ = true;

    // Send initialized event after response
    // Note: In actual implementation, this should be sent after the response
    // For simplicity, we'll schedule it
}

void DAPServer::handle_disconnect(const Request& req, Response& resp) {
    (void)req;
    resp.success = true;
    shutdown_ = true;
}

}  // namespace aria::adap
