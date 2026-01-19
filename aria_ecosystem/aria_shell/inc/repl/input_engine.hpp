/**
 * Input Engine - Modal Finite State Machine
 * 
 * Implements the 4-state FSM for modal multi-line input:
 * - IDLE: Waiting for input
 * - BUFFER_MANIPULATION: Editing multi-line buffer
 * - CHORD_ANALYSIS: Processing key chords (Ctrl+X, etc.)
 * - SUBMISSION: Validating and executing code
 * 
 * Spec: shell_03_multiline_input.txt Section 3
 */

#pragma once

#include "repl/terminal.hpp"
#include <string>
#include <vector>
#include <functional>

namespace ariash {
namespace repl {

/**
 * FSM States
 */
enum class InputState {
    IDLE,                 // Quiescent, waiting for events
    BUFFER_MANIPULATION,  // Editing multi-line buffer
    CHORD_ANALYSIS,       // Processing key chord
    SUBMISSION            // Validating for execution
};

/**
 * Buffer position (line, column)
 */
struct BufferPosition {
    size_t line;
    size_t column;
    
    BufferPosition() : line(0), column(0) {}
    BufferPosition(size_t l, size_t c) : line(l), column(c) {}
};

/**
 * Multi-line edit buffer
 */
class EditBuffer {
public:
    EditBuffer();
    
    // Content manipulation
    void insertChar(char32_t ch);
    void insertNewline();
    void deleteChar();       // Delete at cursor
    void backspace();        // Delete before cursor
    void clear();
    
    // Cursor movement
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorUp();
    void moveCursorDown();
    void moveCursorToStart();
    void moveCursorToEnd();
    void moveCursorToLineStart();
    void moveCursorToLineEnd();
    
    // Query
    std::string getContent() const;
    std::vector<std::string> getLines() const { return lines_; }
    BufferPosition getCursor() const { return cursor_; }
    bool isEmpty() const { return lines_.size() == 1 && lines_[0].empty(); }
    size_t lineCount() const { return lines_.size(); }
    
    // Syntactic analysis
    int getBraceDepth() const;
    bool hasSyntaxError() const;
    bool isBalanced() const;
    bool shouldAutoSubmit() const;  // Check if ready to auto-submit on Enter
    bool endsWithDoubleSemicolon() const;  // Check for ;; pattern
    
private:
    std::vector<std::string> lines_;
    BufferPosition cursor_;
    
    void ensureCursorValid();
    int calculateBraceDepth(const std::string& content) const;
};

/**
 * Input Engine - Core FSM
 */
class InputEngine {
public:
    using SubmissionCallback = std::function<void(const std::string& code)>;
    using ExitCallback = std::function<void()>;
    
    InputEngine(PlatformTerminal& terminal);
    
    /**
     * Main event loop
     * Blocks until user submits code or exits
     */
    void run();
    
    /**
     * Set callback for code submission
     */
    void onSubmission(SubmissionCallback callback) {
        submissionCallback_ = callback;
    }
    
    /**
     * Set callback for exit (Ctrl+D)
     */
    void onExit(ExitCallback callback) {
        exitCallback_ = callback;
    }
    
    /**
     * Get current prompt string
     */
    std::string getPrompt() const;
    
    /**
     * Get current input mode
     */
    bool isEditMode() const { return editMode_; }
    
    /**
     * Toggle between RUN and EDIT mode
     */
    void toggleMode();
    
    /**
     * Request exit (can be called from submission callback)
     */
    void requestExit() { running_ = false; }
    
private:
    PlatformTerminal& terminal_;
    EditBuffer buffer_;
    InputState state_;
    
    SubmissionCallback submissionCallback_;
    ExitCallback exitCallback_;
    
    bool running_;
    bool continuationMode_;  // Multi-line continuation prompt active
    bool editMode_;          // Edit mode (Ctrl+E) vs Run mode (Ctrl+R)
    
    // State handlers
    void handleIdle(const KeyEvent& event);
    void handleBufferManipulation(const KeyEvent& event);
    void handleChordAnalysis(const KeyEvent& event);
    void handleSubmission();
    
    // Actions
    void renderPrompt();
    void renderBuffer();
    void renderCursor();
    void clearScreen();
    void showError(const std::string& message);
    
    // Auto-indentation
    int calculateIndent() const;
    void applyAutoIndent();
};

} // namespace repl
} // namespace ariash
