/**
 * @file code_analyzer.hpp
 * @brief Code Scope Analysis Utility
 * 
 * Analyzes source code files for metrics including:
 * - Lines of code (total, code, comments, blank)
 * - Function definitions and complexity
 * - Scope depth and nesting levels
 * - Six-stream architecture for telemetry and results
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>

// TBB type aliases
using tbb8 = uint8_t;
using tbb32 = uint32_t;
using tbb64 = uint64_t;

// TBB error constant
constexpr tbb64 TBB64_ERR = 0xFFFFFFFFFFFFFFFFULL;

namespace aria {
namespace ascope {

// ============================================================================
// Forward Declarations
// ============================================================================

struct FileMetrics;
struct FunctionMetrics;
struct ScopeMetrics;
struct AnalysisOptions;
struct AnalysisStats;

// ============================================================================
// Type Definitions
// ============================================================================

enum class LineType {
    Code,
    Comment,
    Blank,
    Mixed  // Contains both code and comment
};

enum class ScopeType {
    Function,
    Class,
    Namespace,
    Block,
    ConditionalBlock
};

// ============================================================================
// Metrics Structures
// ============================================================================

struct FunctionMetrics {
    std::string name;
    tbb32 line_start;
    tbb32 line_end;
    tbb32 lines_of_code;
    tbb32 cyclomatic_complexity;  // Measure of code paths
    tbb32 max_nesting_depth;
    tbb32 parameter_count;
    
    FunctionMetrics()
        : line_start(0)
        , line_end(0)
        , lines_of_code(0)
        , cyclomatic_complexity(1)  // Base complexity is 1
        , max_nesting_depth(0)
        , parameter_count(0)
    {}
};

struct ScopeMetrics {
    ScopeType type;
    std::string name;
    tbb32 start_line;
    tbb32 end_line;
    tbb32 nesting_level;
    
    ScopeMetrics()
        : type(ScopeType::Block)
        , start_line(0)
        , end_line(0)
        , nesting_level(0)
    {}
};

struct FileMetrics {
    std::string filepath;
    tbb32 total_lines;
    tbb32 code_lines;
    tbb32 comment_lines;
    tbb32 blank_lines;
    tbb32 max_scope_depth;
    tbb32 function_count;
    
    std::vector<FunctionMetrics> functions;
    std::vector<ScopeMetrics> scopes;
    
    FileMetrics()
        : total_lines(0)
        , code_lines(0)
        , comment_lines(0)
        , blank_lines(0)
        , max_scope_depth(0)
        , function_count(0)
    {}
};

struct AnalysisStats {
    tbb64 files_processed;
    tbb64 total_lines;
    tbb64 total_functions;
    tbb64 errors_count;
    tbb64 warnings_count;
    
    AnalysisStats()
        : files_processed(0)
        , total_lines(0)
        , total_functions(0)
        , errors_count(0)
        , warnings_count(0)
    {}
};

// ============================================================================
// Analysis Options
// ============================================================================

struct AnalysisOptions {
    bool analyze_complexity;     // Calculate cyclomatic complexity
    bool track_scopes;          // Track scope depth and nesting
    bool count_comments;        // Distinguish comment vs code lines
    bool detect_functions;      // Find and analyze functions
    bool verbose_output;        // Detailed telemetry to stddbg
    bool binary_output;         // Write results to stddato (FD 5)
    
    AnalysisOptions()
        : analyze_complexity(true)
        , track_scopes(true)
        , count_comments(true)
        , detect_functions(true)
        , verbose_output(false)
        , binary_output(false)
    {}
};

// ============================================================================
// Code Analyzer Class
// ============================================================================

class CodeAnalyzer {
public:
    CodeAnalyzer(const AnalysisOptions& opts = AnalysisOptions());
    ~CodeAnalyzer() = default;
    
    // Analyze single file
    FileMetrics analyze_file(const std::string& filepath);
    
    // Analyze directory recursively
    std::vector<FileMetrics> analyze_directory(
        const std::string& dirpath,
        const std::string& pattern = "*"
    );
    
    // Get analysis statistics
    const AnalysisStats& get_stats() const { return stats_; }
    
    // Reset statistics
    void reset_stats() { stats_ = AnalysisStats(); }
    
private:
    AnalysisOptions opts_;
    AnalysisStats stats_;
    
    // Line analysis
    LineType classify_line(const std::string& line, bool& in_block_comment);
    bool is_function_definition(const std::string& line);
    FunctionMetrics extract_function_metrics(
        const std::vector<std::string>& lines,
        size_t start_line
    );
    
    // Scope tracking
    tbb32 count_scope_depth(const std::string& line, tbb32 current_depth);
    void track_scope_changes(
        const std::string& line,
        tbb32 line_num,
        std::vector<ScopeMetrics>& scopes,
        tbb32& current_depth
    );
    
    // Complexity analysis
    tbb32 calculate_complexity(const std::vector<std::string>& function_lines);
    tbb32 count_decision_points(const std::string& line);
    
    // Error handling
    void set_error(const std::string& msg);
    tbb64 error_code_;
    std::string error_msg_;
};

// ============================================================================
// Six-Stream Writers
// ============================================================================

namespace streams {

// Write JSON telemetry to FD 3 (stddbg)
void write_telemetry(const std::string& operation, const std::string& data);

// Write binary metrics to FD 5 (stddato)
void write_metrics_binary(const FileMetrics& metrics);

// Check if stream is active
bool is_stream_active(int fd);

} // namespace streams

// ============================================================================
// Analysis Processor
// ============================================================================

class AnalysisProcessor {
public:
    AnalysisProcessor(const AnalysisOptions& opts = AnalysisOptions());
    
    // Process single file and generate report
    std::string process_file(const std::string& filepath);
    
    // Process directory and generate summary
    std::string process_directory(
        const std::string& dirpath,
        const std::string& pattern = "*"
    );
    
    // Stream processing for pipeline use
    void process_stream(
        const std::vector<std::string>& filepaths,
        std::function<void(const FileMetrics&)> callback
    );
    
    // Get statistics
    const AnalysisStats& stats() const { return analyzer_.get_stats(); }
    
private:
    AnalysisOptions opts_;
    CodeAnalyzer analyzer_;
    
    // Formatting
    std::string format_metrics(const FileMetrics& metrics, bool summary = false);
    std::string format_function(const FunctionMetrics& func);
    std::string format_summary(const std::vector<FileMetrics>& all_metrics);
};

// ============================================================================
// Utility Functions
// ============================================================================

// Detect programming language from filename
std::string detect_language(const std::string& filepath);

// Check if file should be analyzed (based on extension)
bool is_analyzable(const std::string& filepath);

// Trim whitespace from string
std::string trim(const std::string& str);

// Check if line is empty or whitespace-only
bool is_blank_line(const std::string& line);

// Check if line starts with comment marker
bool is_comment_line(const std::string& line, const std::string& lang);

// ============================================================================
// FFI C API
// ============================================================================

extern "C" {

// Analyze a single file and return JSON metrics
const char* aria_scope_analyze_file(const char* filepath);

// Analyze directory and return summary
const char* aria_scope_analyze_directory(const char* dirpath, const char* pattern);

// Get last error message
const char* aria_scope_error();

// Free result string
void aria_scope_free(const char* str);

} // extern "C"

} // namespace ascope
} // namespace aria
