/**
 * Windows Bootstrap Protocol Implementation
 * 
 * Enables Hex-Stream topology on Windows by mapping HANDLEs to logical indices.
 */

#ifdef _WIN32

#include "platform/windows_bootstrap.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace ariash {
namespace platform {

// =============================================================================
// WindowsHandleMap Implementation
// =============================================================================

std::wstring WindowsHandleMap::serialize() const {
    std::wostringstream oss;
    
    // Only serialize streams 3-5 (stddbg, stddati, stddato)
    // Streams 0-2 use standard STARTUPINFO fields
    bool first = true;
    
    if (hStdDbg != INVALID_HANDLE_VALUE) {
        oss << L"3:0x" << std::hex << std::uppercase 
            << reinterpret_cast<uintptr_t>(hStdDbg);
        first = false;
    }
    
    if (hStdDatI != INVALID_HANDLE_VALUE) {
        if (!first) oss << L";";
        oss << L"4:0x" << std::hex << std::uppercase 
            << reinterpret_cast<uintptr_t>(hStdDatI);
        first = false;
    }
    
    if (hStdDatO != INVALID_HANDLE_VALUE) {
        if (!first) oss << L";";
        oss << L"5:0x" << std::hex << std::uppercase 
            << reinterpret_cast<uintptr_t>(hStdDatO);
    }
    
    return oss.str();
}

bool WindowsHandleMap::parse(const std::wstring& mapString) {
    if (mapString.empty()) {
        return false;
    }
    
    // Split by semicolon
    std::wistringstream stream(mapString);
    std::wstring pair;
    
    while (std::getline(stream, pair, L';')) {
        // Split by colon
        size_t colonPos = pair.find(L':');
        if (colonPos == std::wstring::npos) {
            continue;  // Malformed pair, skip
        }
        
        std::wstring indexStr = pair.substr(0, colonPos);
        std::wstring handleStr = pair.substr(colonPos + 1);
        
        // Parse index
        int index = _wtoi(indexStr.c_str());
        
        // Parse handle (hex value)
        wchar_t* endPtr = nullptr;
        uintptr_t handleValue = wcstoull(handleStr.c_str(), &endPtr, 16);
        HANDLE handle = reinterpret_cast<HANDLE>(handleValue);
        
        // Map to appropriate field
        switch (index) {
            case 0: hStdIn = handle; break;
            case 1: hStdOut = handle; break;
            case 2: hStdErr = handle; break;
            case 3: hStdDbg = handle; break;
            case 4: hStdDatI = handle; break;
            case 5: hStdDatO = handle; break;
            default:
                // Unknown index, skip
                break;
        }
    }
    
    return true;
}

bool WindowsHandleMap::validateHandles() const {
    // Check streams 3-5 (others validated via STARTUPINFO)
    DWORD flags;
    
    if (hStdDbg != INVALID_HANDLE_VALUE) {
        if (!GetHandleInformation(hStdDbg, &flags)) {
            return false;
        }
    }
    
    if (hStdDatI != INVALID_HANDLE_VALUE) {
        if (!GetHandleInformation(hStdDatI, &flags)) {
            return false;
        }
    }
    
    if (hStdDatO != INVALID_HANDLE_VALUE) {
        if (!GetHandleInformation(hStdDatO, &flags)) {
            return false;
        }
    }
    
    return true;
}

// =============================================================================
// WindowsBootstrap Implementation
// =============================================================================

WindowsBootstrap::WindowsBootstrap()
    : attributeList_(nullptr)
{
    ZeroMemory(&processInfo_, sizeof(processInfo_));
}

WindowsBootstrap::~WindowsBootstrap() {
    closeAll();
    
    if (attributeList_) {
        DeleteProcThreadAttributeList(attributeList_);
        HeapFree(GetProcessHeap(), 0, attributeList_);
        attributeList_ = nullptr;
    }
}

bool WindowsBootstrap::createPipes() {
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;  // CRITICAL: Must be inheritable
    saAttr.lpSecurityDescriptor = NULL;
    
    // Create pipe for stdin (Stream 0)
    if (!CreatePipe(&childHandles_.hStdIn, &parentHandles_.hStdIn, &saAttr, 0)) {
        return false;
    }
    SetHandleInformation(parentHandles_.hStdIn, HANDLE_FLAG_INHERIT, 0);  // Parent write not inherited
    
    // Create pipe for stdout (Stream 1)
    if (!CreatePipe(&parentHandles_.hStdOut, &childHandles_.hStdOut, &saAttr, 0)) {
        return false;
    }
    SetHandleInformation(parentHandles_.hStdOut, HANDLE_FLAG_INHERIT, 0);  // Parent read not inherited
    
    // Create pipe for stderr (Stream 2)
    if (!CreatePipe(&parentHandles_.hStdErr, &childHandles_.hStdErr, &saAttr, 0)) {
        return false;
    }
    SetHandleInformation(parentHandles_.hStdErr, HANDLE_FLAG_INHERIT, 0);
    
    // Create pipe for stddbg (Stream 3) - child writes debug logs
    if (!CreatePipe(&parentHandles_.hStdDbg, &childHandles_.hStdDbg, &saAttr, 0)) {
        return false;
    }
    SetHandleInformation(parentHandles_.hStdDbg, HANDLE_FLAG_INHERIT, 0);
    
    // Create pipe for stddati (Stream 4) - child reads binary data
    if (!CreatePipe(&childHandles_.hStdDatI, &parentHandles_.hStdDatI, &saAttr, 0)) {
        return false;
    }
    SetHandleInformation(parentHandles_.hStdDatI, HANDLE_FLAG_INHERIT, 0);
    
    // Create pipe for stddato (Stream 5) - child writes binary data
    if (!CreatePipe(&parentHandles_.hStdDatO, &childHandles_.hStdDatO, &saAttr, 0)) {
        return false;
    }
    SetHandleInformation(parentHandles_.hStdDatO, HANDLE_FLAG_INHERIT, 0);
    
    return true;
}

bool WindowsBootstrap::createStartupInfo(STARTUPINFOEXW& si) {
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = childHandles_.hStdIn;
    si.StartupInfo.hStdOutput = childHandles_.hStdOut;
    si.StartupInfo.hStdError = childHandles_.hStdErr;
    
    // Build handle whitelist for STARTUPINFOEX
    std::vector<HANDLE> handleList;
    handleList.push_back(childHandles_.hStdIn);
    handleList.push_back(childHandles_.hStdOut);
    handleList.push_back(childHandles_.hStdErr);
    
    if (childHandles_.hStdDbg != INVALID_HANDLE_VALUE) {
        handleList.push_back(childHandles_.hStdDbg);
    }
    if (childHandles_.hStdDatI != INVALID_HANDLE_VALUE) {
        handleList.push_back(childHandles_.hStdDatI);
    }
    if (childHandles_.hStdDatO != INVALID_HANDLE_VALUE) {
        handleList.push_back(childHandles_.hStdDatO);
    }
    
    // Initialize attribute list
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &size);
    
    attributeList_ = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, size);
    if (!attributeList_) {
        return false;
    }
    
    if (!InitializeProcThreadAttributeList(attributeList_, 1, 0, &size)) {
        HeapFree(GetProcessHeap(), 0, attributeList_);
        attributeList_ = nullptr;
        return false;
    }
    
    // Add handle whitelist
    if (!UpdateProcThreadAttribute(
            attributeList_,
            0,
            PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            handleList.data(),
            handleList.size() * sizeof(HANDLE),
            NULL,
            NULL)) {
        DeleteProcThreadAttributeList(attributeList_);
        HeapFree(GetProcessHeap(), 0, attributeList_);
        attributeList_ = nullptr;
        return false;
    }
    
    si.lpAttributeList = attributeList_;
    
    return true;
}

std::vector<wchar_t> WindowsBootstrap::buildEnvironmentBlock(const WindowsHandleMap& handles) {
    // Get current environment
    wchar_t* envBlock = GetEnvironmentStringsW();
    if (!envBlock) {
        return std::vector<wchar_t>();
    }
    
    // Copy existing environment
    std::vector<wchar_t> newEnv;
    wchar_t* p = envBlock;
    while (*p) {
        size_t len = wcslen(p);
        newEnv.insert(newEnv.end(), p, p + len + 1);
        p += len + 1;
    }
    
    FreeEnvironmentStringsW(envBlock);
    
    // Add __ARIA_FD_MAP
    std::wstring fdMap = L"__ARIA_FD_MAP=" + handles.serialize();
    newEnv.insert(newEnv.end(), fdMap.begin(), fdMap.end());
    newEnv.push_back(L'\0');
    
    // Double null terminator
    newEnv.push_back(L'\0');
    
    return newEnv;
}

std::wstring WindowsBootstrap::buildCommandLineWithFlag(
    const std::wstring& cmdLine,
    const WindowsHandleMap& handles)
{
    std::wstring result = cmdLine;
    result += L" --aria-fd-map=";
    result += handles.serialize();
    return result;
}

HANDLE WindowsBootstrap::launchProcess(const std::wstring& commandLine, bool useEnvVar) {
    STARTUPINFOEXW si;
    if (!createStartupInfo(si)) {
        return INVALID_HANDLE_VALUE;
    }
    
    // Build environment or command line
    std::vector<wchar_t> envBlock;
    std::wstring cmdLine = commandLine;
    
    if (useEnvVar) {
        envBlock = buildEnvironmentBlock(childHandles_);
    } else {
        cmdLine = buildCommandLineWithFlag(commandLine, childHandles_);
    }
    
    // Launch process
    wchar_t* cmdLineBuf = _wcsdup(cmdLine.c_str());
    
    BOOL success = CreateProcessW(
        NULL,                                // Application name
        cmdLineBuf,                          // Command line
        NULL,                                // Process security
        NULL,                                // Thread security
        TRUE,                                // Inherit handles
        EXTENDED_STARTUPINFO_PRESENT,        // Creation flags
        envBlock.empty() ? NULL : envBlock.data(),  // Environment
        NULL,                                // Current directory
        &si.StartupInfo,                     // Startup info
        &processInfo_                        // Process info
    );
    
    free(cmdLineBuf);
    
    if (!success) {
        return INVALID_HANDLE_VALUE;
    }
    
    return processInfo_.hProcess;
}

void WindowsBootstrap::closeParentHandles() {
    if (parentHandles_.hStdIn != INVALID_HANDLE_VALUE) {
        CloseHandle(parentHandles_.hStdIn);
        parentHandles_.hStdIn = INVALID_HANDLE_VALUE;
    }
    if (parentHandles_.hStdOut != INVALID_HANDLE_VALUE) {
        CloseHandle(parentHandles_.hStdOut);
        parentHandles_.hStdOut = INVALID_HANDLE_VALUE;
    }
    if (parentHandles_.hStdErr != INVALID_HANDLE_VALUE) {
        CloseHandle(parentHandles_.hStdErr);
        parentHandles_.hStdErr = INVALID_HANDLE_VALUE;
    }
    if (parentHandles_.hStdDbg != INVALID_HANDLE_VALUE) {
        CloseHandle(parentHandles_.hStdDbg);
        parentHandles_.hStdDbg = INVALID_HANDLE_VALUE;
    }
    if (parentHandles_.hStdDatI != INVALID_HANDLE_VALUE) {
        CloseHandle(parentHandles_.hStdDatI);
        parentHandles_.hStdDatI = INVALID_HANDLE_VALUE;
    }
    if (parentHandles_.hStdDatO != INVALID_HANDLE_VALUE) {
        CloseHandle(parentHandles_.hStdDatO);
        parentHandles_.hStdDatO = INVALID_HANDLE_VALUE;
    }
}

void WindowsBootstrap::closeAll() {
    closeParentHandles();
    
    // Close child handles
    if (childHandles_.hStdIn != INVALID_HANDLE_VALUE) {
        CloseHandle(childHandles_.hStdIn);
        childHandles_.hStdIn = INVALID_HANDLE_VALUE;
    }
    if (childHandles_.hStdOut != INVALID_HANDLE_VALUE) {
        CloseHandle(childHandles_.hStdOut);
        childHandles_.hStdOut = INVALID_HANDLE_VALUE;
    }
    if (childHandles_.hStdErr != INVALID_HANDLE_VALUE) {
        CloseHandle(childHandles_.hStdErr);
        childHandles_.hStdErr = INVALID_HANDLE_VALUE;
    }
    if (childHandles_.hStdDbg != INVALID_HANDLE_VALUE) {
        CloseHandle(childHandles_.hStdDbg);
        childHandles_.hStdDbg = INVALID_HANDLE_VALUE;
    }
    if (childHandles_.hStdDatI != INVALID_HANDLE_VALUE) {
        CloseHandle(childHandles_.hStdDatI);
        childHandles_.hStdDatI = INVALID_HANDLE_VALUE;
    }
    if (childHandles_.hStdDatO != INVALID_HANDLE_VALUE) {
        CloseHandle(childHandles_.hStdDatO);
        childHandles_.hStdDatO = INVALID_HANDLE_VALUE;
    }
    
    // Close process handles
    if (processInfo_.hProcess != NULL) {
        CloseHandle(processInfo_.hProcess);
        processInfo_.hProcess = NULL;
    }
    if (processInfo_.hThread != NULL) {
        CloseHandle(processInfo_.hThread);
        processInfo_.hThread = NULL;
    }
}

// =============================================================================
// WindowsHandleMapConsumer Implementation (Runtime-Side)
// =============================================================================

WindowsHandleMap WindowsHandleMapConsumer::retrieveHandleMap(bool envVarFirst) {
    if (envVarFirst) {
        WindowsHandleMap map = parseFromEnvironment();
        if (map.validateHandles()) {
            return map;
        }
        
        // Fallback to command line
        map = parseFromCommandLine();
        if (map.validateHandles()) {
            return map;
        }
    } else {
        WindowsHandleMap map = parseFromCommandLine();
        if (map.validateHandles()) {
            return map;
        }
        
        // Fallback to environment
        map = parseFromEnvironment();
        if (map.validateHandles()) {
            return map;
        }
    }
    
    // Return empty map (all INVALID_HANDLE_VALUE)
    return WindowsHandleMap();
}

WindowsHandleMap WindowsHandleMapConsumer::parseFromEnvironment() {
    WindowsHandleMap map;
    
    wchar_t buffer[2048];
    DWORD result = GetEnvironmentVariableW(L"__ARIA_FD_MAP", buffer, sizeof(buffer) / sizeof(wchar_t));
    
    if (result > 0 && result < sizeof(buffer) / sizeof(wchar_t)) {
        map.parse(buffer);
    }
    
    return map;
}

WindowsHandleMap WindowsHandleMapConsumer::parseFromCommandLine() {
    WindowsHandleMap map;
    
    LPWSTR cmdLine = GetCommandLineW();
    if (!cmdLine) {
        return map;
    }
    
    std::wstring cmdLineStr(cmdLine);
    
    // Find --aria-fd-map=
    size_t pos = cmdLineStr.find(L"--aria-fd-map=");
    if (pos == std::wstring::npos) {
        return map;
    }
    
    // Extract value (until next space or end)
    size_t valueStart = pos + 14;  // Length of "--aria-fd-map="
    size_t valueEnd = cmdLineStr.find(L' ', valueStart);
    if (valueEnd == std::wstring::npos) {
        valueEnd = cmdLineStr.length();
    }
    
    std::wstring value = cmdLineStr.substr(valueStart, valueEnd - valueStart);
    map.parse(value);
    
    // TODO: Remove flag from argv (would require reconstructing argv)
    
    return map;
}

bool WindowsHandleMapConsumer::initializeStreams(const WindowsHandleMap& handles) {
    // This would integrate with actual Aria runtime
    // For now, just validate
    return handles.validateHandles();
}

} // namespace platform
} // namespace ariash

#endif // _WIN32
