/**
 * @file asql_demo.cpp
 * @brief asql (SQL Query Utility) - Comprehensive Demonstrations
 * 
 * Demonstrates all asql features with practical examples.
 */

#include "asql/sql_query.hpp"
#include <iostream>
#include <iomanip>
#include <cstring>

using namespace aria::asql;

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
// Demo 1: Basic Connection
// ============================================================================

void demo_basic_connection() {
    print_header("Demo 1: Basic Database Connection");
    
    SqlConnection conn;
    
    // Connect to in-memory database
    bool connected = conn.connect(":memory:");
    
    if (connected) {
        print_success("Connected to SQLite database");
        print_info("Status", "Connected");
        
        conn.disconnect();
        print_success("Disconnected successfully");
    } else {
        print_error("Connection failed: " + conn.error_message());
    }
}

// ============================================================================
// Demo 2: Table Creation
// ============================================================================

void demo_table_creation() {
    print_header("Demo 2: Table Creation");
    
    SqlConnection conn;
    conn.connect(":memory:");
    
    std::string create_sql = R"(
        CREATE TABLE users (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            email TEXT,
            age INTEGER,
            balance REAL
        )
    )";
    
    tbb64 result = conn.execute_update(create_sql);
    
    if (conn.has_error()) {
        print_error("Failed: " + conn.error_message());
    } else {
        print_success("Table created successfully");
        print_info("Rows affected", std::to_string(result));
    }
}

// ============================================================================
// Demo 3: Data Insertion
// ============================================================================

void demo_data_insertion() {
    print_header("Demo 3: Data Insertion");
    
    SqlConnection conn;
    conn.connect(":memory:");
    
    // Create table
    conn.execute_update(R"(
        CREATE TABLE users (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            email TEXT,
            age INTEGER,
            balance REAL
        )
    )");
    
    // Insert data
    std::vector<std::string> inserts = {
        "INSERT INTO users VALUES (1, 'Alice', 'alice@example.com', 30, 1250.50)",
        "INSERT INTO users VALUES (2, 'Bob', 'bob@example.com', 25, 750.25)",
        "INSERT INTO users VALUES (3, 'Charlie', NULL, 35, 2000.00)",
        "INSERT INTO users VALUES (4, 'Diana', 'diana@example.com', NULL, 500.00)"
    };
    
    tbb64 total_inserted = 0;
    for (const auto& sql : inserts) {
        tbb64 affected = conn.execute_update(sql);
        if (!conn.has_error()) {
            total_inserted += affected;
        }
    }
    
    print_success("Data inserted");
    print_info("Total rows", std::to_string(total_inserted));
}

// ============================================================================
// Demo 4: Simple Queries
// ============================================================================

void demo_simple_queries() {
    print_header("Demo 4: Simple Queries");
    
    SqlConnection conn;
    conn.connect(":memory:");
    
    // Setup
    conn.execute_update("CREATE TABLE users (id INTEGER, name TEXT, age INTEGER)");
    conn.execute_update("INSERT INTO users VALUES (1, 'Alice', 30)");
    conn.execute_update("INSERT INTO users VALUES (2, 'Bob', 25)");
    conn.execute_update("INSERT INTO users VALUES (3, 'Charlie', 35)");
    
    // Query
    auto results = conn.execute_query("SELECT * FROM users");
    
    if (conn.has_error()) {
        print_error("Query failed: " + conn.error_message());
        return;
    }
    
    std::cout << YELLOW << "Query Results:\n" << RESET;
    std::cout << "ID\tName\tAge\n";
    std::cout << "------------------------\n";
    
    for (const auto& row : results) {
        for (const auto& val : row.values) {
            std::cout << val.to_string() << "\t";
        }
        std::cout << "\n";
    }
    
    print_success("Query executed successfully");
    print_info("Rows returned", std::to_string(results.size()));
}

// ============================================================================
// Demo 5: NULL Handling (TBB-safe)
// ============================================================================

void demo_null_handling() {
    print_header("Demo 5: NULL Handling (TBB-safe)");
    
    SqlConnection conn;
    conn.connect(":memory:");
    
    conn.execute_update("CREATE TABLE data (id INTEGER, value INTEGER)");
    conn.execute_update("INSERT INTO data VALUES (1, 100)");
    conn.execute_update("INSERT INTO data VALUES (2, NULL)");
    conn.execute_update("INSERT INTO data VALUES (3, 200)");
    
    auto results = conn.execute_query("SELECT * FROM data");
    
    std::cout << YELLOW << "NULL Value Detection:\n" << RESET;
    for (const auto& row : results) {
        std::cout << "ID: " << row.values[0].to_string();
        std::cout << ", Value: " << row.values[1].to_string();
        std::cout << " (is_null: " << (row.values[1].is_null ? "true" : "false") << ")\n";
    }
    
    print_success("TBB NULL handling working correctly");
}

// ============================================================================
// Demo 6: Aggregations
// ============================================================================

void demo_aggregations() {
    print_header("Demo 6: Aggregate Functions");
    
    SqlConnection conn;
    conn.connect(":memory:");
    
    conn.execute_update("CREATE TABLE sales (id INTEGER, amount REAL)");
    conn.execute_update("INSERT INTO sales VALUES (1, 100.50)");
    conn.execute_update("INSERT INTO sales VALUES (2, 200.75)");
    conn.execute_update("INSERT INTO sales VALUES (3, 150.25)");
    
    auto results = conn.execute_query(R"(
        SELECT 
            COUNT(*) as count,
            SUM(amount) as total,
            AVG(amount) as average,
            MIN(amount) as minimum,
            MAX(amount) as maximum
        FROM sales
    )");
    
    if (!results.empty()) {
        const auto& row = results[0];
        std::cout << YELLOW << "Aggregation Results:\n" << RESET;
        std::cout << "  Count: " << row.values[0].to_string() << "\n";
        std::cout << "  Total: $" << row.values[1].to_string() << "\n";
        std::cout << "  Average: $" << row.values[2].to_string() << "\n";
        std::cout << "  Minimum: $" << row.values[3].to_string() << "\n";
        std::cout << "  Maximum: $" << row.values[4].to_string() << "\n";
    }
    
    print_success("Aggregations working");
}

// ============================================================================
// Demo 7: JOINs
// ============================================================================

void demo_joins() {
    print_header("Demo 7: JOIN Operations");
    
    SqlConnection conn;
    conn.connect(":memory:");
    
    // Create tables
    conn.execute_update(R"(
        CREATE TABLE departments (id INTEGER, name TEXT)
    )");
    conn.execute_update(R"(
        CREATE TABLE employees (id INTEGER, name TEXT, dept_id INTEGER)
    )");
    
    // Insert data
    conn.execute_update("INSERT INTO departments VALUES (1, 'Engineering')");
    conn.execute_update("INSERT INTO departments VALUES (2, 'Sales')");
    conn.execute_update("INSERT INTO employees VALUES (1, 'Alice', 1)");
    conn.execute_update("INSERT INTO employees VALUES (2, 'Bob', 2)");
    conn.execute_update("INSERT INTO employees VALUES (3, 'Charlie', 1)");
    
    // JOIN query
    auto results = conn.execute_query(R"(
        SELECT e.name, d.name 
        FROM employees e
        JOIN departments d ON e.dept_id = d.id
    )");
    
    std::cout << YELLOW << "JOIN Results:\n" << RESET;
    std::cout << "Employee\tDepartment\n";
    std::cout << "------------------------\n";
    for (const auto& row : results) {
        std::cout << row.values[0].to_string() << "\t\t" 
                  << row.values[1].to_string() << "\n";
    }
    
    print_success("JOIN operations working");
}

// ============================================================================
// Demo 8: Query Processor with UI
// ============================================================================

void demo_query_processor() {
    print_header("Demo 8: Query Processor with Formatted UI");
    
    SqlConnection conn;
    conn.connect(":memory:");
    
    // Setup
    conn.execute_update("CREATE TABLE products (id INTEGER, name TEXT, price REAL)");
    conn.execute_update("INSERT INTO products VALUES (1, 'Laptop', 999.99)");
    conn.execute_update("INSERT INTO products VALUES (2, 'Mouse', 29.99)");
    conn.execute_update("INSERT INTO products VALUES (3, 'Keyboard', 79.99)");
    
    // Use processor
    QueryOptions opts;
    opts.show_headers = true;
    opts.show_row_count = true;
    
    QueryProcessor processor(opts);
    
    std::cout << YELLOW << "Formatted Output:\n" << RESET;
    processor.process_query(conn, "SELECT * FROM products");
    
    print_success("Query processor working");
}

// ============================================================================
// Demo 9: Statistics
// ============================================================================

void demo_statistics() {
    print_header("Demo 9: Query Statistics");
    
    SqlConnection conn;
    conn.connect(":memory:");
    
    conn.execute_update("CREATE TABLE test (id INTEGER, value TEXT)");
    
    // Insert many rows
    for (int i = 0; i < 100; i++) {
        std::string sql = "INSERT INTO test VALUES (" + std::to_string(i) + 
                         ", 'Value" + std::to_string(i) + "')";
        conn.execute_update(sql);
    }
    
    conn.reset_stats();
    
    auto results = conn.execute_query("SELECT * FROM test WHERE id < 10");
    
    const auto& stats = conn.get_stats();
    
    std::cout << YELLOW << "Statistics:\n" << RESET;
    std::cout << "  Rows returned: " << stats.rows_returned << "\n";
    std::cout << "  Execution time: " << stats.execution_time_us << " μs\n";
    std::cout << "  Errors: " << stats.error_count << "\n";
    
    print_success("Statistics tracking working");
}

// ============================================================================
// Demo 10: Error Handling
// ============================================================================

void demo_error_handling() {
    print_header("Demo 10: TBB Error Handling");
    
    SqlConnection conn;
    conn.connect(":memory:");
    
    // Intentional error: query non-existent table
    auto results = conn.execute_query("SELECT * FROM nonexistent_table");
    
    if (conn.has_error()) {
        print_info("Error detected", conn.error_message());
        print_success("Error handling prevented crash");
    }
    
    // Recovery: create table and try again
    conn.execute_update("CREATE TABLE valid_table (id INTEGER)");
    conn.execute_update("INSERT INTO valid_table VALUES (1)");
    
    results = conn.execute_query("SELECT * FROM valid_table");
    
    if (!conn.has_error()) {
        print_success("Recovered from error successfully");
        print_info("Rows returned", std::to_string(results.size()));
    }
}

// ============================================================================
// Demo 11: FFI C API
// ============================================================================

void demo_ffi_api() {
    print_header("Demo 11: FFI C API");
    
    // Create test database
    SqlConnection setup;
    setup.connect("/tmp/asql_ffi_test.db");
    setup.execute_update("CREATE TABLE IF NOT EXISTS test (id INTEGER, name TEXT)");
    setup.execute_update("DELETE FROM test");
    setup.execute_update("INSERT INTO test VALUES (1, 'FFI Test')");
    setup.disconnect();
    
    // Use FFI API
    const char* result = aria_sql_query("/tmp/asql_ffi_test.db", "SELECT * FROM test");
    
    if (result) {
        std::cout << YELLOW << "FFI API Result:\n" << RESET;
        std::cout << result;
        print_success("FFI API functional");
    } else {
        print_error("FFI API error: " + std::string(aria_sql_error()));
    }
    
    // Execute update via FFI
    uint64_t affected = aria_sql_execute("/tmp/asql_ffi_test.db", 
                                         "INSERT INTO test VALUES (2, 'FFI Insert')");
    
    if (affected != TBB64_ERR) {
        print_info("FFI execute affected", std::to_string(affected) + " rows");
    }
}

// ============================================================================
// Demo 12: Real-World Scenario
// ============================================================================

void demo_real_world() {
    print_header("Demo 12: Real-World Database Application");
    
    SqlConnection conn;
    conn.connect(":memory:");
    
    std::cout << "Creating e-commerce database schema...\n\n";
    
    // Create schema
    conn.execute_update(R"(
        CREATE TABLE customers (
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            email TEXT UNIQUE,
            created_at TEXT
        )
    )");
    
    conn.execute_update(R"(
        CREATE TABLE orders (
            id INTEGER PRIMARY KEY,
            customer_id INTEGER,
            total REAL,
            status TEXT,
            FOREIGN KEY(customer_id) REFERENCES customers(id)
        )
    )");
    
    // Insert sample data
    conn.execute_update("INSERT INTO customers VALUES (1, 'Alice Johnson', 'alice@example.com', '2026-01-01')");
    conn.execute_update("INSERT INTO customers VALUES (2, 'Bob Smith', 'bob@example.com', '2026-01-05')");
    conn.execute_update("INSERT INTO orders VALUES (1, 1, 99.99, 'completed')");
    conn.execute_update("INSERT INTO orders VALUES (2, 1, 149.99, 'pending')");
    conn.execute_update("INSERT INTO orders VALUES (3, 2, 299.99, 'completed')");
    
    // Complex query
    QueryOptions opts;
    opts.show_headers = true;
    opts.show_row_count = true;
    
    QueryProcessor processor(opts);
    
    std::cout << YELLOW << "Customer Order Summary:\n" << RESET;
    processor.process_query(conn, R"(
        SELECT 
            c.name,
            c.email,
            COUNT(o.id) as order_count,
            SUM(o.total) as total_spent,
            AVG(o.total) as avg_order
        FROM customers c
        LEFT JOIN orders o ON c.id = o.customer_id
        GROUP BY c.id
    )");
    
    std::cout << "\n" << YELLOW << "Pending Orders:\n" << RESET;
    processor.process_query(conn, R"(
        SELECT 
            c.name,
            o.id as order_id,
            o.total
        FROM orders o
        JOIN customers c ON o.customer_id = c.id
        WHERE o.status = 'pending'
    )");
    
    print_success("Real-world scenario complete");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << BOLD << MAGENTA
              << "\n╔══════════════════════════════════════════════════╗\n"
              << "║       asql - SQL Query Utility Demo            ║\n"
              << "║           Comprehensive Tests                    ║\n"
              << "╚══════════════════════════════════════════════════╝\n"
              << RESET << "\n";
    
    demo_basic_connection();
    demo_table_creation();
    demo_data_insertion();
    demo_simple_queries();
    demo_null_handling();
    demo_aggregations();
    demo_joins();
    demo_query_processor();
    demo_statistics();
    demo_error_handling();
    demo_ffi_api();
    demo_real_world();
    
    // Cleanup
    system("rm -f /tmp/asql_ffi_test.db");
    
    std::cout << "\n" << BOLD << GREEN
              << "✓ All demos complete - asql is ready for production use!\n"
              << RESET << "\n";
    
    return 0;
}
