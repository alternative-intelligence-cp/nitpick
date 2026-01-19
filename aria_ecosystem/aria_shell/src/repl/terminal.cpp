/**
 * Platform Terminal Implementation
 * 
 * Cross-platform raw terminal mode for modal multi-line input.
 */

#include "repl/terminal.hpp"
#include <iostream>
#include <cstring>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#else
#include <windows.h>
#endif

namespace ariash {
namespace repl {

// =============================================================================
// Constructor / Destructor
// =============================================================================

PlatformTerminal::PlatformTerminal()
    : protocolLevel_(ProtocolLevel::LEGACY)
    , rawModeActive_(false)
#ifndef _WIN32
    , termiosValid_(false)
#else
    , hStdin_(INVALID_HANDLE_VALUE)
    , hStdout_(INVALID_HANDLE_VALUE)
    , modesValid_(false)
#endif
{
}

PlatformTerminal::~PlatformTerminal() {
    if (rawModeActive_) {
        restoreMode();
    }
}

// =============================================================================
// POSIX Implementation
// =============================================================================

#ifndef _WIN32

bool PlatformTerminal::enterRawMode() {
    // Capture original terminal state (CRITICAL for restoration)
    if (tcgetattr(STDIN_FILENO, &originalTermios_) < 0) {
        return false;
    }
    termiosValid_ = true;
    
    // Create modified termios for raw mode
    struct termios raw = originalTermios_;
    
    // Input flags (Table 1 from shell_03 spec)
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    
    // Output flags - KEEP OPOST enabled so \n becomes \r\n
    // (This allows normal printf/cout to work correctly)
    // raw.c_oflag &= ~(OPOST);  // DON'T disable output processing
    
    // Control flags (8-bit clean for UTF-8)
    raw.c_cflag |= (CS8);
    
    // Local flags (disable canonical, echo, signals, extensions)
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    
    // Control characters: non-blocking read with 100ms timeout
    raw.c_cc[VMIN] = 0;   // Don't wait for minimum bytes
    raw.c_cc[VTIME] = 1;  // 100ms timeout (1 decisecond)
    
    // Apply raw mode, flushing pending input
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        return false;
    }
    
    rawModeActive_ = true;
    return true;
}

void PlatformTerminal::restoreMode() {
    if (!rawModeActive_) return;
    
    if (termiosValid_) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios_);
    }
    
    rawModeActive_ = false;
}

ProtocolLevel PlatformTerminal::negotiateProtocol() {
    // Try Kitty Keyboard Protocol first
    if (detectKittyProtocol()) {
        protocolLevel_ = ProtocolLevel::KITTY_PROGRESSIVE;
        return protocolLevel_;
    }
    
    // Fallback to XTerm modifyOtherKeys
    if (detectXTermModifyKeys()) {
        protocolLevel_ = ProtocolLevel::XTERM_MODIFY_KEYS;
        return protocolLevel_;
    }
    
    // Legacy mode (Alt+Enter fallback)
    protocolLevel_ = ProtocolLevel::LEGACY;
    return protocolLevel_;
}

bool PlatformTerminal::detectKittyProtocol() {
    // Query Kitty protocol support: ESC [?u
    write(STDOUT_FILENO, "\x1B[?u", 4);
    
    // Wait for response with timeout
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    
    if (poll(&pfd, 1, 200) <= 0) {
        return false;  // Timeout or error
    }
    
    // Read response
    char buffer[64];
    ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    if (n <= 0) return false;
    
    buffer[n] = '\0';
    
    // Check for Kitty response: ESC [?<flags>u
    // Enable progressive enhancement: ESC [>1u
    if (strstr(buffer, "[?") != nullptr) {
        write(STDOUT_FILENO, "\x1B[>1u", 5);
        return true;
    }
    
    return false;
}

bool PlatformTerminal::detectXTermModifyKeys() {
    // Enable XTerm modifyOtherKeys mode 2
    // CSI > 4 ; 2 m
    write(STDOUT_FILENO, "\x1B[>4;2m", 8);
    
    // XTerm doesn't always respond, so we optimistically assume success
    // This is safe because it's a widely supported extension
    return true;
}

std::optional<KeyEvent> PlatformTerminal::readEvent() {
    char buf[8];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    
    if (n <= 0) {
        return std::nullopt;  // Timeout or error
    }
    
    // Single byte: simple character or control code
    if (n == 1) {
        unsigned char c = buf[0];
        
        // Control characters
        if (c == 0x03) return KeyEvent{KeyType::CTRL_C};
        if (c == 0x04) return KeyEvent{KeyType::CTRL_D};
        if (c == 0x0C) return KeyEvent{KeyType::CTRL_L};
        if (c == 0x1A) return KeyEvent{KeyType::CTRL_Z};
        if (c == 0x0D || c == 0x0A) return KeyEvent{KeyType::ENTER};
        if (c == 0x7F || c == 0x08) return KeyEvent{KeyType::BACKSPACE};
        if (c == 0x09) return KeyEvent{KeyType::TAB};
        if (c == 0x1B) {
            // Standalone ESC (wait to see if sequence follows)
            struct pollfd pfd;
            pfd.fd = STDIN_FILENO;
            pfd.events = POLLIN;
            
            if (poll(&pfd, 1, 50) <= 0) {
                // No follow-up: genuine ESC key
                return KeyEvent{KeyType::ESCAPE};
            }
            
            // Sequence follows, recurse to parse it
            return parseEscapeSequence();
        }
        
        // Printable ASCII or UTF-8 start byte
        if (c >= 32 && c < 127) {
            return KeyEvent{KeyType::CHARACTER, KeyModifiers::NONE, static_cast<char32_t>(c)};
        }
        
        // UTF-8 multi-byte (TODO: full UTF-8 decoder)
        if (c >= 0x80) {
            return KeyEvent{KeyType::CHARACTER, KeyModifiers::NONE, static_cast<char32_t>(c)};
        }
    }
    
    // Multi-byte sequence: likely escape code
    if (n > 1 && buf[0] == 0x1B) {
        std::string seq(buf, n);
        return parseAnsiSequence(seq);
    }
    
    return std::nullopt;
}

std::optional<KeyEvent> PlatformTerminal::parseEscapeSequence() {
    char buf[32];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    
    if (n <= 0) {
        return KeyEvent{KeyType::ESCAPE};  // Timeout: genuine ESC
    }
    
    std::string seq = "\x1B";
    seq.append(buf, n);
    
    // Check protocol-specific sequences
    if (protocolLevel_ == ProtocolLevel::KITTY_PROGRESSIVE) {
        auto kittyEvent = parseKittySequence(seq);
        if (kittyEvent) return kittyEvent;
    }
    
    if (protocolLevel_ == ProtocolLevel::XTERM_MODIFY_KEYS) {
        auto xtermEvent = parseXTermSequence(seq);
        if (xtermEvent) return xtermEvent;
    }
    
    // Standard ANSI sequences
    return parseAnsiSequence(seq);
}

std::optional<KeyEvent> PlatformTerminal::parseAnsiSequence(const std::string& seq) {
    // Arrow keys
    if (seq == "\x1B[A") return KeyEvent{KeyType::ARROW_UP};
    if (seq == "\x1B[B") return KeyEvent{KeyType::ARROW_DOWN};
    if (seq == "\x1B[C") return KeyEvent{KeyType::ARROW_RIGHT};
    if (seq == "\x1B[D") return KeyEvent{KeyType::ARROW_LEFT};
    
    // Home/End
    if (seq == "\x1B[H" || seq == "\x1B[1~") return KeyEvent{KeyType::HOME};
    if (seq == "\x1B[F" || seq == "\x1B[4~") return KeyEvent{KeyType::END};
    
    // Page Up/Down
    if (seq == "\x1B[5~") return KeyEvent{KeyType::PAGE_UP};
    if (seq == "\x1B[6~") return KeyEvent{KeyType::PAGE_DOWN};
    
    // Delete
    if (seq == "\x1B[3~") return KeyEvent{KeyType::DELETE};
    
    // Function keys
    if (seq == "\x1BOP") return KeyEvent{KeyType::F1};
    if (seq == "\x1BOQ") return KeyEvent{KeyType::F2};
    if (seq == "\x1BOR") return KeyEvent{KeyType::F3};
    if (seq == "\x1BOS") return KeyEvent{KeyType::F4};
    if (seq == "\x1B[15~") return KeyEvent{KeyType::F5};
    if (seq == "\x1B[17~") return KeyEvent{KeyType::F6};
    if (seq == "\x1B[18~") return KeyEvent{KeyType::F7};
    if (seq == "\x1B[19~") return KeyEvent{KeyType::F8};
    if (seq == "\x1B[20~") return KeyEvent{KeyType::F9};
    if (seq == "\x1B[21~") return KeyEvent{KeyType::F10};
    if (seq == "\x1B[23~") return KeyEvent{KeyType::F11};
    if (seq == "\x1B[24~") return KeyEvent{KeyType::F12};
    
    // Alt+Enter fallback (ESC + Enter)
    if (seq == "\x1B\x0D" || seq == "\x1B\x0A") {
        return KeyEvent{KeyType::ALT_ENTER};
    }
    
    return std::nullopt;
}

std::optional<KeyEvent> PlatformTerminal::parseKittySequence(const std::string& seq) {
    // Kitty progressive enhancement format:
    // CSI <unicode>;<modifiers>u
    // Example: Ctrl+Enter = CSI 13;5u
    
    if (seq.length() < 5 || seq[0] != '\x1B' || seq[1] != '[') {
        return std::nullopt;
    }
    
    if (seq.back() != 'u') {
        return std::nullopt;
    }
    
    // Parse parameters
    size_t semicolon = seq.find(';', 2);
    if (semicolon == std::string::npos) {
        return std::nullopt;
    }
    
    int codepoint = std::stoi(seq.substr(2, semicolon - 2));
    int modBits = std::stoi(seq.substr(semicolon + 1, seq.length() - semicolon - 2));
    
    // Decode modifiers (Kitty format: 1=none, 2=shift, 3=alt, 5=ctrl, etc.)
    KeyModifiers mods = KeyModifiers::NONE;
    if (modBits & 1) mods = mods | KeyModifiers::SHIFT;
    if (modBits & 2) mods = mods | KeyModifiers::ALT;
    if (modBits & 4) mods = mods | KeyModifiers::CTRL;
    
    // Ctrl+Enter (codepoint 13 with ctrl modifier)
    if (codepoint == 13 && (mods & KeyModifiers::CTRL)) {
        return KeyEvent{KeyType::CTRL_ENTER, mods};
    }
    
    // Map other special keys
    KeyType type = (codepoint == 13) ? KeyType::ENTER : KeyType::CHARACTER;
    
    return KeyEvent{type, mods, static_cast<char32_t>(codepoint)};
}

std::optional<KeyEvent> PlatformTerminal::parseXTermSequence(const std::string& seq) {
    // XTerm modifyOtherKeys format:
    // CSI 27;<modifiers>;<codepoint>~
    // Example: Ctrl+Enter = CSI 27;5;13~
    
    if (seq.length() < 8 || seq[0] != '\x1B' || seq[1] != '[') {
        return std::nullopt;
    }
    
    if (!seq.starts_with("\x1B[27;")) {
        return std::nullopt;
    }
    
    // Parse parameters
    size_t semi1 = 6;  // After "\x1B[27;"
    size_t semi2 = seq.find(';', semi1);
    if (semi2 == std::string::npos) return std::nullopt;
    
    int modBits = std::stoi(seq.substr(semi1, semi2 - semi1));
    int codepoint = std::stoi(seq.substr(semi2 + 1, seq.length() - semi2 - 2));
    
    // Decode modifiers (XTerm format: 1=shift, 2=alt, 5=ctrl)
    KeyModifiers mods = KeyModifiers::NONE;
    if (modBits == 5) mods = KeyModifiers::CTRL;
    if (modBits == 3) mods = KeyModifiers::ALT;
    if (modBits == 2) mods = KeyModifiers::SHIFT;
    
    // Ctrl+Enter
    if (codepoint == 13 && (mods & KeyModifiers::CTRL)) {
        return KeyEvent{KeyType::CTRL_ENTER, mods};
    }
    
    return KeyEvent{KeyType::CHARACTER, mods, static_cast<char32_t>(codepoint)};
}

std::pair<int, int> PlatformTerminal::getSize() const {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0) {
        return {80, 24};  // Default fallback
    }
    return {ws.ws_col, ws.ws_row};
}

bool PlatformTerminal::isUnicodeSupported() const {
    // Check LANG environment variable
    const char* lang = getenv("LANG");
    if (lang && strstr(lang, "UTF-8")) {
        return true;
    }
    return false;
}

#endif // !_WIN32

// =============================================================================
// Windows Implementation
// =============================================================================

#ifdef _WIN32

bool PlatformTerminal::enterRawMode() {
    hStdin_ = GetStdHandle(STD_INPUT_HANDLE);
    hStdout_ = GetStdHandle(STD_OUTPUT_HANDLE);
    
    if (hStdin_ == INVALID_HANDLE_VALUE || hStdout_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Save original modes
    if (!GetConsoleMode(hStdin_, &originalInputMode_) ||
        !GetConsoleMode(hStdout_, &originalOutputMode_)) {
        return false;
    }
    modesValid_ = true;
    
    // Configure input mode for raw events
    DWORD inputMode = 0;
    inputMode |= ENABLE_WINDOW_INPUT;        // Window resize events
    inputMode |= ENABLE_MOUSE_INPUT;         // Future: mouse support
    inputMode &= ~ENABLE_LINE_INPUT;         // Disable line buffering
    inputMode &= ~ENABLE_ECHO_INPUT;         // Disable echo
    inputMode &= ~ENABLE_PROCESSED_INPUT;    // Disable Ctrl+C processing
    
    // Try to enable Virtual Terminal Input (Windows 10+)
    inputMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    
    if (!SetConsoleMode(hStdin_, inputMode)) {
        return false;
    }
    
    // Configure output mode for VT sequences
    DWORD outputMode = originalOutputMode_;
    outputMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    outputMode |= DISABLE_NEWLINE_AUTO_RETURN;
    
    SetConsoleMode(hStdout_, outputMode);  // Soft-fail OK for legacy systems
    
    rawModeActive_ = true;
    return true;
}

void PlatformTerminal::restoreMode() {
    if (!rawModeActive_) return;
    
    if (modesValid_) {
        SetConsoleMode(hStdin_, originalInputMode_);
        SetConsoleMode(hStdout_, originalOutputMode_);
    }
    
    rawModeActive_ = false;
}

ProtocolLevel PlatformTerminal::negotiateProtocol() {
    // Windows with VT support acts like a modern terminal
    DWORD mode;
    if (GetConsoleMode(hStdin_, &mode) && (mode & ENABLE_VIRTUAL_TERMINAL_INPUT)) {
        // Try Kitty protocol
        if (detectKittyProtocol()) {
            protocolLevel_ = ProtocolLevel::KITTY_PROGRESSIVE;
            return protocolLevel_;
        }
        
        // Assume XTerm compatibility
        protocolLevel_ = ProtocolLevel::XTERM_MODIFY_KEYS;
        return protocolLevel_;
    }
    
    // Legacy Windows console uses KEY_EVENT_RECORD (no ambiguity!)
    protocolLevel_ = ProtocolLevel::LEGACY;
    return protocolLevel_;
}

bool PlatformTerminal::detectKittyProtocol() {
    // Same as POSIX implementation
    DWORD written;
    WriteConsole(hStdout_, L"\x1B[?u", 4, &written, nullptr);
    
    // Wait for response
    DWORD events;
    if (WaitForSingleObject(hStdin_, 200) != WAIT_OBJECT_0) {
        return false;
    }
    
    // Read and check response (simplified)
    INPUT_RECORD irec;
    DWORD read;
    ReadConsoleInput(hStdin_, &irec, 1, &read);
    
    // TODO: Parse actual response
    return false;  // Conservative
}

bool PlatformTerminal::detectXTermModifyKeys() {
    DWORD written;
    WriteConsole(hStdout_, L"\x1B[>4;2m", 8, &written, nullptr);
    return true;
}

std::optional<KeyEvent> PlatformTerminal::readEvent() {
    INPUT_RECORD irec;
    DWORD read;
    
    while (true) {
        if (!ReadConsoleInput(hStdin_, &irec, 1, &read) || read == 0) {
            return std::nullopt;
        }
        
        // Only process key down events
        if (irec.EventType != KEY_EVENT) {
            continue;
        }
        
        if (!irec.Event.KeyEvent.bKeyDown) {
            continue;  // Ignore key-up
        }
        
        return convertWindowsEvent(irec.Event.KeyEvent);
    }
}

std::optional<KeyEvent> PlatformTerminal::convertWindowsEvent(const KEY_EVENT_RECORD& keyEvent) {
    WORD vk = keyEvent.wVirtualKeyCode;
    DWORD ctrlState = keyEvent.dwControlKeyState;
    WCHAR ch = keyEvent.uChar.UnicodeChar;
    
    // Build modifiers
    KeyModifiers mods = KeyModifiers::NONE;
    if (ctrlState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
        mods = mods | KeyModifiers::CTRL;
    }
    if (ctrlState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) {
        mods = mods | KeyModifiers::ALT;
    }
    if (ctrlState & SHIFT_PRESSED) {
        mods = mods | KeyModifiers::SHIFT;
    }
    
    // Ctrl+Enter (Enter with Ctrl modifier)
    if (vk == VK_RETURN && (mods & KeyModifiers::CTRL)) {
        return KeyEvent{KeyType::CTRL_ENTER, mods};
    }
    
    // Alt+Enter
    if (vk == VK_RETURN && (mods & KeyModifiers::ALT)) {
        return KeyEvent{KeyType::ALT_ENTER, mods};
    }
    
    // Standard keys
    switch (vk) {
        case VK_RETURN: return KeyEvent{KeyType::ENTER};
        case VK_BACK: return KeyEvent{KeyType::BACKSPACE};
        case VK_DELETE: return KeyEvent{KeyType::DELETE};
        case VK_TAB: return KeyEvent{KeyType::TAB};
        case VK_ESCAPE: return KeyEvent{KeyType::ESCAPE};
        
        // Ctrl+C/D/Z
        case 'C': if (mods & KeyModifiers::CTRL) return KeyEvent{KeyType::CTRL_C}; break;
        case 'D': if (mods & KeyModifiers::CTRL) return KeyEvent{KeyType::CTRL_D}; break;
        case 'Z': if (mods & KeyModifiers::CTRL) return KeyEvent{KeyType::CTRL_Z}; break;
        case 'L': if (mods & KeyModifiers::CTRL) return KeyEvent{KeyType::CTRL_L}; break;
        
        // Arrow keys
        case VK_UP: return KeyEvent{KeyType::ARROW_UP};
        case VK_DOWN: return KeyEvent{KeyType::ARROW_DOWN};
        case VK_LEFT: return KeyEvent{KeyType::ARROW_LEFT};
        case VK_RIGHT: return KeyEvent{KeyType::ARROW_RIGHT};
        
        // Home/End
        case VK_HOME: return KeyEvent{KeyType::HOME};
        case VK_END: return KeyEvent{KeyType::END};
        
        // Page Up/Down
        case VK_PRIOR: return KeyEvent{KeyType::PAGE_UP};
        case VK_NEXT: return KeyEvent{KeyType::PAGE_DOWN};
        
        // Function keys
        case VK_F1: return KeyEvent{KeyType::F1};
        case VK_F2: return KeyEvent{KeyType::F2};
        case VK_F3: return KeyEvent{KeyType::F3};
        case VK_F4: return KeyEvent{KeyType::F4};
        case VK_F5: return KeyEvent{KeyType::F5};
        case VK_F6: return KeyEvent{KeyType::F6};
        case VK_F7: return KeyEvent{KeyType::F7};
        case VK_F8: return KeyEvent{KeyType::F8};
        case VK_F9: return KeyEvent{KeyType::F9};
        case VK_F10: return KeyEvent{KeyType::F10};
        case VK_F11: return KeyEvent{KeyType::F11};
        case VK_F12: return KeyEvent{KeyType::F12};
    }
    
    // Character
    if (ch >= 32 && ch < 127) {
        return KeyEvent{KeyType::CHARACTER, mods, static_cast<char32_t>(ch)};
    }
    
    return std::nullopt;
}

std::pair<int, int> PlatformTerminal::getSize() const {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hStdout_, &csbi)) {
        int cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        return {cols, rows};
    }
    return {80, 24};
}

bool PlatformTerminal::isUnicodeSupported() const {
    return true;  // Windows consoles support Unicode natively
}

#endif // _WIN32

} // namespace repl
} // namespace ariash
