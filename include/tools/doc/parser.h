#ifndef ARIA_TOOLS_DOC_PARSER_H
#define ARIA_TOOLS_DOC_PARSER_H

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>

namespace aria {
namespace doc {

/**
 * Type of documentation comment
 */
enum class DocCommentType {
    SINGLE_LINE,    // /// comment
    MULTI_LINE,     // /** comment */
    INNER_SINGLE,   // //! module doc
    INNER_MULTI,    // /*! module doc */
};

/**
 * Special annotation tags in doc comments
 */
enum class AnnotationTag {
    PARAM,          // @param
    RETURN,         // @return / @returns
    THROWS,         // @throws / @panics
    EXAMPLE,        // @example / @examples
    DEPRECATED,     // @deprecated
    SEE,            // @see / @seealso
    SINCE,          // @since
    NOTE,           // @note
    WARNING,        // @warning
    TYPE,           // @type (for generics)
};

/**
 * A single annotation within a doc comment
 */
struct Annotation {
    AnnotationTag tag;
    std::string name;       // Parameter name for @param, etc.
    std::string description;
};

/**
 * Parsed documentation comment
 */
struct DocComment {
    DocCommentType type;
    
    // Main documentation text (without annotations)
    std::string summary;        // First paragraph
    std::string description;    // Detailed description
    
    // Annotations
    std::vector<Annotation> annotations;
    
    // Code examples (from @example or ```blocks```)
    std::vector<std::string> examples;
    
    // Source location
    std::string file_path;
    int start_line;
    int end_line;
    
    // Helper methods
    std::vector<Annotation> get_annotations(AnnotationTag tag) const;
    std::optional<Annotation> get_first_annotation(AnnotationTag tag) const;
    bool has_annotation(AnnotationTag tag) const;
};

/**
 * Type of documented item
 */
enum class ItemKind {
    MODULE,
    FUNCTION,
    STRUCT,
    ENUM,
    TRAIT,
    IMPL,
    TYPE_ALIAS,
    CONST,
    VARIABLE,
};

/**
 * Visibility of an item
 */
enum class Visibility {
    PUBLIC,
    PRIVATE,
    INTERNAL,
};

/**
 * A documented item (function, struct, etc.)
 */
struct DocumentedItem {
    ItemKind kind;
    Visibility visibility;
    
    std::string name;
    std::string qualified_name;  // module::item
    std::string signature;       // Full signature with types
    
    // Documentation
    std::optional<DocComment> doc_comment;
    
    // Source location
    std::string file_path;
    int line_number;
    
    // For containers (structs, enums, traits, impls)
    std::vector<std::shared_ptr<DocumentedItem>> members;
    
    // For generic items
    std::vector<std::string> type_parameters;
    
    // For traits
    std::vector<std::string> super_traits;
    
    // For impl blocks
    std::optional<std::string> trait_name;
    std::optional<std::string> type_name;
    
    // Attributes/metadata
    bool is_deprecated = false;
    bool is_async = false;
    bool is_unsafe = false;
    std::optional<std::string> since_version;
};

/**
 * A module with its documentation
 */
struct Module {
    std::string name;
    std::string path;
    std::optional<DocComment> doc_comment;
    
    std::vector<std::shared_ptr<Module>> submodules;
    std::vector<std::shared_ptr<DocumentedItem>> items;
};

/**
 * DocParser extracts documentation comments from Aria source files
 */
class DocParser {
public:
    DocParser();
    ~DocParser();
    
    /**
     * Parse a source file and extract documentation
     * @param file_path Path to .aria source file
     * @return Parsed module with documentation
     */
    std::shared_ptr<Module> parse_file(const std::string& file_path);
    
    /**
     * Parse multiple files (a package)
     * @param file_paths Vector of .aria source files
     * @return Root module with submodules
     */
    std::shared_ptr<Module> parse_package(const std::vector<std::string>& file_paths);
    
    /**
     * Parse a documentation comment string
     * @param comment The comment text (without comment markers)
     * @param type Type of doc comment
     * @return Parsed doc comment
     */
    DocComment parse_doc_comment(const std::string& comment, DocCommentType type);
    
    /**
     * Enable/disable verbose output
     */
    void set_verbose(bool verbose) { m_verbose = verbose; }
    
private:
    bool m_verbose;
    
    /**
     * Read source file contents
     */
    std::string read_file(const std::string& file_path);
    
    /**
     * Extract doc comments and code structure from source
     */
    std::vector<std::pair<std::optional<DocComment>, std::string>> 
        extract_comments_and_code(const std::string& source, const std::string& file_path);
    
    /**
     * Parse item declaration (fn, struct, enum, etc.)
     */
    std::shared_ptr<DocumentedItem> parse_item_declaration(
        const std::string& code,
        const std::optional<DocComment>& doc,
        const std::string& file_path,
        int line_number
    );
    
    /**
     * Determine item kind from code
     */
    ItemKind determine_item_kind(const std::string& code);
    
    /**
     * Extract item name from declaration
     */
    std::string extract_item_name(const std::string& code, ItemKind kind);
    
    /**
     * Extract function signature
     */
    std::string extract_function_signature(const std::string& code);
    
    /**
     * Extract struct/enum members
     */
    std::vector<std::shared_ptr<DocumentedItem>> 
        extract_members(const std::string& code, ItemKind container_kind);
    
    /**
     * Parse annotation from line (e.g., @param x The parameter)
     */
    std::optional<Annotation> parse_annotation(const std::string& line);
    
    /**
     * Parse markdown code block (```aria ... ```)
     */
    std::vector<std::string> extract_code_blocks(const std::string& text);
    
    /**
     * Split comment into summary and description
     */
    void split_summary_description(const std::string& text, 
                                   std::string& summary, 
                                   std::string& description);
    
    /**
     * Check if code line contains visibility modifier
     */
    Visibility extract_visibility(const std::string& code);
    
    /**
     * Check for deprecated attribute
     */
    bool is_deprecated(const std::string& code, const DocComment& doc);
    
    /**
     * Extract since version from annotations
     */
    std::optional<std::string> extract_since_version(const DocComment& doc);
};

} // namespace doc
} // namespace aria

#endif // ARIA_TOOLS_DOC_PARSER_H
