/**
 * src/runtime/io/file.cpp
 *
 * Aria Standard Library - File I/O Functions
 * Version: 0.0.6
 *
 * Provides basic file operations for Aria programs.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

extern "C" {

// Forward declaration for Aria allocator
extern void* aria_alloc(size_t size);
extern void aria_free(void* ptr);

/**
 * Read entire file into a null-terminated string
 * Returns nullptr on error
 *
 * Usage in Aria:
 *   wild string:content = readFile("data.txt");
 *   if (content == null) {
 *       println("Error reading file");
 *   }
 */
char* aria_read_file(const char* path) {
    if (!path) return nullptr;

    // Open file
    FILE* file = fopen(path, "rb");
    if (!file) return nullptr;

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0) {
        fclose(file);
        return nullptr;
    }

    // Allocate buffer (using Aria's wild heap via mimalloc)
    char* buffer = (char*)aria_alloc(size + 1);
    if (!buffer) {
        fclose(file);
        return nullptr;
    }

    // Read file
    size_t bytes_read = fread(buffer, 1, size, file);
    fclose(file);

    buffer[bytes_read] = '\0';
    return buffer;
}

/**
 * Write string to file
 * Returns 0 on success, -1 on error
 *
 * Usage in Aria:
 *   int64:result = writeFile("output.txt", "Hello, World!");
 *   if (result != 0) {
 *       println("Error writing file");
 *   }
 */
int64_t aria_write_file(const char* path, const char* content) {
    if (!path || !content) return -1;

    FILE* file = fopen(path, "wb");
    if (!file) return -1;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);

    return (written == len) ? 0 : -1;
}

/**
 * Append string to file
 * Returns 0 on success, -1 on error
 */
int64_t aria_append_file(const char* path, const char* content) {
    if (!path || !content) return -1;

    FILE* file = fopen(path, "ab");
    if (!file) return -1;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);

    return (written == len) ? 0 : -1;
}

/**
 * Check if file exists
 * Returns 1 if exists, 0 if not
 */
int64_t aria_file_exists(const char* path) {
    if (!path) return 0;

    struct stat st;
    return (stat(path, &st) == 0) ? 1 : 0;
}

/**
 * Get file size in bytes
 * Returns -1 on error
 */
int64_t aria_file_size(const char* path) {
    if (!path) return -1;

    struct stat st;
    if (stat(path, &st) != 0) return -1;

    return (int64_t)st.st_size;
}

/**
 * Delete a file
 * Returns 0 on success, -1 on error
 */
int64_t aria_delete_file(const char* path) {
    if (!path) return -1;
    return unlink(path) == 0 ? 0 : -1;
}

/**
 * Read file as lines (returns array of strings)
 * This is a more complex function that would require
 * dynamic array support. For now, it's a placeholder.
 */
// TODO: Implement aria_read_lines when array support is ready

} // extern "C"
