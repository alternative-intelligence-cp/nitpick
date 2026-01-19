/**
 * token.cpp
 * Token implementation for ABC Parser
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "abc/token.hpp"
#include <sstream>

namespace aria {
namespace abc {

const char* token_type_string(TokenType type) {
    switch (type) {
        case TokenType::LBRACE:         return "LBRACE";
        case TokenType::RBRACE:         return "RBRACE";
        case TokenType::LBRACKET:       return "LBRACKET";
        case TokenType::RBRACKET:       return "RBRACKET";
        case TokenType::COLON:          return "COLON";
        case TokenType::COMMA:          return "COMMA";
        case TokenType::IDENTIFIER:     return "IDENTIFIER";
        case TokenType::STRING:         return "STRING";
        case TokenType::INTEGER:        return "INTEGER";
        case TokenType::TRUE_LITERAL:   return "TRUE";
        case TokenType::FALSE_LITERAL:  return "FALSE";
        case TokenType::END_OF_FILE:    return "EOF";
        case TokenType::ERROR:          return "ERROR";
    }
    return "UNKNOWN";
}

std::string SourceLocation::to_string() const {
    std::ostringstream ss;
    ss << line << ":" << column;
    return ss.str();
}

} // namespace abc
} // namespace aria
