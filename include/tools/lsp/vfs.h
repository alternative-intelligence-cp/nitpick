#ifndef ARIA_LSP_VFS_H
#define ARIA_LSP_VFS_H

#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <optional>

namespace aria {
namespace lsp {

/**
 * Virtual File System
 * 
 * Maintains in-memory copy of files open in the editor.
 * Critical because editor content != disk content (unsaved changes).
 * 
 * Thread-safe: Multiple readers, single writer (std::shared_mutex).
 * Research_034: "The VFS should be implemented as a specialized class 
 * wrapping a std::unordered_map<std::string, std::string>"
 */
class VirtualFileSystem {
public:
    VirtualFileSystem() = default;
    
    /**
     * Add or update file content (didOpen, didChange)
     * Thread-safe: Acquires write lock
     */
    void set_content(const std::string& uri, const std::string& content);
    
    /**
     * Get file content
     * Thread-safe: Acquires read lock
     * Returns std::nullopt if file not in VFS
     */
    std::optional<std::string> get_content(const std::string& uri) const;
    
    /**
     * Remove file from VFS (didClose)
     * Thread-safe: Acquires write lock
     */
    void remove(const std::string& uri);
    
    /**
     * Check if file exists in VFS
     * Thread-safe: Acquires read lock
     */
    bool contains(const std::string& uri) const;
    
    /**
     * Get all open document URIs
     * Thread-safe: Acquires read lock
     */
    std::vector<std::string> get_open_documents() const;
    
    /**
     * Get number of open documents
     */
    size_t size() const;
    
private:
    // URI -> file content
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> files_;
};

} // namespace lsp
} // namespace aria

#endif // ARIA_LSP_VFS_H
