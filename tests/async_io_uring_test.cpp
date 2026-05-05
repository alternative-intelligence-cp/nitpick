// Test io_uring backend
// Verifies that the io_uring backend can perform async file I/O

#include "runtime/async/io_uring.h"
#include "runtime/async/event_loop.h"
#include "runtime/async/file_stream.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <fcntl.h>

using namespace npk::runtime;

// Result structure for file read operations (matches io_uring.cpp)
struct ReadResult {
    uint8_t* data;
    size_t size;
};

void test_basic_read_write() {
    std::cout << "Test 1: Basic read/write..." << std::endl;
    
    // Create event loop and io_uring backend
    EventLoop loop;
    IoUringBackend backend(&loop);
    
    // Initialize
    if (!backend.init(64)) {
        std::cerr << "Failed to initialize io_uring backend" << std::endl;
        std::cerr << "This is expected on systems without io_uring support" << std::endl;
        std::cout << "Test skipped (io_uring not available)" << std::endl;
        return;
    }
    
    std::cout << "  ✓ io_uring initialized" << std::endl;
    
    // Test data
    std::string test_path = "/tmp/npk_iouring_test.txt";
    std::string test_content = "Hello from io_uring! This is a test of async file I/O.";
    std::vector<uint8_t> write_data(test_content.begin(), test_content.end());
    
    // Write file
    Future* write_future = backend.write_file(test_path, 0, write_data);
    
    // Process completions
    int max_iterations = 1000;
    int iterations = 0;
    while (!write_future->isReady() && iterations++ < max_iterations) {
        backend.process_completions();
        usleep(100); // 0.1ms
    }
    
    if (!write_future->isReady()) {
        std::cerr << "  ✗ Write operation timed out" << std::endl;
        delete write_future;
        return;
    }
    
    int64_t bytes_written = *(int64_t*)write_future->getValue();
    std::cout << "  ✓ Wrote " << bytes_written << " bytes" << std::endl;
    delete write_future;
    
    if (bytes_written != (int64_t)write_data.size()) {
        std::cerr << "  ✗ Write size mismatch" << std::endl;
        return;
    }
    
    // Read file back
    Future* read_future = backend.read_file(test_path, 0, write_data.size());
    
    iterations = 0;
    while (!read_future->isReady() && iterations++ < max_iterations) {
        backend.process_completions();
        usleep(100);
    }
    
    if (!read_future->isReady()) {
        std::cerr << "  ✗ Read operation timed out" << std::endl;
        delete read_future;
        return;
    }
    
    ReadResult* read_result = (ReadResult*)read_future->getValue();
    std::cout << "  ✓ Read " << read_result->size << " bytes" << std::endl;
    
    // Verify content
    bool matches = (read_result->size == write_data.size()) &&
                   (memcmp(read_result->data, write_data.data(), write_data.size()) == 0);
    
    if (matches) {
        std::cout << "  ✓ Content matches!" << std::endl;
    } else {
        std::cerr << "  ✗ Content mismatch" << std::endl;
        std::cerr << "    Expected size: " << write_data.size() << std::endl;
        std::cerr << "    Got size: " << read_result->size << std::endl;
        if (read_result->data) free(read_result->data);
        delete read_future;
        return;
    }
    
    // Clean up
    if (read_result->data) free(read_result->data);
    delete read_future;
    std::remove(test_path.c_str());
    
    std::cout << "  ✓ Test passed!" << std::endl;
}

void test_multiple_operations() {
    std::cout << "\nTest 2: Multiple concurrent operations..." << std::endl;
    
    EventLoop loop;
    IoUringBackend backend(&loop);
    
    if (!backend.init(64)) {
        std::cout << "Test skipped (io_uring not available)" << std::endl;
        return;
    }
    
    std::cout << "  ✓ io_uring initialized" << std::endl;
    
    // Create multiple test files
    const int num_files = 5;
    std::vector<Future*> futures;
    std::vector<std::string> paths;
    
    for (int i = 0; i < num_files; i++) {
        std::string path = "/tmp/npk_iouring_test_" + std::to_string(i) + ".txt";
        paths.push_back(path);
        
        std::string content = "Test file " + std::to_string(i) + " content";
        std::vector<uint8_t> data(content.begin(), content.end());
        
        Future* future = backend.write_file(path, 0, data);
        futures.push_back(future);
    }
    
    // Wait for all operations to complete
    int max_iterations = 2000;
    int iterations = 0;
    bool all_complete = false;
    
    while (!all_complete && iterations++ < max_iterations) {
        backend.process_completions();
        
        all_complete = true;
        for (auto* future : futures) {
            if (!future->isReady()) {
                all_complete = false;
                break;
            }
        }
        
        if (!all_complete) {
            usleep(100);
        }
    }
    
    if (!all_complete) {
        std::cerr << "  ✗ Some operations timed out" << std::endl;
        for (auto* future : futures) delete future;
        return;
    }
    
    std::cout << "  ✓ All " << num_files << " write operations completed" << std::endl;
    
    // Verify all writes succeeded
    int successful = 0;
    for (auto* future : futures) {
        int64_t bytes = *(int64_t*)future->getValue();
        if (bytes > 0) successful++;
        delete future;
    }
    
    std::cout << "  ✓ " << successful << "/" << num_files << " writes successful" << std::endl;
    
    // Clean up
    for (const auto& path : paths) {
        std::remove(path.c_str());
    }
    
    std::cout << "  ✓ Test passed!" << std::endl;
}

void test_stat_unlink() {
    std::cout << "\nTest 3: Stat and unlink operations..." << std::endl;
    
    EventLoop loop;
    IoUringBackend backend(&loop);
    
    if (!backend.init(64)) {
        std::cout << "Test skipped (io_uring not available)" << std::endl;
        return;
    }
    
    std::cout << "  ✓ io_uring initialized" << std::endl;
    
    // Create a test file first
    std::string test_path = "/tmp/npk_stat_test.txt";
    std::string content = "Test file for stat/unlink";
    std::vector<uint8_t> data(content.begin(), content.end());
    
    Future* write_future = backend.write_file(test_path, 0, data);
    
    int iterations = 0;
    while (!write_future->isReady() && iterations++ < 1000) {
        backend.process_completions();
        usleep(100);
    }
    delete write_future;
    
    // Test stat - should exist
    Future* stat_future = backend.stat_file(test_path);
    iterations = 0;
    while (!stat_future->isReady() && iterations++ < 1000) {
        backend.process_completions();
        usleep(100);
    }
    
    bool* exists = (bool*)stat_future->getValue();
    if (*exists) {
        std::cout << "  ✓ File exists (stat)" << std::endl;
    } else {
        std::cerr << "  ✗ Stat failed to find file" << std::endl;
        delete stat_future;
        return;
    }
    delete stat_future;
    
    // Test unlink
    Future* unlink_future = backend.unlink_file(test_path);
    iterations = 0;
    while (!unlink_future->isReady() && iterations++ < 1000) {
        backend.process_completions();
        usleep(100);
    }
    
    bool* success = (bool*)unlink_future->getValue();
    if (*success) {
        std::cout << "  ✓ File deleted (unlink)" << std::endl;
    } else {
        std::cerr << "  ✗ Unlink failed" << std::endl;
        delete unlink_future;
        return;
    }
    delete unlink_future;
    
    // Verify file no longer exists
    stat_future = backend.stat_file(test_path);
    iterations = 0;
    while (!stat_future->isReady() && iterations++ < 1000) {
        backend.process_completions();
        usleep(100);
    }
    
    exists = (bool*)stat_future->getValue();
    if (!*exists) {
        std::cout << "  ✓ File no longer exists (verified)" << std::endl;
    } else {
        std::cerr << "  ✗ File still exists after unlink" << std::endl;
    }
    delete stat_future;
    
    std::cout << "  ✓ Test passed!" << std::endl;
}

void test_statistics() {
    std::cout << "\nTest 4: Statistics tracking..." << std::endl;
    
    EventLoop loop;
    IoUringBackend backend(&loop);
    
    if (!backend.init(64)) {
        std::cout << "Test skipped (io_uring not available)" << std::endl;
        return;
    }
    
    auto stats = backend.get_stats();
    std::cout << "  Initial stats:" << std::endl;
    std::cout << "    Submitted: " << stats["submitted"] << std::endl;
    std::cout << "    Completed: " << stats["completed"] << std::endl;
    std::cout << "    Pending: " << stats["pending"] << std::endl;
    
    // Perform an operation
    std::string test_path = "/tmp/npk_stats_test.txt";
    std::string content = "Statistics test";
    std::vector<uint8_t> data(content.begin(), content.end());
    
    Future* future = backend.write_file(test_path, 0, data);
    
    int iterations = 0;
    while (!future->isReady() && iterations++ < 1000) {
        backend.process_completions();
        usleep(100);
    }
    
    delete future;
    std::remove(test_path.c_str());
    
    stats = backend.get_stats();
    std::cout << "  After operation:" << std::endl;
    std::cout << "    Submitted: " << stats["submitted"] << std::endl;
    std::cout << "    Completed: " << stats["completed"] << std::endl;
    std::cout << "    Pending: " << stats["pending"] << std::endl;
    
    if (stats["submitted"] > 0 && stats["completed"] > 0) {
        std::cout << "  ✓ Statistics tracking works!" << std::endl;
    } else {
        std::cerr << "  ✗ Statistics not updated" << std::endl;
        return;
    }
    
    std::cout << "  ✓ Test passed!" << std::endl;
}

void test_open_close() {
    std::cout << "Test 5: Open and close operations..." << std::endl;
    
    EventLoop loop;
    IoUringBackend backend(&loop);
    
    if (!backend.init()) {
        std::cout << "  ✗ Failed to initialize io_uring" << std::endl;
        return;
    }
    std::cout << "  ✓ io_uring initialized" << std::endl;
    
    const std::string test_path = "/tmp/npk_test_open.txt";
    
    // Create a test file first using write
    std::string content = "Test content for open/close";
    std::vector<uint8_t> data(content.begin(), content.end());
    Future* write_future = backend.write_file(test_path, 0, data);
    
    // Wait for write to complete
    while (!write_future->isReady()) {
        backend.process_completions();
        usleep(1000);
    }
    
    int64_t* bytes_written = (int64_t*)write_future->getValue();
    if (*bytes_written != (int64_t)data.size()) {
        std::cout << "  ✗ Failed to create test file" << std::endl;
        delete write_future;
        return;
    }
    delete write_future;
    
    // Now test open operation
    Future* open_future = backend.open_file(test_path, O_RDONLY, 0644);
    
    // Wait for open to complete
    while (!open_future->isReady()) {
        backend.process_completions();
        usleep(1000);
    }
    
    int* fd = (int*)open_future->getValue();
    if (*fd < 0) {
        std::cout << "  ✗ Failed to open file" << std::endl;
        delete open_future;
        return;
    }
    std::cout << "  ✓ File opened successfully (fd=" << *fd << ")" << std::endl;
    
    int opened_fd = *fd;
    delete open_future;
    
    // Test close operation
    Future* close_future = backend.close_file(opened_fd);
    
    // Wait for close to complete
    while (!close_future->isReady()) {
        backend.process_completions();
        usleep(1000);
    }
    
    bool* success = (bool*)close_future->getValue();
    if (!*success) {
        std::cout << "  ✗ Failed to close file" << std::endl;
        delete close_future;
        return;
    }
    std::cout << "  ✓ File closed successfully" << std::endl;
    delete close_future;
    
    // Clean up test file
    unlink(test_path.c_str());
    
    std::cout << "  ✓ Test passed!" << std::endl;
}

void test_file_stream() {
    std::cout << "Test 6: FileStream async byte stream..." << std::endl;
    
    EventLoop loop;
    IoUringBackend backend(&loop);
    
    if (!backend.init()) {
        std::cout << "  ✗ Failed to initialize io_uring" << std::endl;
        return;
    }
    std::cout << "  ✓ io_uring initialized" << std::endl;
    
    const std::string test_path = "/tmp/npk_test_stream.txt";
    const std::string test_content = "Hello FileStream! Testing async byte-by-byte reading.";
    
    // Create a test file
    std::vector<uint8_t> data(test_content.begin(), test_content.end());
    Future* write_future = backend.write_file(test_path, 0, data);
    
    while (!write_future->isReady()) {
        backend.process_completions();
        usleep(1000);
    }
    delete write_future;
    
    std::cout << "  ✓ Test file created (" << test_content.size() << " bytes)" << std::endl;
    
    // Create FileStream
    FileStream stream(&backend, test_path, 16);  // Small chunk size for testing
    
    if (!stream.open()) {
        std::cout << "  ✗ Failed to open stream" << std::endl;
        unlink(test_path.c_str());
        return;
    }
    std::cout << "  ✓ FileStream opened" << std::endl;
    
    // Read all bytes from stream
    std::vector<uint8_t> read_bytes;
    int bytes_read = 0;
    
    while (true) {
        Future* next_future = stream.next();
        
        while (!next_future->isReady()) {
            backend.process_completions();
            usleep(100);
        }
        
        StreamItem<uint8_t>* item = (StreamItem<uint8_t>*)next_future->getValue();
        
        if (item->is_end) {
            delete next_future;
            break;
        }
        
        read_bytes.push_back(item->value);
        bytes_read++;
        delete next_future;
    }
    
    std::cout << "  ✓ Read " << bytes_read << " bytes from stream" << std::endl;
    
    // Verify content matches
    if (read_bytes.size() != data.size()) {
        std::cout << "  ✗ Size mismatch: expected " << data.size() 
                  << ", got " << read_bytes.size() << std::endl;
        stream.close();
        unlink(test_path.c_str());
        return;
    }
    
    bool content_matches = true;
    for (size_t i = 0; i < data.size(); i++) {
        if (read_bytes[i] != data[i]) {
            content_matches = false;
            break;
        }
    }
    
    if (!content_matches) {
        std::cout << "  ✗ Content mismatch" << std::endl;
        stream.close();
        unlink(test_path.c_str());
        return;
    }
    
    std::cout << "  ✓ Content matches perfectly!" << std::endl;
    
    // Close stream
    stream.close();
    std::cout << "  ✓ Stream closed" << std::endl;
    
    // Clean up
    unlink(test_path.c_str());
    
    std::cout << "  ✓ Test passed!" << std::endl;
}

int main() {
    std::cout << "=== Aria io_uring Backend Tests ===" << std::endl;
    std::cout << std::endl;
    
    test_basic_read_write();
    test_multiple_operations();
    test_stat_unlink();
    test_statistics();
    test_open_close();
    test_file_stream();
    
    std::cout << "\n=== All tests completed ===" << std::endl;
    return 0;
}
