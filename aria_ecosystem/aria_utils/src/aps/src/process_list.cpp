/**
 * process_list.cpp
 * Cross-platform process listing implementation
 *
 * Linux: Uses /proc filesystem
 * macOS: Uses sysctl
 * Windows: Uses Windows API
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "aps/process_list.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>
#include <map>

#ifdef __linux__
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <libproc.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#endif

namespace aria {
namespace aps {

// ============================================================================
// ProcessInfo Constructor
// ============================================================================

ProcessInfo::ProcessInfo()
    : pid(0), ppid(0), pgid(0), sid(0),
      uid(0), gid(0), euid(0), egid(0),
      state(ProcessState::UNKNOWN),
      nice(0), priority(0), threads(1),
      utime(0), stime(0), start_time(0),
      vsize(0), rss(0), shared(0), text(0), data(0),
      cpu_percent(0.0),
      read_bytes(0), write_bytes(0) {}

FilterOptions::FilterOptions()
    : include_threads(false), include_kernel(false) {}

// ============================================================================
// State Conversion
// ============================================================================

char state_to_char(ProcessState state) {
    switch (state) {
        case ProcessState::RUNNING:    return 'R';
        case ProcessState::SLEEPING:   return 'S';
        case ProcessState::DISK_SLEEP: return 'D';
        case ProcessState::STOPPED:    return 'T';
        case ProcessState::ZOMBIE:     return 'Z';
        case ProcessState::DEAD:       return 'X';
        case ProcessState::IDLE:       return 'I';
        default:                       return '?';
    }
}

ProcessState char_to_state(char c) {
    switch (c) {
        case 'R': return ProcessState::RUNNING;
        case 'S': return ProcessState::SLEEPING;
        case 'D': return ProcessState::DISK_SLEEP;
        case 'T': case 't': return ProcessState::STOPPED;
        case 'Z': return ProcessState::ZOMBIE;
        case 'X': case 'x': return ProcessState::DEAD;
        case 'I': return ProcessState::IDLE;
        default:  return ProcessState::UNKNOWN;
    }
}

// ============================================================================
// Linux Implementation
// ============================================================================

#ifdef __linux__

static std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static std::string read_file_first_line(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";

    std::string line;
    std::getline(file, line);
    return line;
}

static std::string read_link(const std::string& path) {
    char buf[4096];
    ssize_t len = readlink(path.c_str(), buf, sizeof(buf) - 1);
    if (len < 0) return "";
    buf[len] = '\0';
    return std::string(buf);
}

static bool parse_stat(const std::string& stat_content, ProcessInfo& info) {
    // Format: pid (comm) state ppid pgid sid ...
    // The comm field can contain spaces and parentheses, so we need special parsing

    size_t comm_start = stat_content.find('(');
    size_t comm_end = stat_content.rfind(')');
    if (comm_start == std::string::npos || comm_end == std::string::npos) {
        return false;
    }

    // Parse PID
    info.pid = std::stoi(stat_content.substr(0, comm_start - 1));

    // Parse comm (process name)
    info.name = stat_content.substr(comm_start + 1, comm_end - comm_start - 1);

    // Parse rest after comm
    std::string rest = stat_content.substr(comm_end + 2);
    std::istringstream iss(rest);

    char state_char;
    iss >> state_char;
    info.state = char_to_state(state_char);

    iss >> info.ppid >> info.pgid >> info.sid;

    // Skip tty_nr, tpgid, flags
    int dummy;
    unsigned long dummy_ul;
    iss >> dummy >> dummy >> dummy_ul;

    // Skip minflt, cminflt, majflt, cmajflt
    iss >> dummy_ul >> dummy_ul >> dummy_ul >> dummy_ul;

    // utime, stime
    iss >> info.utime >> info.stime;

    // Skip cutime, cstime
    long long dummy_ll;
    iss >> dummy_ll >> dummy_ll;

    // priority, nice
    iss >> info.priority >> info.nice;

    // num_threads
    iss >> info.threads;

    // Skip itrealvalue
    iss >> dummy_ll;

    // starttime
    iss >> info.start_time;

    // vsize, rss
    iss >> info.vsize >> info.rss;

    return true;
}

static bool parse_status(const std::string& status_content, ProcessInfo& info) {
    std::istringstream iss(status_content);
    std::string line;

    while (std::getline(iss, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        // Trim leading whitespace from value
        size_t start = value.find_first_not_of(" \t");
        if (start != std::string::npos) {
            value = value.substr(start);
        }

        if (key == "Uid") {
            std::istringstream uid_iss(value);
            uid_iss >> info.uid >> info.euid;
        } else if (key == "Gid") {
            std::istringstream gid_iss(value);
            gid_iss >> info.gid >> info.egid;
        } else if (key == "VmSize") {
            info.vsize = std::stoull(value) * 1024;  // Convert kB to bytes
        } else if (key == "VmRSS") {
            info.rss = std::stoull(value) * 1024;
        } else if (key == "RssAnon") {
            info.data = std::stoull(value) * 1024;
        } else if (key == "RssShmem") {
            info.shared = std::stoull(value) * 1024;
        } else if (key == "VmExe") {
            info.text = std::stoull(value) * 1024;
        }
    }

    return true;
}

static bool parse_io(const std::string& io_content, ProcessInfo& info) {
    std::istringstream iss(io_content);
    std::string line;

    while (std::getline(iss, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        size_t start = value.find_first_not_of(" \t");
        if (start != std::string::npos) {
            value = value.substr(start);
        }

        if (key == "read_bytes") {
            info.read_bytes = std::stoull(value);
        } else if (key == "write_bytes") {
            info.write_bytes = std::stoull(value);
        }
    }

    return true;
}

std::tuple<ProcessInfo, ErrorCode> get_process_info(int32_t pid) {
    ProcessInfo info;
    std::string proc_path = "/proc/" + std::to_string(pid);

    // Check if process exists
    struct stat st;
    if (stat(proc_path.c_str(), &st) != 0) {
        return {info, ErrorCode::PROCESS_NOT_FOUND};
    }

    // Read /proc/[pid]/stat
    std::string stat_content = read_file(proc_path + "/stat");
    if (stat_content.empty() || !parse_stat(stat_content, info)) {
        return {info, ErrorCode::READ_ERROR};
    }

    // Read /proc/[pid]/status for UID/GID
    std::string status_content = read_file(proc_path + "/status");
    if (!status_content.empty()) {
        parse_status(status_content, info);
    }

    // Read /proc/[pid]/cmdline
    std::string cmdline = read_file(proc_path + "/cmdline");
    // Replace null bytes with spaces
    std::replace(cmdline.begin(), cmdline.end(), '\0', ' ');
    if (!cmdline.empty() && cmdline.back() == ' ') {
        cmdline.pop_back();
    }
    info.cmdline = cmdline;

    // Read /proc/[pid]/exe
    info.exe = read_link(proc_path + "/exe");

    // Read /proc/[pid]/cwd
    info.cwd = read_link(proc_path + "/cwd");

    // Read /proc/[pid]/io (may require root)
    std::string io_content = read_file(proc_path + "/io");
    if (!io_content.empty()) {
        parse_io(io_content, info);
    }

    // Convert RSS from pages to bytes
    info.rss *= get_page_size();

    return {info, ErrorCode::OK};
}

ProcessListResult get_process_list(const FilterOptions& filter,
                                    const SortOptions& sort) {
    ProcessListResult result;

    DIR* dir = opendir("/proc");
    if (!dir) {
        result.error = ErrorCode::PERMISSION_DENIED;
        result.error_message = "Failed to open /proc";
        return result;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Check if entry is a PID directory (all digits)
        bool is_pid = true;
        for (const char* p = entry->d_name; *p; ++p) {
            if (*p < '0' || *p > '9') {
                is_pid = false;
                break;
            }
        }
        if (!is_pid) continue;

        int32_t pid = std::stoi(entry->d_name);

        // Apply PID filter early
        if (filter.pid.has_value() && pid != filter.pid.value()) {
            continue;
        }

        auto [info, err] = get_process_info(pid);
        if (err != ErrorCode::OK) {
            continue;  // Skip processes we can't read
        }

        // Apply filters
        if (filter.ppid.has_value() && info.ppid != filter.ppid.value()) {
            continue;
        }
        if (filter.uid.has_value() && info.uid != filter.uid.value()) {
            continue;
        }
        if (filter.user.has_value()) {
            auto uid = name_to_uid(filter.user.value());
            if (!uid.has_value() || info.uid != uid.value()) {
                continue;
            }
        }
        if (filter.name.has_value()) {
            if (info.name.find(filter.name.value()) == std::string::npos) {
                continue;
            }
        }
        if (filter.cmdline.has_value()) {
            if (info.cmdline.find(filter.cmdline.value()) == std::string::npos) {
                continue;
            }
        }
        if (filter.state.has_value() && info.state != filter.state.value()) {
            continue;
        }

        // Filter kernel threads if not requested
        if (!filter.include_kernel && info.cmdline.empty() && info.ppid == 2) {
            continue;
        }

        result.processes.push_back(info);
    }

    closedir(dir);

    // Sort results
    std::sort(result.processes.begin(), result.processes.end(),
        [&sort](const ProcessInfo& a, const ProcessInfo& b) {
            int cmp = 0;
            switch (sort.field) {
                case SortField::PID:
                    cmp = (a.pid < b.pid) ? -1 : (a.pid > b.pid) ? 1 : 0;
                    break;
                case SortField::PPID:
                    cmp = (a.ppid < b.ppid) ? -1 : (a.ppid > b.ppid) ? 1 : 0;
                    break;
                case SortField::NAME:
                    cmp = a.name.compare(b.name);
                    break;
                case SortField::USER:
                    cmp = (a.uid < b.uid) ? -1 : (a.uid > b.uid) ? 1 : 0;
                    break;
                case SortField::STATE:
                    cmp = static_cast<int>(a.state) - static_cast<int>(b.state);
                    break;
                case SortField::CPU:
                    cmp = (a.cpu_percent < b.cpu_percent) ? -1 :
                          (a.cpu_percent > b.cpu_percent) ? 1 : 0;
                    break;
                case SortField::MEMORY:
                    cmp = (a.rss < b.rss) ? -1 : (a.rss > b.rss) ? 1 : 0;
                    break;
                case SortField::START_TIME:
                    cmp = (a.start_time < b.start_time) ? -1 :
                          (a.start_time > b.start_time) ? 1 : 0;
                    break;
                case SortField::THREADS:
                    cmp = (a.threads < b.threads) ? -1 :
                          (a.threads > b.threads) ? 1 : 0;
                    break;
            }
            return sort.descending ? (cmp > 0) : (cmp < 0);
        });

    return result;
}

std::vector<ProcessTreeNode> get_process_tree(const FilterOptions& filter) {
    // First, get all processes
    auto result = get_process_list(filter, SortOptions());

    // Build parent-child map
    std::map<int32_t, ProcessInfo> by_pid;
    std::map<int32_t, std::vector<int32_t>> children;

    for (const auto& proc : result.processes) {
        by_pid[proc.pid] = proc;
        children[proc.ppid].push_back(proc.pid);
    }

    // Build tree recursively
    std::function<ProcessTreeNode(int32_t)> build_node = [&](int32_t pid) {
        ProcessTreeNode node;
        auto it = by_pid.find(pid);
        if (it != by_pid.end()) {
            node.info = it->second;
        }

        for (int32_t child_pid : children[pid]) {
            node.children.push_back(build_node(child_pid));
        }

        return node;
    };

    // Find root processes (PPID = 0 or PPID not in list)
    std::vector<ProcessTreeNode> roots;
    for (const auto& proc : result.processes) {
        if (proc.ppid == 0 || by_pid.find(proc.ppid) == by_pid.end()) {
            roots.push_back(build_node(proc.pid));
        }
    }

    return roots;
}

std::vector<ProcessInfo> get_children(int32_t pid, bool recursive) {
    FilterOptions filter;
    filter.ppid = pid;

    auto result = get_process_list(filter, SortOptions());

    if (!recursive) {
        return result.processes;
    }

    // Recursively get grandchildren
    std::vector<ProcessInfo> all;
    all = result.processes;

    for (const auto& child : result.processes) {
        auto grandchildren = get_children(child.pid, true);
        all.insert(all.end(), grandchildren.begin(), grandchildren.end());
    }

    return all;
}

bool process_exists(int32_t pid) {
    std::string path = "/proc/" + std::to_string(pid);
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

int32_t get_current_pid() {
    return getpid();
}

int32_t get_parent_pid() {
    return getppid();
}

std::string uid_to_name(uint32_t uid) {
    struct passwd* pw = getpwuid(uid);
    if (pw) {
        return pw->pw_name;
    }
    return std::to_string(uid);
}

std::optional<uint32_t> name_to_uid(const std::string& name) {
    struct passwd* pw = getpwnam(name.c_str());
    if (pw) {
        return pw->pw_uid;
    }
    return std::nullopt;
}

std::string gid_to_name(uint32_t gid) {
    struct group* gr = getgrgid(gid);
    if (gr) {
        return gr->gr_name;
    }
    return std::to_string(gid);
}

int get_cpu_count() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

double get_uptime() {
    std::ifstream uptime_file("/proc/uptime");
    if (!uptime_file.is_open()) {
        return 0.0;
    }

    double uptime;
    uptime_file >> uptime;
    return uptime;
}

long get_clock_ticks() {
    return sysconf(_SC_CLK_TCK);
}

long get_page_size() {
    return sysconf(_SC_PAGESIZE);
}

uint64_t get_total_memory() {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        return 0;
    }

    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0) {
            std::istringstream iss(line.substr(9));
            uint64_t mem_kb;
            iss >> mem_kb;
            return mem_kb * 1024;  // Convert kB to bytes
        }
    }

    return 0;
}

#elif defined(__APPLE__)

// macOS implementation using sysctl and libproc
// Simplified version - full implementation would use:
// - sysctl with CTL_KERN/KERN_PROC
// - libproc functions like proc_listpids, proc_pidinfo

std::tuple<ProcessInfo, ErrorCode> get_process_info(int32_t pid) {
    ProcessInfo info;
    // TODO: Implement using proc_pidinfo and related functions
    return {info, ErrorCode::SYSTEM_ERROR};
}

ProcessListResult get_process_list(const FilterOptions& filter,
                                    const SortOptions& sort) {
    ProcessListResult result;
    // TODO: Implement using sysctl(CTL_KERN, KERN_PROC, ...)
    result.error = ErrorCode::SYSTEM_ERROR;
    result.error_message = "macOS implementation not yet complete";
    return result;
}

std::vector<ProcessTreeNode> get_process_tree(const FilterOptions& filter) {
    return {};
}

std::vector<ProcessInfo> get_children(int32_t pid, bool recursive) {
    return {};
}

bool process_exists(int32_t pid) {
    return kill(pid, 0) == 0;
}

int32_t get_current_pid() {
    return getpid();
}

int32_t get_parent_pid() {
    return getppid();
}

std::string uid_to_name(uint32_t uid) {
    struct passwd* pw = getpwuid(uid);
    return pw ? pw->pw_name : std::to_string(uid);
}

std::optional<uint32_t> name_to_uid(const std::string& name) {
    struct passwd* pw = getpwnam(name.c_str());
    return pw ? std::optional<uint32_t>(pw->pw_uid) : std::nullopt;
}

std::string gid_to_name(uint32_t gid) {
    struct group* gr = getgrgid(gid);
    return gr ? gr->gr_name : std::to_string(gid);
}

int get_cpu_count() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

double get_uptime() {
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    if (sysctl(mib, 2, &boottime, &len, NULL, 0) == 0) {
        time_t now = time(NULL);
        return difftime(now, boottime.tv_sec);
    }
    return 0.0;
}

long get_clock_ticks() {
    return sysconf(_SC_CLK_TCK);
}

long get_page_size() {
    return sysconf(_SC_PAGESIZE);
}

uint64_t get_total_memory() {
    int64_t memsize;
    size_t len = sizeof(memsize);
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    if (sysctl(mib, 2, &memsize, &len, NULL, 0) == 0) {
        return memsize;
    }
    return 0;
}

#elif defined(_WIN32)

// Windows implementation using Windows API
// TODO: Full implementation using:
// - CreateToolhelp32Snapshot
// - Process32First/Process32Next
// - OpenProcess/GetProcessMemoryInfo

std::tuple<ProcessInfo, ErrorCode> get_process_info(int32_t pid) {
    ProcessInfo info;
    // TODO: Implement using OpenProcess and related functions
    return {info, ErrorCode::SYSTEM_ERROR};
}

ProcessListResult get_process_list(const FilterOptions& filter,
                                    const SortOptions& sort) {
    ProcessListResult result;
    // TODO: Implement using CreateToolhelp32Snapshot
    result.error = ErrorCode::SYSTEM_ERROR;
    result.error_message = "Windows implementation not yet complete";
    return result;
}

std::vector<ProcessTreeNode> get_process_tree(const FilterOptions& filter) {
    return {};
}

std::vector<ProcessInfo> get_children(int32_t pid, bool recursive) {
    return {};
}

bool process_exists(int32_t pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess != NULL) {
        CloseHandle(hProcess);
        return true;
    }
    return false;
}

int32_t get_current_pid() {
    return GetCurrentProcessId();
}

int32_t get_parent_pid() {
    // Windows doesn't have a direct equivalent
    return 0;
}

std::string uid_to_name(uint32_t uid) {
    return std::to_string(uid);
}

std::optional<uint32_t> name_to_uid(const std::string& name) {
    return std::nullopt;
}

std::string gid_to_name(uint32_t gid) {
    return std::to_string(gid);
}

int get_cpu_count() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return sysInfo.dwNumberOfProcessors;
}

double get_uptime() {
    return GetTickCount64() / 1000.0;
}

long get_clock_ticks() {
    return 100;  // Windows uses 100 ticks per second
}

long get_page_size() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return sysInfo.dwPageSize;
}

uint64_t get_total_memory() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(memInfo);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return memInfo.ullTotalPhys;
    }
    return 0;
}

#endif

} // namespace aps
} // namespace aria
