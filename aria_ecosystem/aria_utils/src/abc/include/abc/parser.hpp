/**
 * parser.hpp
 * Recursive Descent Parser for Aria Build Configuration (ABC) Files
 *
 * Features:
 * - LL(1) predictive parsing with single token lookahead
 * - Handles unquoted identifier keys (Aria-style)
 * - Supports trailing commas in objects and arrays
 * - Panic-mode error recovery for multiple error reporting
 * - Precise source location tracking for IDE integration
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_ABC_PARSER_HPP
#define ARIA_ABC_PARSER_HPP

#include "lexer.hpp"
#include "ast.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace aria {
namespace abc {

// -----------------------------------------------------------------------------
// Parse Error
// -----------------------------------------------------------------------------
struct ParseError {
    std::string message;
    SourceLocation loc;
    std::string context;  // Surrounding source for display

    std::string to_string() const;
};

// -----------------------------------------------------------------------------
// Parse Result
// -----------------------------------------------------------------------------
struct ParseResult {
    std::unique_ptr<BuildFileNode> ast;
    std::vector<ParseError> errors;

    bool success() const { return ast != nullptr && errors.empty(); }
    bool has_errors() const { return !errors.empty(); }
};

// -----------------------------------------------------------------------------
// Parser
// -----------------------------------------------------------------------------
class Parser {
public:
    /**
     * Construct parser with source.
     * The source must remain valid for the lifetime of the parser.
     */
    explicit Parser(std::string_view source);

    // No copying
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;

    /**
     * Parse the source and return result.
     * Even on error, a partial AST may be returned.
     */
    ParseResult parse();

    /**
     * Get all parse errors.
     */
    const std::vector<ParseError>& errors() const { return errors_; }

    /**
     * Check if any errors occurred.
     */
    bool has_errors() const { return !errors_.empty(); }

private:
    // Grammar rules
    std::unique_ptr<BuildFileNode> parse_build_file();
    std::unique_ptr<ObjectNode> parse_object();
    std::unique_ptr<ArrayNode> parse_array();
    std::unique_ptr<ValueNode> parse_value();
    std::string parse_key();

    // Token helpers
    const Token& peek() const;
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    void expect(TokenType type, const std::string& message);
    bool is_at_end() const;

    // Error handling
    void error(const std::string& message);
    void error_at(const Token& token, const std::string& message);
    void synchronize();

    // Helper to extract string content (strips quotes)
    std::string extract_string(const Token& token);

    // Get context around a source location
    std::string get_context(SourceLocation loc, size_t context_lines = 1);

private:
    Lexer lexer_;
    std::vector<ParseError> errors_;
    bool panic_mode_ = false;
};

// -----------------------------------------------------------------------------
// Convenience function
// -----------------------------------------------------------------------------

/**
 * Parse ABC source string.
 * Returns ParseResult with AST and any errors.
 */
ParseResult parse_abc(std::string_view source);

/**
 * Parse ABC file from disk.
 * Returns ParseResult with AST and any errors.
 */
ParseResult parse_abc_file(const std::string& filename);

} // namespace abc
} // namespace aria

#endif // ARIA_ABC_PARSER_HPP
