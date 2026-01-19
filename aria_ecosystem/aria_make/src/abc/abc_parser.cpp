/**
 * Aria Build Configuration (ABC) Parser Implementation
 *
 * ARIA-019: ABC Config Parser Design and Implementation
 */

#include "abc/abc_parser.hpp"
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace abc {

// =============================================================================
// Arena Allocator
// =============================================================================

ArenaAllocator::ArenaAllocator(size_t blockSize)
    : defaultBlockSize(blockSize) {
    allocateNewBlock(blockSize);
}

ArenaAllocator::~ArenaAllocator() {
    for (auto& block : blocks) {
        std::free(block.data);
    }
}

void ArenaAllocator::allocateNewBlock(size_t minSize) {
    size_t size = std::max(minSize, defaultBlockSize);
    Block block;
    block.data = static_cast<char*>(std::malloc(size));
    block.size = size;
    block.used = 0;
    blocks.push_back(block);
}

void* ArenaAllocator::allocate(size_t size, size_t alignment) {
    Block& current = blocks.back();

    // Align the current position
    size_t aligned = (current.used + alignment - 1) & ~(alignment - 1);

    if (aligned + size > current.size) {
        allocateNewBlock(size + alignment);
        return allocate(size, alignment);
    }

    void* ptr = current.data + aligned;
    current.used = aligned + size;
    return ptr;
}

void ArenaAllocator::reset() {
    // Keep first block, free the rest
    for (size_t i = 1; i < blocks.size(); ++i) {
        std::free(blocks[i].data);
    }
    if (!blocks.empty()) {
        blocks.resize(1);
        blocks[0].used = 0;
    }
}

// =============================================================================
// ObjectNode helpers
// =============================================================================

ASTNode* ObjectNode::find(const std::string& key) const {
    for (const auto& pair : members) {
        if (pair.key == key) {
            return pair.value;
        }
    }
    return nullptr;
}

std::string ObjectNode::getString(const std::string& key, const std::string& defaultVal) const {
    ASTNode* node = find(key);
    if (!node) return defaultVal;

    if (node->getKind() == Kind::LiteralString) {
        return static_cast<LiteralStringNode*>(node)->value;
    }
    return defaultVal;
}

int64_t ObjectNode::getInteger(const std::string& key, int64_t defaultVal) const {
    ASTNode* node = find(key);
    if (!node) return defaultVal;

    if (node->getKind() == Kind::Integer) {
        return static_cast<IntegerNode*>(node)->value;
    }
    return defaultVal;
}

bool ObjectNode::getBoolean(const std::string& key, bool defaultVal) const {
    ASTNode* node = find(key);
    if (!node) return defaultVal;

    if (node->getKind() == Kind::Boolean) {
        return static_cast<BooleanNode*>(node)->value;
    }
    return defaultVal;
}

ArrayNode* ObjectNode::getArray(const std::string& key) const {
    ASTNode* node = find(key);
    if (!node) return nullptr;

    if (node->getKind() == Kind::Array) {
        return static_cast<ArrayNode*>(node);
    }
    return nullptr;
}

ObjectNode* ObjectNode::getObject(const std::string& key) const {
    ASTNode* node = find(key);
    if (!node) return nullptr;

    if (node->getKind() == Kind::Object) {
        return static_cast<ObjectNode*>(node);
    }
    return nullptr;
}

// =============================================================================
// Parser
// =============================================================================

Parser::Parser(Lexer& lexer, ArenaAllocator& arena)
    : lexer(lexer), arena(arena) {
    advance();
}

void Parser::advance() {
    previous = current;
    current = lexer.nextToken();
}

bool Parser::check(TokenType type) {
    return current.type == type;
}

bool Parser::match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

void Parser::consume(TokenType type, const char* message) {
    if (check(type)) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

void Parser::error(const char* message) {
    if (panicMode) return;
    panicMode = true;

    std::ostringstream oss;
    oss << "line " << previous.line << ":" << previous.column
        << ": error: " << message;
    errors.push_back(oss.str());
}

void Parser::errorAtCurrent(const char* message) {
    if (panicMode) return;
    panicMode = true;

    std::ostringstream oss;
    oss << "line " << current.line << ":" << current.column
        << ": error: " << message;
    errors.push_back(oss.str());
}

void Parser::synchronize() {
    panicMode = false;

    while (current.type != TokenType::END_OF_FILE) {
        // Synchronize at structural delimiters
        if (current.type == TokenType::RIGHT_BRACE ||
            current.type == TokenType::RIGHT_BRACKET) {
            advance();
            return;
        }

        // Or at new key-value pairs (identifier followed by colon)
        if (current.type == TokenType::IDENTIFIER &&
            lexer.peekToken().type == TokenType::COLON) {
            return;
        }

        advance();
    }
}

ABCDocument Parser::parse() {
    ABCDocument doc;

    // Parse top-level object
    if (!check(TokenType::LEFT_BRACE)) {
        errorAtCurrent("Expected '{' at start of ABC file");
        return doc;
    }

    ObjectNode* root = parseObject();
    if (!root) return doc;

    // Extract sections
    doc.project = root->getObject("project");
    doc.variables = root->getObject("variables");
    doc.targets = root->getArray("targets");

    return doc;
}

ASTNode* Parser::parseValue() {
    if (match(TokenType::LEFT_BRACE)) {
        // Back up since parseObject expects to consume LEFT_BRACE
        // Actually, let's adjust - parseObject will be called after we match
        return parseObject();
    }

    if (check(TokenType::LEFT_BRACE)) {
        return parseObject();
    }

    if (check(TokenType::LEFT_BRACKET)) {
        return parseArray();
    }

    if (check(TokenType::STRING_LITERAL)) {
        return parseString();
    }

    if (check(TokenType::INTEGER)) {
        return parseNumber();
    }

    if (match(TokenType::BOOLEAN_TRUE)) {
        auto* node = arena.create<BooleanNode>(true);
        node->line = previous.line;
        node->column = previous.column;
        return node;
    }

    if (match(TokenType::BOOLEAN_FALSE)) {
        auto* node = arena.create<BooleanNode>(false);
        node->line = previous.line;
        node->column = previous.column;
        return node;
    }

    if (match(TokenType::NULL_LITERAL)) {
        auto* node = arena.create<NullNode>();
        node->line = previous.line;
        node->column = previous.column;
        return node;
    }

    // Bare identifiers as strings (for convenience)
    if (check(TokenType::IDENTIFIER)) {
        advance();
        auto* node = arena.create<LiteralStringNode>(std::string(previous.lexeme));
        node->line = previous.line;
        node->column = previous.column;
        return node;
    }

    errorAtCurrent("Expected value");
    return nullptr;
}

ObjectNode* Parser::parseObject() {
    consume(TokenType::LEFT_BRACE, "Expected '{'");

    auto* obj = arena.create<ObjectNode>();
    obj->line = previous.line;
    obj->column = previous.column;

    while (!check(TokenType::RIGHT_BRACE) && !check(TokenType::END_OF_FILE)) {
        // Parse key
        std::string key;
        if (check(TokenType::IDENTIFIER)) {
            advance();
            key = std::string(previous.lexeme);
        } else if (check(TokenType::STRING_LITERAL)) {
            advance();
            key = std::string(previous.lexeme);
        } else {
            errorAtCurrent("Expected key (identifier or string)");
            synchronize();
            continue;
        }

        // Expect colon
        if (!match(TokenType::COLON)) {
            errorAtCurrent("Expected ':' after key");
            synchronize();
            continue;
        }

        // Parse value
        ASTNode* value = parseValue();
        if (!value) {
            synchronize();
            continue;
        }

        obj->members.push_back({key, value});

        // Handle comma (optional trailing comma supported)
        if (check(TokenType::COMMA)) {
            advance();
            // Allow trailing comma - just continue
        } else if (!check(TokenType::RIGHT_BRACE)) {
            errorAtCurrent("Expected ',' or '}' after value");
            synchronize();
        }
    }

    consume(TokenType::RIGHT_BRACE, "Expected '}'");
    return obj;
}

ArrayNode* Parser::parseArray() {
    consume(TokenType::LEFT_BRACKET, "Expected '['");

    auto* arr = arena.create<ArrayNode>();
    arr->line = previous.line;
    arr->column = previous.column;

    while (!check(TokenType::RIGHT_BRACKET) && !check(TokenType::END_OF_FILE)) {
        ASTNode* element = parseValue();
        if (!element) {
            synchronize();
            continue;
        }

        arr->elements.push_back(element);

        // Handle comma (optional trailing comma supported)
        if (check(TokenType::COMMA)) {
            advance();
            // Allow trailing comma
        } else if (!check(TokenType::RIGHT_BRACKET)) {
            errorAtCurrent("Expected ',' or ']' after element");
            synchronize();
        }
    }

    consume(TokenType::RIGHT_BRACKET, "Expected ']'");
    return arr;
}

ASTNode* Parser::parseString() {
    consume(TokenType::STRING_LITERAL, "Expected string");

    // Check if this is a simple string or has interpolation
    std::string content(previous.lexeme);

    // Check for &{ in the content
    size_t interpPos = content.find("&{");
    if (interpPos == std::string::npos) {
        // Simple literal string
        auto* node = arena.create<LiteralStringNode>(content);
        node->line = previous.line;
        node->column = previous.column;
        return node;
    }

    // Composite string with interpolation
    auto* composite = arena.create<CompositeStringNode>();
    composite->line = previous.line;
    composite->column = previous.column;

    size_t pos = 0;
    while (pos < content.size()) {
        interpPos = content.find("&{", pos);

        if (interpPos == std::string::npos) {
            // Rest is literal
            if (pos < content.size()) {
                CompositeStringNode::Segment seg;
                seg.isVariable = false;
                seg.value = content.substr(pos);
                composite->segments.push_back(seg);
            }
            break;
        }

        // Literal before interpolation
        if (interpPos > pos) {
            CompositeStringNode::Segment seg;
            seg.isVariable = false;
            seg.value = content.substr(pos, interpPos - pos);
            composite->segments.push_back(seg);
        }

        // Find closing brace
        size_t closePos = content.find('}', interpPos);
        if (closePos == std::string::npos) {
            // Unterminated interpolation
            error("Unterminated variable interpolation");
            break;
        }

        // Extract variable name
        std::string varName = content.substr(interpPos + 2, closePos - interpPos - 2);
        CompositeStringNode::Segment varSeg;
        varSeg.isVariable = true;
        varSeg.value = varName;
        composite->segments.push_back(varSeg);

        pos = closePos + 1;
    }

    return composite;
}

ASTNode* Parser::parseNumber() {
    consume(TokenType::INTEGER, "Expected number");

    int64_t value = std::strtoll(std::string(previous.lexeme).c_str(), nullptr, 10);
    auto* node = arena.create<IntegerNode>(value);
    node->line = previous.line;
    node->column = previous.column;
    return node;
}

} // namespace abc
