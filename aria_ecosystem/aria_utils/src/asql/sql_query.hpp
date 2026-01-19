/**
 * @file sql_query.hpp
 * @brief SQL Query Utility - Six-stream SQL processing
 * 
 * Implements SQL query execution with Aria's architectural patterns:
 * - Six-stream topology (stdout/UI, stddbg/telemetry, stddato/binary)
 * - SQLite backend for portability
 * - TBB-safe value handling
 * - Zero-copy result handling where possible
 * - Binary result protocol
 * 
 * Reference: sysUtils_5_sql.txt (443 lines)
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>
#include <sqlite3.h>

// TBB type aliases
using tbb8 = uint8_t;
using tbb32 = uint32_t;
using tbb64 = uint64_t;

// TBB error constant
constexpr tbb64 TBB64_ERR = 0xFFFFFFFFFFFFFFFFULL;

namespace aria {
namespace asql {

/**
 * @brief SQL column type enumeration
 */
enum class ColumnType {
    INTEGER,
    REAL,
    TEXT,
    BLOB,
    NULL_TYPE
};

/**
 * @brief SQL value representation (TBB-safe)
 */
struct SqlValue {
    ColumnType type;
    int64_t int_value;      // For INTEGER
    double real_value;      // For REAL
    std::string text_value; // For TEXT
    std::vector<uint8_t> blob_value; // For BLOB
    bool is_null;
    
    SqlValue() : type(ColumnType::NULL_TYPE), int_value(0), real_value(0.0), is_null(true) {}
    
    // TBB error representation
    bool is_error() const { return is_null && type == ColumnType::NULL_TYPE; }
    
    // Convert to string for display
    std::string to_string() const;
};

/**
 * @brief SQL column metadata
 */
struct ColumnInfo {
    std::string name;
    ColumnType type;
    int index;
};

/**
 * @brief SQL result row
 */
struct ResultRow {
    std::vector<SqlValue> values;
    tbb64 row_id;
    
    ResultRow() : row_id(0) {}
};

/**
 * @brief Query execution statistics
 */
struct QueryStats {
    tbb64 rows_affected;
    tbb64 rows_returned;
    tbb64 execution_time_us;
    tbb64 bytes_transferred;
    tbb32 error_count;
    tbb32 warning_count;
    std::string last_error;
    
    QueryStats() 
        : rows_affected(0)
        , rows_returned(0)
        , execution_time_us(0)
        , bytes_transferred(0)
        , error_count(0)
        , warning_count(0) {}
    
    void record_error(const std::string& msg) {
        error_count++;
        last_error = msg;
    }
};

/**
 * @brief Query execution options
 */
struct QueryOptions {
    bool quiet_mode;        // Suppress UI output
    bool binary_output;     // Enable binary protocol
    bool show_headers;      // Show column headers
    bool show_row_count;    // Show row count at end
    bool detailed_timing;   // Detailed timing info
    bool explain_queries;   // Run EXPLAIN on queries
    
    QueryOptions()
        : quiet_mode(false)
        , binary_output(true)
        , show_headers(true)
        , show_row_count(true)
        , detailed_timing(false)
        , explain_queries(false) {}
};

/**
 * @brief SQL database connection wrapper
 */
class SqlConnection {
public:
    SqlConnection();
    ~SqlConnection();
    
    // Connection management
    bool connect(const std::string& db_path);
    void disconnect();
    bool is_connected() const { return db_ != nullptr && !has_error(); }
    
    // Query execution
    std::vector<ResultRow> execute_query(const std::string& sql);
    tbb64 execute_update(const std::string& sql);
    
    // Metadata
    std::vector<ColumnInfo> get_columns() const { return columns_; }
    const QueryStats& get_stats() const { return stats_; }
    void reset_stats() { stats_ = QueryStats(); }
    
    // Error handling (TBB-safe)
    bool has_error() const { return error_code_ == TBB64_ERR; }
    std::string error_message() const { return error_msg_; }
    
private:
    void set_error(const std::string& msg);
    void clear_error();
    
    sqlite3* db_;
    std::vector<ColumnInfo> columns_;
    QueryStats stats_;
    tbb64 error_code_;
    std::string error_msg_;
};

/**
 * @brief Six-stream query processor
 */
class QueryProcessor {
public:
    explicit QueryProcessor(const QueryOptions& opts);
    
    // Process single query
    void process_query(SqlConnection& conn, const std::string& sql);
    
    // Process query from file
    void process_file(SqlConnection& conn, const std::string& filepath);
    
    // Process interactive mode
    void process_interactive(SqlConnection& conn);
    
    const QueryStats& stats() const { return processor_stats_; }
    
private:
    // Formatters
    std::string format_header(const std::vector<ColumnInfo>& columns);
    std::string format_row(const ResultRow& row, const std::vector<ColumnInfo>& columns);
    std::string format_stats(const QueryStats& stats);
    std::string format_explain(const std::vector<ResultRow>& plan);
    
    void emit_ui(const std::string& msg);
    void emit_debug(const std::string& tag, const std::string& msg);
    void emit_data(const ResultRow& row, const std::vector<ColumnInfo>& columns);
    
    QueryOptions options_;
    QueryStats processor_stats_;
};

// Stream writers (separate from QueryProcessor)
namespace streams {
    // Telemetry to FD 3 (stddbg)
    void write_telemetry(const std::string& event, const std::map<std::string, std::string>& data);
    
    // Binary results to FD 5 (stddato)
    void write_result_binary(const ResultRow& row, const std::vector<ColumnInfo>& columns);
    void write_stats_binary(const QueryStats& stats);
}

} // namespace asql
} // namespace aria

// C FFI API
extern "C" {
    /**
     * @brief Execute SQL query and return formatted results
     * @param db_path Database file path
     * @param query SQL query string
     * @return Formatted results (caller must free)
     */
    const char* aria_sql_query(const char* db_path, const char* query);
    
    /**
     * @brief Execute SQL update (INSERT/UPDATE/DELETE)
     * @param db_path Database file path
     * @param sql SQL statement
     * @return Number of rows affected, or TBB64_ERR on error
     */
    uint64_t aria_sql_execute(const char* db_path, const char* sql);
    
    /**
     * @brief Get last error message
     * @return Error message (static buffer)
     */
    const char* aria_sql_error();
    
    /**
     * @brief Free result string
     * @param result Result from aria_sql_query
     */
    void aria_sql_free(const char* result);
}
