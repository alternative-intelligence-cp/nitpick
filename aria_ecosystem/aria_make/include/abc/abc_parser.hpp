/**
 * Aria Build Configuration (ABC) Parser
 *
 * ARIA-019: ABC Config Parser Design and Implementation
 *
 * Recursive-descent parser that constructs a strongly-typed AST.
 * Features:
 * - Arena allocation for high performance
 * - Panic mode error recovery
 * - Trailing comma support
 * - Interpolated string handling
 */

#ifndef ABC_PARSER_HPP
#define ABC_PARSER_HPP

#include "abc/abc_lexer.hpp"
#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <optional>

namespace abc {

// Forward declarations
class ASTNode;
class ObjectNode;
class ArrayNode;
class StringNode;
class LiteralStringNode;
class CompositeStringNode;
class VariableRefNode;
class IntegerNode;
class BooleanNode;
class NullNode;

/**
 * Arena Allocator for AST nodes
 *
 * Provides O(1) allocation and bulk deallocation.
 * All nodes are allocated contiguously for cache efficiency.
 */
class ArenaAllocator {
public:
    explicit ArenaAllocator(size_t blockSize = 64 * 1024);
    ~ArenaAllocator();

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    /**
     * Allocate memory from the arena
     *
     * @param size Number of bytes to allocate
     * @param alignment Alignment requirement
     * @return Pointer to allocated memory
     */
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t));

    /**
     * Create an object in the arena
     */
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }

    /**
     * Reset the arena (frees all allocations)
     */
    void reset();

private:
    struct Block {
        char* data;
        size_t size;
        size_t used;
    };
    std::vector<Block> blocks;
    size_t defaultBlockSize;

    void allocateNewBlock(size_t minSize);
};

/**
 * Base class for all AST nodes
 */
class ASTNode {
public:
    virtual ~ASTNode() = default;

    uint32_t line = 0;
    uint32_t column = 0;

    enum class Kind {
        Object,
        Array,
        LiteralString,
        CompositeString,
        VariableRef,
        Integer,
        Boolean,
        Null
    };

    virtual Kind getKind() const = 0;
};

/**
 * Key-value pair in an object
 */
struct KeyValuePair {
    std::string key;
    ASTNode* value;
};

/**
 * Object node - represents { key: value, ... }
 */
class ObjectNode : public ASTNode {
public:
    std::vector<KeyValuePair> members;

    Kind getKind() const override { return Kind::Object; }

    /**
     * Find a member by key
     */
    ASTNode* find(const std::string& key) const;

    /**
     * Get string value by key (returns empty if not found or not string)
     */
    std::string getString(const std::string& key, const std::string& defaultVal = "") const;

    /**
     * Get integer value by key
     */
    int64_t getInteger(const std::string& key, int64_t defaultVal = 0) const;

    /**
     * Get boolean value by key
     */
    bool getBoolean(const std::string& key, bool defaultVal = false) const;

    /**
     * Get array value by key
     */
    ArrayNode* getArray(const std::string& key) const;

    /**
     * Get object value by key
     */
    ObjectNode* getObject(const std::string& key) const;
};

/**
 * Array node - represents [ value, value, ... ]
 */
class ArrayNode : public ASTNode {
public:
    std::vector<ASTNode*> elements;

    Kind getKind() const override { return Kind::Array; }

    size_t size() const { return elements.size(); }
    bool empty() const { return elements.empty(); }
};

/**
 * Literal string node - simple string without interpolation
 */
class LiteralStringNode : public ASTNode {
public:
    std::string value;

    explicit LiteralStringNode(std::string val) : value(std::move(val)) {}

    Kind getKind() const override { return Kind::LiteralString; }
};

/**
 * String segment - either literal or variable reference
 */
using StringSegment = std::variant<std::string, std::string>; // literal or var name

/**
 * Composite string node - string with interpolation
 * Represents: `prefix&{var}suffix`
 */
class CompositeStringNode : public ASTNode {
public:
    struct Segment {
        bool isVariable;
        std::string value;  // literal text or variable name
    };
    std::vector<Segment> segments;

    Kind getKind() const override { return Kind::CompositeString; }
};

/**
 * Variable reference node - represents &{name}
 */
class VariableRefNode : public ASTNode {
public:
    std::string name;

    explicit VariableRefNode(std::string n) : name(std::move(n)) {}

    Kind getKind() const override { return Kind::VariableRef; }
};

/**
 * Integer node
 */
class IntegerNode : public ASTNode {
public:
    int64_t value;

    explicit IntegerNode(int64_t val) : value(val) {}

    Kind getKind() const override { return Kind::Integer; }
};

/**
 * Boolean node
 */
class BooleanNode : public ASTNode {
public:
    bool value;

    explicit BooleanNode(bool val) : value(val) {}

    Kind getKind() const override { return Kind::Boolean; }
};

/**
 * Null node
 */
class NullNode : public ASTNode {
public:
    Kind getKind() const override { return Kind::Null; }
};

/**
 * ABC Build Configuration document
 */
struct ABCDocument {
    ObjectNode* project = nullptr;      // project: { ... }
    ObjectNode* variables = nullptr;    // variables: { ... }
    ArrayNode* targets = nullptr;       // targets: [ ... ]

    bool hasProject() const { return project != nullptr; }
    bool hasVariables() const { return variables != nullptr; }
    bool hasTargets() const { return targets != nullptr; }
};

/**
 * Parser class - constructs AST from token stream
 */
class Parser {
public:
    /**
     * Construct parser with arena allocator
     */
    Parser(Lexer& lexer, ArenaAllocator& arena);

    /**
     * Parse the entire ABC document
     */
    ABCDocument parse();

    /**
     * Parse a single value (for testing)
     */
    ASTNode* parseValue();

    /**
     * Get error messages
     */
    const std::vector<std::string>& getErrors() const { return errors; }

    /**
     * Check if there were errors
     */
    bool hasErrors() const { return !errors.empty(); }

private:
    Lexer& lexer;
    ArenaAllocator& arena;
    Token current;
    Token previous;
    std::vector<std::string> errors;
    bool panicMode = false;

    // Token handling
    void advance();
    bool check(TokenType type);
    bool match(TokenType type);
    void consume(TokenType type, const char* message);

    // Error handling
    void error(const char* message);
    void errorAtCurrent(const char* message);
    void synchronize();

    // Parsing methods (match EBNF grammar)
    ObjectNode* parseObject();
    ArrayNode* parseArray();
    ASTNode* parseString();
    ASTNode* parseNumber();
};

} // namespace abc

#endif // ABC_PARSER_HPP
