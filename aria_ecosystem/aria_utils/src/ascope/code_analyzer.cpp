/**
 * @file code_analyzer.cpp
 * @brief Code Scope Analysis - Implementation
 */

#include "code_analyzer.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>

namespace aria {
namespace ascope {

// ============================================================================
// Utility Functions
// ============================================================================

std::string trim(const std::string& str) {
    size_t start = 0;
    size_t end = str.length();
    
    while (start < end && std::isspace(str[start])) start++;
    while (end > start && std::isspace(str[end - 1])) end--;
    
    return str.substr(start, end - start);
}

bool is_blank_line(const std::string& line) {
    return trim(line).empty();
}

bool is_comment_line(const std::string& line, const std::string& lang) {
    std::string trimmed = trim(line);
    
    if (lang == "cpp" || lang == "c" || lang == "java") {
        return trimmed.substr(0, 2) == "//";
    } else if (lang == "python" || lang == "ruby" || lang == "shell") {
        return !trimmed.empty() && trimmed[0] == '#';
    }
    
    return false;
}

std::string detect_language(const std::string& filepath) {
    size_t dot = filepath.rfind('.');
    if (dot == std::string::npos) return "unknown";
    
    std::string ext = filepath.substr(dot + 1);
    
    if (ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "hpp" || ext == "h") return "cpp";
    if (ext == "c") return "c";
    if (ext == "py") return "python";
    if (ext == "js" || ext == "ts") return "javascript";
    if (ext == "java") return "java";
    if (ext == "rb") return "ruby";
    if (ext == "sh" || ext == "bash") return "shell";
    if (ext == "aria") return "aria";
    
    return "unknown";
}

bool is_analyzable(const std::string& filepath) {
    std::string lang = detect_language(filepath);
    return lang != "unknown";
}

// ============================================================================
// CodeAnalyzer Implementation
// ============================================================================

CodeAnalyzer::CodeAnalyzer(const AnalysisOptions& opts)
    : opts_(opts)
    , error_code_(0)
{}

void CodeAnalyzer::set_error(const std::string& msg) {
    error_code_ = TBB64_ERR;
    error_msg_ = msg;
    stats_.errors_count++;
}

LineType CodeAnalyzer::classify_line(const std::string& line, bool& in_block_comment) {
    std::string trimmed = trim(line);
    
    if (trimmed.empty()) {
        return LineType::Blank;
    }
    
    // Check for block comment start/end (C-style /* */)
    if (trimmed.find("/*") != std::string::npos) {
        in_block_comment = true;
    }
    if (trimmed.find("*/") != std::string::npos) {
        in_block_comment = false;
        return LineType::Comment;
    }
    
    if (in_block_comment) {
        return LineType::Comment;
    }
    
    // Check for single-line comments
    if (trimmed.substr(0, 2) == "//" || trimmed.substr(0, 1) == "#") {
        return LineType::Comment;
    }
    
    // Check for mixed (code + comment on same line)
    size_t comment_pos = trimmed.find("//");
    if (comment_pos != std::string::npos && comment_pos > 0) {
        return LineType::Mixed;
    }
    
    return LineType::Code;
}

bool CodeAnalyzer::is_function_definition(const std::string& line) {
    std::string trimmed = trim(line);
    
    // Skip common non-function patterns
    if (trimmed.empty()) return false;
    if (trimmed[0] == '#') return false;  // Preprocessor
    if (trimmed.substr(0, 2) == "//") return false;  // Comment
    if (trimmed.find("class ") != std::string::npos) return false;
    if (trimmed.find("struct ") != std::string::npos) return false;
    
    // Look for function pattern: type name(params) or name(params)
    size_t paren = trimmed.find('(');
    if (paren == std::string::npos) return false;
    
    // Check for opening brace (not just declaration)
    size_t brace = trimmed.find('{');
    
    // Function definitions often have '{' on same line or will on next line
    // For simple detection, look for '(' and not ending in ';' (which would be declaration)
    size_t semicolon = trimmed.find(';');
    
    return paren != std::string::npos && (brace != std::string::npos || semicolon == std::string::npos);
}

tbb32 CodeAnalyzer::count_decision_points(const std::string& line) {
    tbb32 count = 0;
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Count control flow keywords
    std::vector<std::string> keywords = {
        "if", "else if", "for", "while", "case", "catch", "&&", "||", "?"
    };
    
    for (const auto& kw : keywords) {
        size_t pos = 0;
        while ((pos = lower.find(kw, pos)) != std::string::npos) {
            count++;
            pos += kw.length();
        }
    }
    
    return count;
}

tbb32 CodeAnalyzer::calculate_complexity(const std::vector<std::string>& function_lines) {
    tbb32 complexity = 1;  // Base complexity
    
    for (const auto& line : function_lines) {
        complexity += count_decision_points(line);
    }
    
    return complexity;
}

tbb32 CodeAnalyzer::count_scope_depth(const std::string& line, tbb32 current_depth) {
    tbb32 depth = current_depth;
    
    for (char c : line) {
        if (c == '{') depth++;
        if (c == '}' && depth > 0) depth--;
    }
    
    return depth;
}

void CodeAnalyzer::track_scope_changes(
    const std::string& line,
    tbb32 line_num,
    std::vector<ScopeMetrics>& scopes,
    tbb32& current_depth)
{
    std::string trimmed = trim(line);
    
    // Opening scope
    if (trimmed.find('{') != std::string::npos) {
        ScopeMetrics scope;
        scope.start_line = line_num;
        scope.nesting_level = current_depth;
        
        // Detect scope type
        if (trimmed.find("namespace") != std::string::npos) {
            scope.type = ScopeType::Namespace;
        } else if (trimmed.find("class") != std::string::npos) {
            scope.type = ScopeType::Class;
        } else if (is_function_definition(trimmed)) {
            scope.type = ScopeType::Function;
        } else if (trimmed.find("if") != std::string::npos ||
                   trimmed.find("for") != std::string::npos ||
                   trimmed.find("while") != std::string::npos) {
            scope.type = ScopeType::ConditionalBlock;
        } else {
            scope.type = ScopeType::Block;
        }
        
        scopes.push_back(scope);
        current_depth++;
    }
    
    // Closing scope
    if (trimmed.find('}') != std::string::npos && current_depth > 0) {
        // Find matching open scope and set end line
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->end_line == 0 && it->nesting_level == current_depth - 1) {
                it->end_line = line_num;
                break;
            }
        }
        current_depth--;
    }
}

FunctionMetrics CodeAnalyzer::extract_function_metrics(
    const std::vector<std::string>& lines,
    size_t start_line)
{
    FunctionMetrics metrics;
    metrics.line_start = static_cast<tbb32>(start_line);
    
    // Extract function name (simplified)
    std::string first_line = trim(lines[start_line]);
    size_t paren = first_line.find('(');
    if (paren != std::string::npos) {
        // Find function name before '('
        size_t name_end = paren;
        while (name_end > 0 && std::isspace(first_line[name_end - 1])) name_end--;
        size_t name_start = name_end;
        while (name_start > 0 && (std::isalnum(first_line[name_start - 1]) || 
                                  first_line[name_start - 1] == '_')) {
            name_start--;
        }
        metrics.name = first_line.substr(name_start, name_end - name_start);
        
        // Count parameters (simplified - count commas + 1)
        size_t close_paren = first_line.find(')', paren);
        if (close_paren != std::string::npos) {
            std::string params = first_line.substr(paren + 1, close_paren - paren - 1);
            if (trim(params).empty()) {
                metrics.parameter_count = 0;
            } else {
                metrics.parameter_count = 1;
                for (char c : params) {
                    if (c == ',') metrics.parameter_count++;
                }
            }
        }
    }
    
    // Find function end and collect lines
    tbb32 depth = 0;
    bool started = false;
    std::vector<std::string> func_lines;
    
    for (size_t i = start_line; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        
        if (line.find('{') != std::string::npos) {
            started = true;
            depth++;
        }
        
        if (started) {
            func_lines.push_back(line);
            
            // Track maximum nesting
            for (char c : line) {
                if (c == '{') {
                    tbb32 current = depth;
                    if (current > metrics.max_nesting_depth) {
                        metrics.max_nesting_depth = current;
                    }
                }
            }
        }
        
        if (line.find('}') != std::string::npos && started) {
            depth--;
            if (depth == 0) {
                metrics.line_end = static_cast<tbb32>(i);
                break;
            }
        }
    }
    
    metrics.lines_of_code = metrics.line_end - metrics.line_start + 1;
    
    if (opts_.analyze_complexity) {
        metrics.cyclomatic_complexity = calculate_complexity(func_lines);
    }
    
    return metrics;
}

FileMetrics CodeAnalyzer::analyze_file(const std::string& filepath) {
    FileMetrics metrics;
    metrics.filepath = filepath;
    
    // Read file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        set_error("Failed to open file: " + filepath);
        return metrics;
    }
    
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    
    metrics.total_lines = static_cast<tbb32>(lines.size());
    
    // Analyze lines
    bool in_block_comment = false;
    tbb32 current_scope_depth = 0;
    tbb32 max_depth = 0;
    
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        
        // Classify line
        LineType type = classify_line(line, in_block_comment);
        
        switch (type) {
            case LineType::Code:
            case LineType::Mixed:
                metrics.code_lines++;
                break;
            case LineType::Comment:
                metrics.comment_lines++;
                break;
            case LineType::Blank:
                metrics.blank_lines++;
                break;
        }
        
        // Track scopes
        if (opts_.track_scopes) {
            track_scope_changes(line, static_cast<tbb32>(i + 1), metrics.scopes, current_scope_depth);
            if (current_scope_depth > max_depth) {
                max_depth = current_scope_depth;
            }
        }
        
        // Detect functions
        if (opts_.detect_functions && is_function_definition(line)) {
            FunctionMetrics func = extract_function_metrics(lines, i);
            if (!func.name.empty()) {
                metrics.functions.push_back(func);
                metrics.function_count++;
            }
        }
    }
    
    metrics.max_scope_depth = max_depth;
    
    // Update statistics
    stats_.files_processed++;
    stats_.total_lines += metrics.total_lines;
    stats_.total_functions += metrics.function_count;
    
    // Write telemetry
    if (opts_.verbose_output) {
        std::ostringstream oss;
        oss << "{\"file\":\"" << filepath << "\",\"lines\":" << metrics.total_lines
            << ",\"functions\":" << metrics.function_count << "}";
        streams::write_telemetry("analyze_file", oss.str());
    }
    
    // Write binary output
    if (opts_.binary_output) {
        streams::write_metrics_binary(metrics);
    }
    
    return metrics;
}

std::vector<FileMetrics> CodeAnalyzer::analyze_directory(
    const std::string& dirpath,
    const std::string& pattern)
{
    std::vector<FileMetrics> results;
    
    DIR* dir = opendir(dirpath.c_str());
    if (!dir) {
        set_error("Failed to open directory: " + dirpath);
        return results;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        
        // Skip . and ..
        if (name == "." || name == "..") continue;
        
        std::string fullpath = dirpath + "/" + name;
        
        struct stat st;
        if (stat(fullpath.c_str(), &st) != 0) continue;
        
        if (S_ISDIR(st.st_mode)) {
            // Recurse into subdirectory
            auto sub_results = analyze_directory(fullpath, pattern);
            results.insert(results.end(), sub_results.begin(), sub_results.end());
        } else if (S_ISREG(st.st_mode) && is_analyzable(fullpath)) {
            // Analyze file
            FileMetrics metrics = analyze_file(fullpath);
            results.push_back(metrics);
        }
    }
    
    closedir(dir);
    return results;
}

// ============================================================================
// Six-Stream Writers
// ============================================================================

namespace streams {

void write_telemetry(const std::string& operation, const std::string& data) {
    static const int STDDBG_FD = 3;
    
    struct stat st;
    if (fstat(STDDBG_FD, &st) != 0) return;
    
    std::ostringstream oss;
    oss << "{\"op\":\"" << operation << "\",\"data\":" << data << "}\n";
    std::string msg = oss.str();
    
    write(STDDBG_FD, msg.c_str(), msg.length());
}

void write_metrics_binary(const FileMetrics& metrics) {
    static const int STDDATO_FD = 5;
    
    struct stat st;
    if (fstat(STDDATO_FD, &st) != 0) return;
    
    // Simple binary format: [magic:4][version:2][metrics_struct]
    uint32_t magic = 0x41534350; // "ASCP"
    uint16_t version = 1;
    
    write(STDDATO_FD, &magic, sizeof(magic));
    write(STDDATO_FD, &version, sizeof(version));
    
    // Write key metrics as binary
    write(STDDATO_FD, &metrics.total_lines, sizeof(metrics.total_lines));
    write(STDDATO_FD, &metrics.code_lines, sizeof(metrics.code_lines));
    write(STDDATO_FD, &metrics.comment_lines, sizeof(metrics.comment_lines));
    write(STDDATO_FD, &metrics.blank_lines, sizeof(metrics.blank_lines));
    write(STDDATO_FD, &metrics.function_count, sizeof(metrics.function_count));
    write(STDDATO_FD, &metrics.max_scope_depth, sizeof(metrics.max_scope_depth));
}

bool is_stream_active(int fd) {
    struct stat st;
    return fstat(fd, &st) == 0;
}

} // namespace streams

// ============================================================================
// AnalysisProcessor Implementation
// ============================================================================

AnalysisProcessor::AnalysisProcessor(const AnalysisOptions& opts)
    : opts_(opts)
    , analyzer_(opts)
{}

std::string AnalysisProcessor::format_function(const FunctionMetrics& func) {
    std::ostringstream oss;
    oss << "  " << func.name << " (lines " << func.line_start << "-" << func.line_end << ")\n";
    oss << "    LOC: " << func.lines_of_code << "\n";
    oss << "    Complexity: " << func.cyclomatic_complexity << "\n";
    oss << "    Max Nesting: " << func.max_nesting_depth << "\n";
    oss << "    Parameters: " << func.parameter_count << "\n";
    return oss.str();
}

std::string AnalysisProcessor::format_metrics(const FileMetrics& metrics, bool summary) {
    std::ostringstream oss;
    
    oss << "File: " << metrics.filepath << "\n";
    oss << "  Total Lines: " << metrics.total_lines << "\n";
    oss << "  Code Lines: " << metrics.code_lines << "\n";
    oss << "  Comment Lines: " << metrics.comment_lines << "\n";
    oss << "  Blank Lines: " << metrics.blank_lines << "\n";
    oss << "  Functions: " << metrics.function_count << "\n";
    oss << "  Max Scope Depth: " << metrics.max_scope_depth << "\n";
    
    if (!summary && !metrics.functions.empty()) {
        oss << "\nFunctions:\n";
        for (const auto& func : metrics.functions) {
            oss << format_function(func);
        }
    }
    
    return oss.str();
}

std::string AnalysisProcessor::format_summary(const std::vector<FileMetrics>& all_metrics) {
    std::ostringstream oss;
    
    tbb64 total_lines = 0;
    tbb64 total_code = 0;
    tbb64 total_comments = 0;
    tbb64 total_blank = 0;
    tbb64 total_functions = 0;
    tbb32 max_depth = 0;
    
    for (const auto& m : all_metrics) {
        total_lines += m.total_lines;
        total_code += m.code_lines;
        total_comments += m.comment_lines;
        total_blank += m.blank_lines;
        total_functions += m.function_count;
        if (m.max_scope_depth > max_depth) {
            max_depth = m.max_scope_depth;
        }
    }
    
    oss << "\n=== Summary ===\n";
    oss << "Files Analyzed: " << all_metrics.size() << "\n";
    oss << "Total Lines: " << total_lines << "\n";
    oss << "  Code: " << total_code << "\n";
    oss << "  Comments: " << total_comments << "\n";
    oss << "  Blank: " << total_blank << "\n";
    oss << "Total Functions: " << total_functions << "\n";
    oss << "Max Scope Depth: " << max_depth << "\n";
    
    return oss.str();
}

std::string AnalysisProcessor::process_file(const std::string& filepath) {
    FileMetrics metrics = analyzer_.analyze_file(filepath);
    return format_metrics(metrics, false);
}

std::string AnalysisProcessor::process_directory(
    const std::string& dirpath,
    const std::string& pattern)
{
    std::vector<FileMetrics> all_metrics = analyzer_.analyze_directory(dirpath, pattern);
    
    std::ostringstream oss;
    for (const auto& metrics : all_metrics) {
        oss << format_metrics(metrics, true) << "\n";
    }
    
    oss << format_summary(all_metrics);
    
    return oss.str();
}

void AnalysisProcessor::process_stream(
    const std::vector<std::string>& filepaths,
    std::function<void(const FileMetrics&)> callback)
{
    for (const auto& path : filepaths) {
        FileMetrics metrics = analyzer_.analyze_file(path);
        callback(metrics);
    }
}

// ============================================================================
// FFI C API
// ============================================================================

static std::string g_result_buffer;
static std::string g_error_buffer;

extern "C" {

const char* aria_scope_analyze_file(const char* filepath) {
    if (!filepath) return "";
    
    AnalysisOptions opts;
    AnalysisProcessor processor(opts);
    
    g_result_buffer = processor.process_file(filepath);
    return g_result_buffer.c_str();
}

const char* aria_scope_analyze_directory(const char* dirpath, const char* pattern) {
    if (!dirpath) return "";
    
    AnalysisOptions opts;
    AnalysisProcessor processor(opts);
    
    g_result_buffer = processor.process_directory(
        dirpath,
        pattern ? pattern : "*"
    );
    
    return g_result_buffer.c_str();
}

const char* aria_scope_error() {
    return g_error_buffer.c_str();
}

void aria_scope_free(const char* str) {
    // In this simple implementation, we use static buffers
    // In production, would use proper memory management
}

} // extern "C"

} // namespace ascope
} // namespace aria
