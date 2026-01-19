/**
 * parser.cpp
 * Recursive Descent Parser implementation for ABC files
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "abc/parser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace aria {
namespace abc {

// -----------------------------------------------------------------------------
// ParseError
// -----------------------------------------------------------------------------
std::string ParseError::to_string() const {
    std::ostringstream ss;
    ss << "Error at " << loc.to_string() << ": " << message;
    if (!context.empty()) {
        ss << "\n" << context;
    }
    return ss.str();
}

// -----------------------------------------------------------------------------
// Parser
// -----------------------------------------------------------------------------
Parser::Parser(std::string_view source)
    : lexer_(source) {}

ParseResult Parser::parse() {
    ParseResult result;
    result.ast = parse_build_file();
    result.errors = std::move(errors_);
    return result;
}

std::unique_ptr<BuildFileNode> Parser::parse_build_file() {
    auto root = std::make_unique<BuildFileNode>();

    // Expect opening brace
    if (!match(TokenType::LBRACE)) {
        error("Expected '{' at start of build file");
        return root;
    }

    // Parse top-level sections
    while (!check(TokenType::RBRACE) && !is_at_end()) {
        // Reset panic mode at section boundaries
        panic_mode_ = false;

        // Parse key
        std::string key = parse_key();
        if (key.empty()) {
            synchronize();
            continue;
        }

        // Expect colon
        if (!match(TokenType::COLON)) {
            error("Expected ':' after section name");
            synchronize();
            continue;
        }

        // Parse section value based on key
        if (key == "project") {
            if (root->project) {
                error("Duplicate 'project' section");
            }
            auto obj = parse_object();
            if (obj) {
                root->project = std::move(obj);
            }
        } else if (key == "variables") {
            if (root->variables) {
                error("Duplicate 'variables' section");
            }
            auto obj = parse_object();
            if (obj) {
                root->variables = std::move(obj);
            }
        } else if (key == "targets") {
            if (root->targets) {
                error("Duplicate 'targets' section");
            }
            auto arr = parse_array();
            if (arr) {
                root->targets = std::move(arr);
            }
        } else {
            error("Unknown top-level section: '" + key + "' (expected 'project', 'variables', or 'targets')");
            // Skip this value
            parse_value();
        }

        // Optional trailing comma
        match(TokenType::COMMA);
    }

    // Expect closing brace
    if (!match(TokenType::RBRACE)) {
        error("Expected '}' at end of build file");
    }

    return root;
}

std::unique_ptr<ObjectNode> Parser::parse_object() {
    auto obj = std::make_unique<ObjectNode>();
    obj->loc = peek().loc;

    if (!match(TokenType::LBRACE)) {
        error("Expected '{'");
        return obj;
    }

    while (!check(TokenType::RBRACE) && !is_at_end()) {
        SourceLocation key_loc = peek().loc;

        // Parse key
        std::string key = parse_key();
        if (key.empty()) {
            synchronize();
            continue;
        }

        // Expect colon
        if (!match(TokenType::COLON)) {
            error("Expected ':' after key '" + key + "'");
            synchronize();
            continue;
        }

        // Parse value
        auto value = parse_value();
        if (value) {
            obj->add(std::move(key), std::move(value), key_loc);
        }

        // Optional trailing comma (Aria-style)
        match(TokenType::COMMA);
    }

    if (!match(TokenType::RBRACE)) {
        error("Expected '}' to close object");
    }

    return obj;
}

std::unique_ptr<ArrayNode> Parser::parse_array() {
    auto arr = std::make_unique<ArrayNode>();
    arr->loc = peek().loc;

    if (!match(TokenType::LBRACKET)) {
        error("Expected '['");
        return arr;
    }

    while (!check(TokenType::RBRACKET) && !is_at_end()) {
        auto value = parse_value();
        if (value) {
            arr->add(std::move(value));
        } else {
            synchronize();
        }

        // Optional trailing comma (Aria-style)
        match(TokenType::COMMA);
    }

    if (!match(TokenType::RBRACKET)) {
        error("Expected ']' to close array");
    }

    return arr;
}

std::unique_ptr<ValueNode> Parser::parse_value() {
    const Token& tok = peek();

    switch (tok.type) {
        case TokenType::STRING: {
            advance();
            std::string content = extract_string(tok);
            return make_string_value(std::move(content), tok.loc);
        }

        case TokenType::INTEGER: {
            advance();
            int64_t val = std::stoll(std::string(tok.text));
            return make_int_value(val, tok.loc);
        }

        case TokenType::TRUE_LITERAL: {
            advance();
            return make_bool_value(true, tok.loc);
        }

        case TokenType::FALSE_LITERAL: {
            advance();
            return make_bool_value(false, tok.loc);
        }

        case TokenType::LBRACE: {
            auto obj = parse_object();
            return make_object_value(std::move(obj), tok.loc);
        }

        case TokenType::LBRACKET: {
            auto arr = parse_array();
            return make_array_value(std::move(arr), tok.loc);
        }

        case TokenType::ERROR:
            error_at(tok, tok.error_message);
            advance();
            return nullptr;

        default:
            error("Expected value (string, number, boolean, object, or array)");
            return nullptr;
    }
}

std::string Parser::parse_key() {
    const Token& tok = peek();

    if (tok.type == TokenType::IDENTIFIER) {
        advance();
        return std::string(tok.text);
    }

    if (tok.type == TokenType::STRING) {
        advance();
        return extract_string(tok);
    }

    error("Expected key (identifier or string)");
    return "";
}

const Token& Parser::peek() const {
    return const_cast<Lexer&>(lexer_).peek();
}

Token Parser::advance() {
    return lexer_.advance();
}

bool Parser::check(TokenType type) const {
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

void Parser::expect(TokenType type, const std::string& message) {
    if (!match(type)) {
        error(message);
    }
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::END_OF_FILE;
}

void Parser::error(const std::string& message) {
    error_at(peek(), message);
}

void Parser::error_at(const Token& token, const std::string& message) {
    if (panic_mode_) return;  // Suppress cascading errors
    panic_mode_ = true;

    ParseError err;
    err.message = message;
    err.loc = token.loc;
    err.context = get_context(token.loc);

    errors_.push_back(std::move(err));
}

void Parser::synchronize() {
    panic_mode_ = false;

    while (!is_at_end()) {
        TokenType type = peek().type;

        // Synchronize at structural tokens
        if (type == TokenType::RBRACE ||
            type == TokenType::RBRACKET ||
            type == TokenType::COMMA) {
            return;
        }

        // Or at known section keys
        if (type == TokenType::IDENTIFIER) {
            std::string_view text = peek().text;
            if (text == "project" || text == "variables" || text == "targets") {
                return;
            }
        }

        advance();
    }
}

std::string Parser::extract_string(const Token& token) {
    std::string_view text = token.text;

    // Remove surrounding quotes
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        text = text.substr(1, text.size() - 2);
    }

    // Process escape sequences
    std::string result;
    result.reserve(text.size());

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            char next = text[++i];
            switch (next) {
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case 'r':  result += '\r'; break;
                case '\\': result += '\\'; break;
                case '"':  result += '"';  break;
                case '&':  result += '&';  break;  // Allow escaping interpolation
                default:   result += next; break;
            }
        } else {
            result += text[i];
        }
    }

    return result;
}

std::string Parser::get_context(SourceLocation loc, size_t context_lines) {
    std::string_view source = lexer_.source();

    // Find start of the line containing loc
    size_t line_start = 0;
    size_t current_line = 1;

    for (size_t i = 0; i < source.size() && current_line < loc.line; ++i) {
        if (source[i] == '\n') {
            current_line++;
            line_start = i + 1;
        }
    }

    // Find end of the line
    size_t line_end = source.find('\n', line_start);
    if (line_end == std::string_view::npos) {
        line_end = source.size();
    }

    // Extract the line
    std::string line(source.substr(line_start, line_end - line_start));

    // Build context with caret indicator
    std::ostringstream ss;
    ss << loc.line << " | " << line << "\n";
    ss << std::string(std::to_string(loc.line).size(), ' ') << " | ";

    // Add spaces up to the column, then caret
    for (size_t i = 1; i < loc.column; ++i) {
        ss << ' ';
    }
    ss << '^';

    return ss.str();
}

// -----------------------------------------------------------------------------
// Convenience functions
// -----------------------------------------------------------------------------
ParseResult parse_abc(std::string_view source) {
    Parser parser(source);
    return parser.parse();
}

ParseResult parse_abc_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        ParseResult result;
        ParseError err;
        err.message = "Cannot open file: " + filename;
        err.loc = {0, 0, 0};
        result.errors.push_back(std::move(err));
        return result;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    Parser parser(source);
    return parser.parse();
}

} // namespace abc
} // namespace aria
