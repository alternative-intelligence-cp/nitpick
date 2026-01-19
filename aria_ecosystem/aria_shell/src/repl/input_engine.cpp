/**
 * Input Engine Implementation
 * 
 * Modal FSM for multi-line REPL input with syntax-aware editing.
 */

#include "repl/input_engine.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace ariash {
namespace repl {

// =============================================================================
// EditBuffer Implementation
// =============================================================================

EditBuffer::EditBuffer() {
    lines_.push_back("");  // Start with one empty line
    cursor_ = BufferPosition{0, 0};
}

void EditBuffer::insertChar(char32_t ch) {
    if (cursor_.line >= lines_.size()) {
        lines_.resize(cursor_.line + 1);
    }
    
    std::string& line = lines_[cursor_.line];
    
    // Simple ASCII for now (TODO: full UTF-8 support)
    if (ch < 128) {
        line.insert(cursor_.column, 1, static_cast<char>(ch));
        cursor_.column++;
    }
}

void EditBuffer::insertNewline() {
    if (cursor_.line >= lines_.size()) {
        lines_.resize(cursor_.line + 1);
    }
    
    // Split current line at cursor
    std::string& currentLine = lines_[cursor_.line];
    std::string remainder = currentLine.substr(cursor_.column);
    currentLine = currentLine.substr(0, cursor_.column);
    
    // Insert new line
    lines_.insert(lines_.begin() + cursor_.line + 1, remainder);
    
    // Move cursor to start of new line
    cursor_.line++;
    cursor_.column = 0;
}

void EditBuffer::backspace() {
    if (cursor_.column > 0) {
        // Delete character before cursor on current line
        lines_[cursor_.line].erase(cursor_.column - 1, 1);
        cursor_.column--;
    } else if (cursor_.line > 0) {
        // Join with previous line
        std::string currentLine = lines_[cursor_.line];
        lines_.erase(lines_.begin() + cursor_.line);
        cursor_.line--;
        cursor_.column = lines_[cursor_.line].length();
        lines_[cursor_.line] += currentLine;
    }
}

void EditBuffer::deleteChar() {
    if (cursor_.column < lines_[cursor_.line].length()) {
        // Delete character at cursor
        lines_[cursor_.line].erase(cursor_.column, 1);
    } else if (cursor_.line < lines_.size() - 1) {
        // Join with next line
        lines_[cursor_.line] += lines_[cursor_.line + 1];
        lines_.erase(lines_.begin() + cursor_.line + 1);
    }
}

void EditBuffer::clear() {
    lines_.clear();
    lines_.push_back("");
    cursor_ = BufferPosition{0, 0};
}

void EditBuffer::moveCursorLeft() {
    if (cursor_.column > 0) {
        cursor_.column--;
    } else if (cursor_.line > 0) {
        cursor_.line--;
        cursor_.column = lines_[cursor_.line].length();
    }
}

void EditBuffer::moveCursorRight() {
    if (cursor_.column < lines_[cursor_.line].length()) {
        cursor_.column++;
    } else if (cursor_.line < lines_.size() - 1) {
        cursor_.line++;
        cursor_.column = 0;
    }
}

void EditBuffer::moveCursorUp() {
    if (cursor_.line > 0) {
        cursor_.line--;
        cursor_.column = std::min(cursor_.column, lines_[cursor_.line].length());
    }
}

void EditBuffer::moveCursorDown() {
    if (cursor_.line < lines_.size() - 1) {
        cursor_.line++;
        cursor_.column = std::min(cursor_.column, lines_[cursor_.line].length());
    }
}

void EditBuffer::moveCursorToStart() {
    cursor_ = BufferPosition{0, 0};
}

void EditBuffer::moveCursorToEnd() {
    cursor_.line = lines_.size() - 1;
    cursor_.column = lines_[cursor_.line].length();
}

void EditBuffer::moveCursorToLineStart() {
    cursor_.column = 0;
}

void EditBuffer::moveCursorToLineEnd() {
    cursor_.column = lines_[cursor_.line].length();
}

std::string EditBuffer::getContent() const {
    std::ostringstream oss;
    for (size_t i = 0; i < lines_.size(); ++i) {
        oss << lines_[i];
        if (i < lines_.size() - 1) {
            oss << '\n';
        }
    }
    return oss.str();
}

int EditBuffer::getBraceDepth() const {
    return calculateBraceDepth(getContent());
}

bool EditBuffer::isBalanced() const {
    return getBraceDepth() == 0;
}

bool EditBuffer::hasSyntaxError() const {
    // Simple check: negative brace depth indicates syntax error
    return getBraceDepth() < 0;
}

int EditBuffer::calculateBraceDepth(const std::string& content) const {
    int depth = 0;
    bool inString = false;
    char quoteChar = 0;
    bool inComment = false;
    
    for (size_t i = 0; i < content.length(); ++i) {
        char c = content[i];
        
        // Handle comment state
        if (inComment) {
            if (c == '\n') inComment = false;
            continue;
        }
        
        // Check for comment start
        if (c == '/' && i + 1 < content.length() && content[i + 1] == '/') {
            inComment = true;
            i++;
            continue;
        }
        
        // Handle string state
        if (inString) {
            if (c == quoteChar && (i == 0 || content[i - 1] != '\\')) {
                inString = false;
            }
            continue;
        }
        
        // Check for string start
        if (c == '"' || c == '\'' || c == '`') {
            inString = true;
            quoteChar = c;
            continue;
        }
        
        // Count braces outside strings and comments
        if (c == '{' || c == '[' || c == '(') {
            depth++;
        } else if (c == '}' || c == ']' || c == ')') {
            depth--;
        }
    }
    
    return depth;
}

bool EditBuffer::shouldAutoSubmit() const {
    // Auto-submit if:
    // 1. Braces are balanced (no unclosed braces)
    // 2. Content ends with semicolon (indicating statement completion)
    
    std::string content = getContent();
    
    // Trim trailing whitespace
    size_t lastNonSpace = content.find_last_not_of(" \t\n\r");
    if (lastNonSpace == std::string::npos) {
        return false;  // Empty or all whitespace
    }
    
    // Check if ends with semicolon
    if (content[lastNonSpace] != ';') {
        return false;
    }
    
    // Check if balanced
    return getBraceDepth() == 0;
}

bool EditBuffer::endsWithDoubleSemicolon() const {
    // Check if buffer ends with ;; (ignoring whitespace)
    std::string content = getContent();
    
    // Trim trailing whitespace
    size_t end = content.find_last_not_of(" \t\n\r");
    if (end == std::string::npos || end < 1) {
        return false;  // Empty or too short
    }
    
    // Check for two semicolons
    if (content[end] != ';') {
        return false;
    }
    
    // Find previous non-whitespace character
    size_t prev = content.find_last_not_of(" \t\n\r", end - 1);
    if (prev == std::string::npos) {
        return false;
    }
    
    return content[prev] == ';';
}

void EditBuffer::ensureCursorValid() {
    if (cursor_.line >= lines_.size()) {
        cursor_.line = lines_.size() - 1;
    }
    if (cursor_.column > lines_[cursor_.line].length()) {
        cursor_.column = lines_[cursor_.line].length();
    }
}

// =============================================================================
// InputEngine Implementation
// =============================================================================

InputEngine::InputEngine(PlatformTerminal& terminal)
    : terminal_(terminal)
    , state_(InputState::IDLE)
    , running_(false)
    , continuationMode_(false)
    , editMode_(false)  // Start in RUN mode (Enter submits)
{
}

void InputEngine::run() {
    // Enter raw mode to capture control characters
    if (!terminal_.enterRawMode()) {
        std::cerr << "Failed to enter raw mode\n";
        return;
    }
    
    running_ = true;
    state_ = InputState::IDLE;
    
    renderPrompt();
    
    while (running_) {
        auto event = terminal_.readEvent();
        if (!event) {
            continue;  // Timeout, keep waiting
        }
        
        switch (state_) {
            case InputState::IDLE:
                handleIdle(*event);
                break;
            case InputState::BUFFER_MANIPULATION:
                handleBufferManipulation(*event);
                break;
            case InputState::CHORD_ANALYSIS:
                handleChordAnalysis(*event);
                break;
            case InputState::SUBMISSION:
                handleSubmission();
                break;
        }
    }
    
    // Restore terminal mode on exit
    terminal_.restoreMode();
}

void InputEngine::handleIdle(const KeyEvent& event) {
    // Transition to appropriate state based on input
    
    // ESC toggles between RUN and EDIT mode
    if (event.type == KeyType::ESCAPE) {
        editMode_ = !editMode_;
        std::string mode = editMode_ ? "EDIT" : "RUN";
        std::cout << " [" << mode << " MODE]";
        std::cout << "\r\033[K";  // Clear line
        renderPrompt();
        return;
    }
    
    if (event.type == KeyType::CHARACTER) {
        state_ = InputState::BUFFER_MANIPULATION;
        handleBufferManipulation(event);  // Process immediately
        return;
    }
    
    if (event.type == KeyType::ENTER) {
        state_ = InputState::BUFFER_MANIPULATION;
        handleBufferManipulation(event);
        return;
    }
    
    if (event.type == KeyType::BACKSPACE) {
        state_ = InputState::BUFFER_MANIPULATION;
        handleBufferManipulation(event);
        return;
    }
    
    if (event.modifiers & KeyModifiers::CTRL) {
        state_ = InputState::CHORD_ANALYSIS;
        handleChordAnalysis(event);
        return;
    }
    
    // Handle direct actions
    if (event.type == KeyType::CTRL_C) {
        buffer_.clear();
        std::cout << "^C\n";
        renderPrompt();
        return;
    }
    
    if (event.type == KeyType::CTRL_D) {
        if (buffer_.isEmpty()) {
            std::cout << "\n";
            if (exitCallback_) {
                exitCallback_();
            }
            running_ = false;
        }
        return;
    }
}

void InputEngine::handleBufferManipulation(const KeyEvent& event) {
    // Process editing commands
    
    if (event.type == KeyType::CHARACTER) {
        buffer_.insertChar(event.codepoint);
        // Echo the character (simple echo for now, cursor at end)
        std::cout << static_cast<char>(event.codepoint) << std::flush;
    }
    else if (event.type == KeyType::ENTER) {
        // Mode-dependent behavior
        if (editMode_) {
            // EDIT mode: Check for ;; pattern to submit
            if (buffer_.endsWithDoubleSemicolon()) {
                // Remove the extra semicolon before submitting
                buffer_.backspace();  // Remove one ;
                state_ = InputState::SUBMISSION;
                std::cout << "\n";
                handleSubmission();
                return;
            }
            
            // Otherwise, add newline for multi-line editing
            buffer_.insertNewline();
            std::cout << "\n";
            continuationMode_ = true;
            renderPrompt();
            applyAutoIndent();
        } else {
            // RUN mode: Enter always submits
            state_ = InputState::SUBMISSION;
            std::cout << "\n";
            handleSubmission();
            return;
        }
    }
    else if (event.type == KeyType::BACKSPACE) {
        buffer_.backspace();
        std::cout << "\b \b" << std::flush;  // Erase character
    }
    else if (event.type == KeyType::DELETE) {
        buffer_.deleteChar();
    }
    else if (event.type == KeyType::ARROW_LEFT) {
        buffer_.moveCursorLeft();
        renderCursor();
    }
    else if (event.type == KeyType::ARROW_RIGHT) {
        buffer_.moveCursorRight();
        renderCursor();
    }
    else if (event.type == KeyType::ARROW_UP) {
        buffer_.moveCursorUp();
        renderCursor();
    }
    else if (event.type == KeyType::ARROW_DOWN) {
        buffer_.moveCursorDown();
        renderCursor();
    }
    else if (event.type == KeyType::HOME) {
        buffer_.moveCursorToLineStart();
        renderCursor();
    }
    else if (event.type == KeyType::END) {
        buffer_.moveCursorToLineEnd();
        renderCursor();
    }
    else if (event.type == KeyType::CTRL_C) {
        buffer_.clear();
        std::cout << "^C\n";
        continuationMode_ = false;
        renderPrompt();
    }
    else if (event.type == KeyType::CTRL_L) {
        clearScreen();
        renderBuffer();
    }
    
    // Check for chord transitions
    if (event.modifiers & KeyModifiers::CTRL) {
        state_ = InputState::CHORD_ANALYSIS;
    } else {
        state_ = InputState::IDLE;
    }
}

void InputEngine::handleChordAnalysis(const KeyEvent& event) {
    // Process multi-key chords
    
    if (event.type == KeyType::CTRL_ENTER || event.type == KeyType::ALT_ENTER) {
        // Submission trigger (works in both modes)
        state_ = InputState::SUBMISSION;
        handleSubmission();
        return;
    }
    
    // Fallback: Ctrl+J (which most terminals send for Ctrl+Enter in legacy mode)
    if (event.type == KeyType::CHARACTER && event.codepoint == 10 && (event.modifiers & KeyModifiers::CTRL)) {
        state_ = InputState::SUBMISSION;
        handleSubmission();
        return;
    }
    
    if (event.type == KeyType::CTRL_C) {
        buffer_.clear();
        std::cout << "^C\n";
        continuationMode_ = false;
        renderPrompt();
        state_ = InputState::IDLE;
        return;
    }
    
    if (event.type == KeyType::CTRL_D) {
        if (buffer_.isEmpty()) {
            std::cout << "\n";
            if (exitCallback_) {
                exitCallback_();
            }
            running_ = false;
        }
        state_ = InputState::IDLE;
        return;
    }
    
    // Other editing macros (future: Ctrl+W for delete-word, etc.)
    
    state_ = InputState::IDLE;
}

void InputEngine::handleSubmission() {
    std::string code = buffer_.getContent();
    
    // Debug: Show what we're trying to execute
    // std::cerr << "[DEBUG] Submitting: " << code << std::endl;
    
    // Validate syntax (simple brace balance check)
    if (buffer_.hasSyntaxError()) {
        showError("Syntax Error: Unbalanced braces");
        state_ = InputState::IDLE;
        return;
    }
    
    if (!buffer_.isBalanced()) {
        showError("Incomplete: Missing closing brace");
        state_ = InputState::IDLE;
        return;
    }
    
    // Valid submission!
    std::cout << "\n";  // Move to new line after code
    
    if (submissionCallback_) {
        submissionCallback_(code);
    }
    
    // Reset for next input
    buffer_.clear();
    continuationMode_ = false;
    state_ = InputState::IDLE;
    renderPrompt();
}

std::string InputEngine::getPrompt() const {
    std::string modeIndicator = editMode_ ? "[EDIT] " : "[RUN] ";
    
    if (continuationMode_) {
        // Continuation prompt with indent hint
        int indent = calculateIndent();
        return modeIndicator + std::string(indent * 2, ' ') + "... ";
    }
    return modeIndicator + "aria> ";
}

void InputEngine::toggleMode() {
    editMode_ = !editMode_;
    
    // Clear line and reprint prompt with new mode
    std::cout << "\r\033[K";  // Clear line
    renderPrompt();
    renderBuffer();
}

void InputEngine::renderPrompt() {
    std::cout << getPrompt() << std::flush;
}

void InputEngine::renderBuffer() {
    // Redraw entire buffer (for clear-screen)
    std::cout << "\r";  // Carriage return
    renderPrompt();
    
    auto lines = buffer_.getLines();
    for (size_t i = 0; i < lines.size(); ++i) {
        std::cout << lines[i];
        if (i < lines.size() - 1) {
            std::cout << "\n";
            renderPrompt();
        }
    }
    
    std::cout << std::flush;
}

void InputEngine::renderCursor() {
    // TODO: ANSI cursor movement sequences
    // For now, just flush (cursor movement happens via arrow keys)
    std::cout << std::flush;
}

void InputEngine::clearScreen() {
    // ANSI clear screen: ESC[2J ESC[H
    std::cout << "\x1B[2J\x1B[H" << std::flush;
}

void InputEngine::showError(const std::string& message) {
    // Red error message
    std::cout << "\n\x1B[31m" << message << "\x1B[0m\n";
    renderPrompt();
}

int InputEngine::calculateIndent() const {
    // Count open braces to determine indent level
    return std::max(0, buffer_.getBraceDepth());
}

void InputEngine::applyAutoIndent() {
    // Automatically indent based on brace depth
    int indent = calculateIndent();
    std::string spaces(indent * 2, ' ');
    std::cout << spaces << std::flush;
    
    // Insert spaces into buffer
    for (char c : spaces) {
        buffer_.insertChar(c);
    }
}

} // namespace repl
} // namespace ariash
