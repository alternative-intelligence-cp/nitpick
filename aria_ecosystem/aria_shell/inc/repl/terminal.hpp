/**
 * Platform Terminal Abstraction
 * 
 * Cross-platform interface for raw terminal mode, enabling modal multi-line
 * input in the Aria REPL. Implements spec from shell_03_multiline_input.txt.
 * 
 * Key Features:
 * - POSIX raw mode (tcsetattr/termios)
 * - Windows console mode (SetConsoleMode/ReadConsoleInput)
 * - Virtual Terminal Sequence support
 * - Kitty Keyboard Protocol negotiation
 * - XTerm modifyOtherKeys support
 */

#pragma once

#include <string>
#include <cstdint>
#include <optional>

#ifndef _WIN32
#include <termios.h>
#else
#include <windows.h>
#endif

namespace ariash {
namespace repl {

/**
 * Key event types
 */
enum class KeyType {
    CHARACTER,      // Printable character
    ENTER,          // Standard Enter key
    CTRL_ENTER,     // Ctrl+Enter (submission trigger)
    ALT_ENTER,      // Alt+Enter (fallback submission)
    BACKSPACE,
    DELETE,
    ARROW_UP,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT,
    HOME,
    END,
    PAGE_UP,
    PAGE_DOWN,
    TAB,
    CTRL_C,         // Interrupt
    CTRL_D,         // EOF/Exit
    CTRL_Z,         // Suspend
    CTRL_L,         // Clear screen
    ESCAPE,         // Standalone Esc key
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    UNKNOWN
};

/**
 * Modifier flags (bitmask)
 */
enum class KeyModifiers : uint8_t {
    NONE    = 0,
    SHIFT   = 1 << 0,
    CTRL    = 1 << 1,
    ALT     = 1 << 2,
    META    = 1 << 3
};

inline KeyModifiers operator|(KeyModifiers a, KeyModifiers b) {
    return static_cast<KeyModifiers>(
        static_cast<uint8_t>(a) | static_cast<uint8_t>(b)
    );
}

inline bool operator&(KeyModifiers a, KeyModifiers b) {
    return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

/**
 * Unified key event structure
 */
struct KeyEvent {
    KeyType type;
    KeyModifiers modifiers;
    char32_t codepoint;  // UTF-32 for Unicode support
    
    KeyEvent() 
        : type(KeyType::UNKNOWN)
        , modifiers(KeyModifiers::NONE)
        , codepoint(0) 
    {}
    
    KeyEvent(KeyType t, KeyModifiers m = KeyModifiers::NONE, char32_t cp = 0)
        : type(t)
        , modifiers(m)
        , codepoint(cp)
    {}
};

/**
 * Terminal protocol capabilities
 */
enum class ProtocolLevel {
    LEGACY,             // Basic byte stream, Alt+Enter fallback
    XTERM_MODIFY_KEYS,  // XTerm modifyOtherKeys extension
    KITTY_PROGRESSIVE   // Kitty Keyboard Protocol (gold standard)
};

/**
 * Platform-agnostic terminal interface
 * 
 * Handles raw mode setup, restoration, and cross-platform event reading.
 */
class PlatformTerminal {
public:
    PlatformTerminal();
    ~PlatformTerminal();
    
    /**
     * Enter raw mode (disable canonical processing)
     * 
     * POSIX: Disables ICANON, ECHO, ISIG, etc.
     * Windows: Disables ENABLE_LINE_INPUT, ENABLE_ECHO_INPUT
     */
    bool enterRawMode();
    
    /**
     * Restore original terminal state
     * CRITICAL: Must be called on exit or Ctrl+Z to avoid unusable terminal
     */
    void restoreMode();
    
    /**
     * Attempt protocol negotiation for enhanced key disambiguation
     * 
     * Tries in order:
     * 1. Kitty Keyboard Protocol
     * 2. XTerm modifyOtherKeys
     * 3. Legacy fallback (Alt+Enter)
     */
    ProtocolLevel negotiateProtocol();
    
    /**
     * Read next key event (blocking)
     * 
     * POSIX: Parses ANSI escape sequences with VTIME timeout
     * Windows: Uses ReadConsoleInput with KEY_EVENT_RECORD
     * 
     * @return KeyEvent, or std::nullopt on error/timeout
     */
    std::optional<KeyEvent> readEvent();
    
    /**
     * Get terminal dimensions
     */
    std::pair<int, int> getSize() const;
    
    /**
     * Check if terminal supports Unicode
     */
    bool isUnicodeSupported() const;
    
    /**
     * Query current protocol level
     */
    ProtocolLevel getProtocolLevel() const { return protocolLevel_; }
    
private:
    // POSIX implementation
#ifndef _WIN32
    struct termios originalTermios_;
    bool termiosValid_;
    
    std::optional<KeyEvent> parseEscapeSequence();
    std::optional<KeyEvent> parseKittySequence(const std::string& seq);
    std::optional<KeyEvent> parseXTermSequence(const std::string& seq);
    std::optional<KeyEvent> parseAnsiSequence(const std::string& seq);
    
#else
    // Windows implementation
    HANDLE hStdin_;
    HANDLE hStdout_;
    DWORD originalInputMode_;
    DWORD originalOutputMode_;
    bool modesValid_;
    
    std::optional<KeyEvent> convertWindowsEvent(const KEY_EVENT_RECORD& keyEvent);
#endif
    
    ProtocolLevel protocolLevel_;
    bool rawModeActive_;
    
    // Protocol detection helpers
    bool detectKittyProtocol();
    bool detectXTermModifyKeys();
};

} // namespace repl
} // namespace ariash
