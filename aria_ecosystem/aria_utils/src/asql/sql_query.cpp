/**
 * @file sql_query.cpp
 * @brief SQL Query Utility Implementation
 */

#include "asql/sql_query.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>
#include <unistd.h>

namespace aria {
namespace asql {

// ============================================================================
// SqlValue Implementation
// ============================================================================

std::string SqlValue::to_string() const {
    if (is_null) return "NULL";
    
    switch (type) {
        case ColumnType::INTEGER:
            return std::to_string(int_value);
        case ColumnType::REAL:
            return std::to_string(real_value);
        case ColumnType::TEXT:
            return text_value;
        case ColumnType::BLOB:
            return "<BLOB:" + std::to_string(blob_value.size()) + " bytes>";
        case ColumnType::NULL_TYPE:
            return "NULL";
    }
    return "";
}

// ============================================================================
// SqlConnection Implementation
// ============================================================================

SqlConnection::SqlConnection()
    : db_(nullptr)
    , error_code_(0) {
}

SqlConnection::~SqlConnection() {
    disconnect();
}

void SqlConnection::set_error(const std::string& msg) {
    error_code_ = TBB64_ERR;
    error_msg_ = msg;
    stats_.record_error(msg);
}

void SqlConnection::clear_error() {
    error_code_ = 0;
    error_msg_.clear();
}

bool SqlConnection::connect(const std::string& db_path) {
    if (db_) {
        disconnect();
    }
    
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        set_error(std::string("Failed to open database: ") + sqlite3_errmsg(db_));
        return false;
    }
    
    clear_error();
    return true;
}

void SqlConnection::disconnect() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

std::vector<ResultRow> SqlConnection::execute_query(const std::string& sql) {
    std::vector<ResultRow> results;
    
    if (!db_) {
        set_error("Not connected to database");
        return results;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        set_error(std::string("Prepare failed: ") + sqlite3_errmsg(db_));
        return results;
    }
    
    // Extract column information
    columns_.clear();
    int col_count = sqlite3_column_count(stmt);
    for (int i = 0; i < col_count; i++) {
        ColumnInfo info;
        info.name = sqlite3_column_name(stmt, i);
        info.index = i;
        
        // Type will be determined per-row
        info.type = ColumnType::NULL_TYPE;
        columns_.push_back(info);
    }
    
    // Fetch rows
    tbb64 row_count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        ResultRow row;
        row.row_id = row_count++;
        
        for (int i = 0; i < col_count; i++) {
            SqlValue value;
            
            int col_type = sqlite3_column_type(stmt, i);
            
            switch (col_type) {
                case SQLITE_INTEGER:
                    value.type = ColumnType::INTEGER;
                    value.int_value = sqlite3_column_int64(stmt, i);
                    value.is_null = false;
                    break;
                    
                case SQLITE_FLOAT:
                    value.type = ColumnType::REAL;
                    value.real_value = sqlite3_column_double(stmt, i);
                    value.is_null = false;
                    break;
                    
                case SQLITE_TEXT: {
                    value.type = ColumnType::TEXT;
                    const unsigned char* text = sqlite3_column_text(stmt, i);
                    value.text_value = text ? reinterpret_cast<const char*>(text) : "";
                    value.is_null = false;
                    break;
                }
                    
                case SQLITE_BLOB: {
                    value.type = ColumnType::BLOB;
                    const void* blob = sqlite3_column_blob(stmt, i);
                    int blob_size = sqlite3_column_bytes(stmt, i);
                    if (blob && blob_size > 0) {
                        const uint8_t* data = static_cast<const uint8_t*>(blob);
                        value.blob_value.assign(data, data + blob_size);
                    }
                    value.is_null = false;
                    break;
                }
                    
                case SQLITE_NULL:
                default:
                    value.type = ColumnType::NULL_TYPE;
                    value.is_null = true;
                    break;
            }
            
            row.values.push_back(value);
        }
        
        results.push_back(row);
    }
    
    if (rc != SQLITE_DONE) {
        set_error(std::string("Step failed: ") + sqlite3_errmsg(db_));
    } else {
        clear_error();
    }
    
    sqlite3_finalize(stmt);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    stats_.rows_returned = row_count;
    stats_.execution_time_us = duration.count();
    
    return results;
}

tbb64 SqlConnection::execute_update(const std::string& sql) {
    if (!db_) {
        set_error("Not connected to database");
        return TBB64_ERR;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        set_error(error);
        return TBB64_ERR;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    tbb64 changes = sqlite3_changes(db_);
    stats_.rows_affected = changes;
    stats_.execution_time_us = duration.count();
    
    clear_error();
    return changes;
}

// ============================================================================
// QueryProcessor Implementation
// ============================================================================

QueryProcessor::QueryProcessor(const QueryOptions& opts)
    : options_(opts) {
}

void QueryProcessor::emit_ui(const std::string& msg) {
    if (!options_.quiet_mode) {
        std::cout << msg;
        std::cout.flush();
    }
}

void QueryProcessor::emit_debug(const std::string& tag, const std::string& msg) {
    // Write to FD 3 (stddbg)
    std::string output = "[" + tag + "] " + msg + "\n";
    ssize_t written = write(3, output.c_str(), output.size());
    (void)written; // Suppress unused warning
}

void QueryProcessor::emit_data(const ResultRow& row, const std::vector<ColumnInfo>& columns) {
    if (options_.binary_output) {
        asql::streams::write_result_binary(row, columns);
    }
}

std::string QueryProcessor::format_header(const std::vector<ColumnInfo>& columns) {
    if (columns.empty()) return "";
    
    std::ostringstream oss;
    
    // Calculate column widths
    std::vector<size_t> widths;
    for (const auto& col : columns) {
        widths.push_back(std::max<size_t>(12, col.name.length() + 2));
    }
    
    // Top border
    oss << "+";
    for (size_t w : widths) {
        oss << std::string(w, '-') << "+";
    }
    oss << "\n";
    
    // Header row
    oss << "|";
    for (size_t i = 0; i < columns.size(); i++) {
        oss << " " << std::left << std::setw(widths[i] - 1) << columns[i].name << "|";
    }
    oss << "\n";
    
    // Separator
    oss << "+";
    for (size_t w : widths) {
        oss << std::string(w, '=') << "+";
    }
    oss << "\n";
    
    return oss.str();
}

std::string QueryProcessor::format_row(const ResultRow& row, const std::vector<ColumnInfo>& columns) {
    std::ostringstream oss;
    
    // Calculate column widths
    std::vector<size_t> widths;
    for (const auto& col : columns) {
        widths.push_back(std::max<size_t>(12, col.name.length() + 2));
    }
    
    oss << "|";
    for (size_t i = 0; i < row.values.size() && i < columns.size(); i++) {
        std::string value_str = row.values[i].to_string();
        if (value_str.length() > widths[i] - 2) {
            value_str = value_str.substr(0, widths[i] - 5) + "...";
        }
        oss << " " << std::left << std::setw(widths[i] - 1) << value_str << "|";
    }
    oss << "\n";
    
    return oss.str();
}

std::string QueryProcessor::format_stats(const QueryStats& stats) {
    std::ostringstream oss;
    oss << "\n(" << stats.rows_returned << " rows in " 
        << (stats.execution_time_us / 1000.0) << " ms)\n";
    return oss.str();
}

std::string QueryProcessor::format_explain(const std::vector<ResultRow>& plan) {
    std::ostringstream oss;
    oss << "Query Plan:\n";
    for (const auto& row : plan) {
        for (const auto& val : row.values) {
            oss << "  " << val.to_string() << "\n";
        }
    }
    return oss.str();
}

void QueryProcessor::process_query(SqlConnection& conn, const std::string& sql) {
    emit_debug("asql", "Processing query: " + sql.substr(0, 50) + (sql.length() > 50 ? "..." : ""));
    
    // Handle EXPLAIN if requested
    if (options_.explain_queries && sql.find("SELECT") != std::string::npos) {
        std::string explain_sql = "EXPLAIN QUERY PLAN " + sql;
        auto plan = conn.execute_query(explain_sql);
        emit_debug("plan", format_explain(plan));
    }
    
    // Determine if this is a query or update
    std::string sql_upper = sql;
    for (char& c : sql_upper) c = std::toupper(c);
    
    bool is_select = sql_upper.find("SELECT") != std::string::npos;
    
    if (is_select) {
        // Execute query
        auto results = conn.execute_query(sql);
        
        if (conn.has_error()) {
            emit_debug("error", conn.error_message());
            processor_stats_.record_error(conn.error_message());
            return;
        }
        
        // Output header
        if (options_.show_headers && !results.empty()) {
            emit_ui(format_header(conn.get_columns()));
        }
        
        // Output rows
        for (const auto& row : results) {
            emit_ui(format_row(row, conn.get_columns()));
            emit_data(row, conn.get_columns());
        }
        
        // Bottom border
        if (options_.show_headers && !results.empty()) {
            std::vector<size_t> widths;
            for (const auto& col : conn.get_columns()) {
                widths.push_back(std::max<size_t>(12, col.name.length() + 2));
            }
            std::ostringstream border;
            border << "+";
            for (size_t w : widths) {
                border << std::string(w, '-') << "+";
            }
            border << "\n";
            emit_ui(border.str());
        }
        
        // Stats
        if (options_.show_row_count) {
            emit_ui(format_stats(conn.get_stats()));
        }
        
        emit_debug("stats", "Rows: " + std::to_string(conn.get_stats().rows_returned) + 
                   ", Time: " + std::to_string(conn.get_stats().execution_time_us) + " us");
        
    } else {
        // Execute update
        tbb64 affected = conn.execute_update(sql);
        
        if (conn.has_error()) {
            emit_debug("error", conn.error_message());
            processor_stats_.record_error(conn.error_message());
            return;
        }
        
        emit_ui("Query OK, " + std::to_string(affected) + " row(s) affected\n");
        emit_debug("update", "Affected: " + std::to_string(affected));
    }
    
    processor_stats_.rows_returned += conn.get_stats().rows_returned;
    processor_stats_.rows_affected += conn.get_stats().rows_affected;
}

void QueryProcessor::process_file(SqlConnection& conn, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        emit_debug("error", "Cannot open file: " + filepath);
        return;
    }
    
    std::string sql;
    std::string line;
    
    while (std::getline(file, line)) {
        // Skip comments
        if (line.empty() || line[0] == '#' || line.substr(0, 2) == "--") {
            continue;
        }
        
        sql += line + " ";
        
        // Check for statement terminator
        if (line.find(';') != std::string::npos) {
            process_query(conn, sql);
            sql.clear();
        }
    }
    
    // Process remaining SQL
    if (!sql.empty()) {
        process_query(conn, sql);
    }
}

void QueryProcessor::process_interactive(SqlConnection& conn) {
    emit_ui("SQLite Interactive Mode (type .exit to quit)\n");
    emit_ui("asql> ");
    
    std::string sql;
    std::string line;
    
    while (std::getline(std::cin, line)) {
        if (line == ".exit" || line == ".quit") {
            break;
        }
        
        if (line.empty()) {
            emit_ui("asql> ");
            continue;
        }
        
        sql += line + " ";
        
        if (line.back() == ';') {
            process_query(conn, sql);
            sql.clear();
        }
        
        emit_ui(sql.empty() ? "asql> " : "   -> ");
    }
}

// ============================================================================
// Stream Writers
// ============================================================================

namespace streams {

void write_telemetry(const std::string& event, const std::map<std::string, std::string>& data) {
    // JSON format to FD 3
    std::ostringstream json;
    json << "{\"event\":\"" << event << "\"";
    for (const auto& kv : data) {
        json << ",\"" << kv.first << "\":\"" << kv.second << "\"";
    }
    json << "}\n";
    
    std::string output = json.str();
    write(3, output.c_str(), output.size());
}

void write_result_binary(const ResultRow& row, const std::vector<ColumnInfo>& columns) {
    // Binary protocol to FD 5
    // Format: [magic:4][row_id:8][col_count:4][values...]
    
    std::vector<uint8_t> buffer;
    
    // Magic number: "ASQL"
    buffer.push_back('A');
    buffer.push_back('S');
    buffer.push_back('Q');
    buffer.push_back('L');
    
    // Row ID (8 bytes, little-endian)
    tbb64 rid = row.row_id;
    for (int i = 0; i < 8; i++) {
        buffer.push_back(static_cast<uint8_t>(rid & 0xFF));
        rid >>= 8;
    }
    
    // Column count (4 bytes)
    tbb32 col_count = row.values.size();
    for (int i = 0; i < 4; i++) {
        buffer.push_back(static_cast<uint8_t>(col_count & 0xFF));
        col_count >>= 8;
    }
    
    // Values
    for (const auto& val : row.values) {
        buffer.push_back(static_cast<uint8_t>(val.type));
        buffer.push_back(val.is_null ? 1 : 0);
        
        if (!val.is_null) {
            switch (val.type) {
                case ColumnType::INTEGER: {
                    int64_t iv = val.int_value;
                    for (int i = 0; i < 8; i++) {
                        buffer.push_back(static_cast<uint8_t>(iv & 0xFF));
                        iv >>= 8;
                    }
                    break;
                }
                case ColumnType::REAL: {
                    double rv = val.real_value;
                    uint64_t bits;
                    std::memcpy(&bits, &rv, 8);
                    for (int i = 0; i < 8; i++) {
                        buffer.push_back(static_cast<uint8_t>(bits & 0xFF));
                        bits >>= 8;
                    }
                    break;
                }
                case ColumnType::TEXT: {
                    tbb32 len = val.text_value.length();
                    for (int i = 0; i < 4; i++) {
                        buffer.push_back(static_cast<uint8_t>(len & 0xFF));
                        len >>= 8;
                    }
                    buffer.insert(buffer.end(), val.text_value.begin(), val.text_value.end());
                    break;
                }
                case ColumnType::BLOB: {
                    tbb32 len = val.blob_value.size();
                    for (int i = 0; i < 4; i++) {
                        buffer.push_back(static_cast<uint8_t>(len & 0xFF));
                        len >>= 8;
                    }
                    buffer.insert(buffer.end(), val.blob_value.begin(), val.blob_value.end());
                    break;
                }
                default:
                    break;
            }
        }
    }
    
    write(5, buffer.data(), buffer.size());
}

void write_stats_binary(const QueryStats& stats) {
    // Stats protocol to FD 5
    std::vector<uint8_t> buffer;
    
    // Magic: "STAT"
    buffer.push_back('S');
    buffer.push_back('T');
    buffer.push_back('A');
    buffer.push_back('T');
    
    // Serialize stats
    auto write_u64 = [&buffer](uint64_t val) {
        for (int i = 0; i < 8; i++) {
            buffer.push_back(static_cast<uint8_t>(val & 0xFF));
            val >>= 8;
        }
    };
    
    write_u64(stats.rows_returned);
    write_u64(stats.rows_affected);
    write_u64(stats.execution_time_us);
    
    write(5, buffer.data(), buffer.size());
}

} // namespace streams

} // namespace asql
} // namespace aria

// ============================================================================
// FFI Implementation
// ============================================================================

static std::string g_ffi_error;
static std::string g_ffi_result;

extern "C" {

const char* aria_sql_query(const char* db_path, const char* query) {
    using namespace aria::asql;
    
    SqlConnection conn;
    if (!conn.connect(db_path)) {
        g_ffi_error = conn.error_message();
        return nullptr;
    }
    
    auto results = conn.execute_query(query);
    
    if (conn.has_error()) {
        g_ffi_error = conn.error_message();
        return nullptr;
    }
    
    // Format results
    std::ostringstream oss;
    
    // Header
    for (const auto& col : conn.get_columns()) {
        oss << col.name << "\t";
    }
    oss << "\n";
    
    // Rows
    for (const auto& row : results) {
        for (const auto& val : row.values) {
            oss << val.to_string() << "\t";
        }
        oss << "\n";
    }
    
    g_ffi_result = oss.str();
    return g_ffi_result.c_str();
}

uint64_t aria_sql_execute(const char* db_path, const char* sql) {
    using namespace aria::asql;
    
    SqlConnection conn;
    if (!conn.connect(db_path)) {
        g_ffi_error = conn.error_message();
        return TBB64_ERR;
    }
    
    tbb64 result = conn.execute_update(sql);
    
    if (conn.has_error()) {
        g_ffi_error = conn.error_message();
        return TBB64_ERR;
    }
    
    return result;
}

const char* aria_sql_error() {
    return g_ffi_error.c_str();
}

void aria_sql_free(const char* result) {
    // Result is in static buffer, no need to free
    (void)result;
}

}
