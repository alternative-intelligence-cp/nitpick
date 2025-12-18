#include "tools/lsp/vfs.h"
#include <mutex>
#include <algorithm>

namespace aria {
namespace lsp {

void VirtualFileSystem::set_content(const std::string& uri, const std::string& content) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    files_[uri] = content;
}

std::optional<std::string> VirtualFileSystem::get_content(const std::string& uri) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    auto it = files_.find(uri);
    if (it != files_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void VirtualFileSystem::remove(const std::string& uri) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    files_.erase(uri);
}

bool VirtualFileSystem::contains(const std::string& uri) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return files_.find(uri) != files_.end();
}

std::vector<std::string> VirtualFileSystem::get_open_documents() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<std::string> uris;
    uris.reserve(files_.size());
    
    for (const auto& pair : files_) {
        uris.push_back(pair.first);
    }
    
    return uris;
}

size_t VirtualFileSystem::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return files_.size();
}

} // namespace lsp
} // namespace aria
