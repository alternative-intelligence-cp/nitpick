#include "tools/doc/parser.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <iostream>

namespace aria {
namespace doc {

// Helper: Trim whitespace from string
static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// Helper: Split string by delimiter
static std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    return result;
}

// Helper: Join strings
static std::string join(const std::vector<std::string>& vec, const std::string& delimiter) {
    std::string result;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) result += delimiter;
        result += vec[i];
    }
    return result;
}

// DocComment helper methods
std::vector<Annotation> DocComment::get_annotations(AnnotationTag tag) const {
    std::vector<Annotation> result;
    for (const auto& ann : annotations) {
        if (ann.tag == tag) {
            result.push_back(ann);
        }
    }
    return result;
}

std::optional<Annotation> DocComment::get_first_annotation(AnnotationTag tag) const {
    for (const auto& ann : annotations) {
        if (ann.tag == tag) {
            return ann;
        }
    }
    return std::nullopt;
}

bool DocComment::has_annotation(AnnotationTag tag) const {
    for (const auto& ann : annotations) {
        if (ann.tag == tag) {
            return true;
        }
    }
    return false;
}

// DocParser implementation
DocParser::DocParser() : m_verbose(false) {
}

DocParser::~DocParser() {
}

std::string DocParser::read_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + file_path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::shared_ptr<Module> DocParser::parse_file(const std::string& file_path) {
    if (m_verbose) {
        std::cout << "Parsing file: " << file_path << std::endl;
    }
    
    auto module = std::make_shared<Module>();
    module->path = file_path;
    
    // Extract module name from file path
    size_t last_slash = file_path.find_last_of("/\\");
    size_t dot = file_path.find_last_of(".");
    if (last_slash != std::string::npos && dot != std::string::npos) {
        module->name = file_path.substr(last_slash + 1, dot - last_slash - 1);
    } else {
        module->name = file_path;
    }
    
    // Read source file
    std::string source = read_file(file_path);
    
    // Extract comments and code
    auto comments_and_code = extract_comments_and_code(source, file_path);
    
    // Parse each item
    int line_number = 1;
    for (const auto& [doc, code] : comments_and_code) {
        if (code.empty()) {
            // Module-level doc comment
            if (doc && (doc->type == DocCommentType::INNER_SINGLE || 
                       doc->type == DocCommentType::INNER_MULTI)) {
                module->doc_comment = doc;
            }
            continue;
        }
        
        auto item = parse_item_declaration(code, doc, file_path, line_number);
        if (item) {
            module->items.push_back(item);
        }
        
        // Update line number
        line_number += std::count(code.begin(), code.end(), '\n');
    }
    
    return module;
}

std::shared_ptr<Module> DocParser::parse_package(const std::vector<std::string>& file_paths) {
    auto root = std::make_shared<Module>();
    root->name = "package";
    
    for (const auto& file_path : file_paths) {
        auto module = parse_file(file_path);
        root->submodules.push_back(module);
    }
    
    return root;
}

std::vector<std::pair<std::optional<DocComment>, std::string>> 
DocParser::extract_comments_and_code(const std::string& source, const std::string& file_path) {
    std::vector<std::pair<std::optional<DocComment>, std::string>> result;
    
    std::vector<std::string> lines = split(source, '\n');
    
    std::optional<DocComment> current_doc;
    std::vector<std::string> doc_lines;
    DocCommentType doc_type = DocCommentType::SINGLE_LINE;
    int doc_start_line = 0;
    bool in_multiline_doc = false;
    std::vector<std::string> code_lines;
    
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string line = lines[i];
        std::string trimmed = trim(line);
        
        // Check for doc comments
        if (trimmed.rfind("///", 0) == 0) {  // starts_with equivalent
            if (!doc_lines.empty() && doc_type != DocCommentType::SINGLE_LINE) {
                // Different doc type, finish previous
                if (!code_lines.empty()) {
                    result.push_back({current_doc, join(code_lines, "\n")});
                    code_lines.clear();
                }
                current_doc = std::nullopt;
                doc_lines.clear();
            }
            
            doc_type = DocCommentType::SINGLE_LINE;
            if (doc_lines.empty()) {
                doc_start_line = i + 1;
            }
            doc_lines.push_back(trimmed.substr(3));  // Remove ///
        }
        else if (trimmed.rfind("//!", 0) == 0) {
            doc_type = DocCommentType::INNER_SINGLE;
            if (doc_lines.empty()) {
                doc_start_line = i + 1;
            }
            doc_lines.push_back(trimmed.substr(3));  // Remove //!
        }
        else if (trimmed.rfind("/**", 0) == 0) {
            in_multiline_doc = true;
            doc_type = DocCommentType::MULTI_LINE;
            doc_start_line = i + 1;
            
            std::string content = trimmed.substr(3);
            if (content.size() >= 2 && content.substr(content.size() - 2) == "*/") {
                content = content.substr(0, content.length() - 2);
                in_multiline_doc = false;
            }
            doc_lines.push_back(content);
        }
        else if (trimmed.rfind("/*!", 0) == 0) {
            in_multiline_doc = true;
            doc_type = DocCommentType::INNER_MULTI;
            doc_start_line = i + 1;
            
            std::string content = trimmed.substr(3);
            if (content.size() >= 2 && content.substr(content.size() - 2) == "*/") {
                content = content.substr(0, content.length() - 2);
                in_multiline_doc = false;
            }
            doc_lines.push_back(content);
        }
        else if (in_multiline_doc) {
            if (trimmed.size() >= 2 && trimmed.substr(trimmed.size() - 2) == "*/") {
                std::string content = trimmed.substr(0, trimmed.length() - 2);
                // Remove leading * if present
                if (!content.empty() && content[0] == '*') {
                    content = content.substr(1);
                }
                doc_lines.push_back(trim(content));
                in_multiline_doc = false;
            } else {
                // Remove leading * if present
                std::string content = trimmed;
                if (!content.empty() && content[0] == '*') {
                    content = content.substr(1);
                }
                doc_lines.push_back(trim(content));
            }
        }
        else {
            // Regular code line
            if (!doc_lines.empty()) {
                // Finish doc comment
                std::string doc_text = join(doc_lines, "\n");
                
                current_doc = parse_doc_comment(doc_text, doc_type);
                current_doc->file_path = file_path;
                current_doc->start_line = doc_start_line;
                current_doc->end_line = i;
                
                doc_lines.clear();
            }
            
            // Skip empty lines and regular comments
            if (trimmed.empty() || trimmed.rfind("//", 0) == 0) {
                continue;
            }
            
            code_lines.push_back(line);
            
            // Check if this line ends a declaration (simplified)
            if (!trimmed.empty() && (trimmed[trimmed.size() - 1] == '}' || trimmed[trimmed.size() - 1] == ';')) {
                result.push_back({current_doc, join(code_lines, "\n")});
                code_lines.clear();
                current_doc = std::nullopt;
            }
        }
    }
    
    // Handle remaining code
    if (!code_lines.empty()) {
        result.push_back({current_doc, join(code_lines, "\n")});
    }
    
    return result;
}

DocComment DocParser::parse_doc_comment(const std::string& comment, DocCommentType type) {
    DocComment doc;
    doc.type = type;
    
    std::vector<std::string> lines = split(comment, '\n');
    std::vector<std::string> text_lines;
    
    for (const auto& line : lines) {
        std::string trimmed = trim(line);
        
        // Check for annotation
        auto annotation = parse_annotation(trimmed);
        if (annotation) {
            doc.annotations.push_back(*annotation);
            continue;
        }
        
        text_lines.push_back(trimmed);
    }
    
    // Join text lines and split into summary/description
    std::string full_text = join(text_lines, "\n");
    split_summary_description(full_text, doc.summary, doc.description);
    
    // Extract code blocks
    doc.examples = extract_code_blocks(full_text);
    
    return doc;
}

std::optional<Annotation> DocParser::parse_annotation(const std::string& line) {
    if (line.empty() || line[0] != '@') {
        return std::nullopt;
    }
    
    Annotation ann;
    
    // Parse tag
    size_t space = line.find(' ');
    std::string tag_str = line.substr(1, space - 1);
    
    if (tag_str == "param" || tag_str == "parameter") {
        ann.tag = AnnotationTag::PARAM;
    } else if (tag_str == "return" || tag_str == "returns") {
        ann.tag = AnnotationTag::RETURN;
    } else if (tag_str == "throws" || tag_str == "panics") {
        ann.tag = AnnotationTag::THROWS;
    } else if (tag_str == "example" || tag_str == "examples") {
        ann.tag = AnnotationTag::EXAMPLE;
    } else if (tag_str == "deprecated") {
        ann.tag = AnnotationTag::DEPRECATED;
    } else if (tag_str == "see" || tag_str == "seealso") {
        ann.tag = AnnotationTag::SEE;
    } else if (tag_str == "since") {
        ann.tag = AnnotationTag::SINCE;
    } else if (tag_str == "note") {
        ann.tag = AnnotationTag::NOTE;
    } else if (tag_str == "warning") {
        ann.tag = AnnotationTag::WARNING;
    } else if (tag_str == "type") {
        ann.tag = AnnotationTag::TYPE;
    } else {
        return std::nullopt;  // Unknown tag
    }
    
    if (space != std::string::npos) {
        std::string rest = trim(line.substr(space + 1));
        
        // For @param, first word is parameter name
        if (ann.tag == AnnotationTag::PARAM) {
            size_t name_end = rest.find(' ');
            if (name_end != std::string::npos) {
                ann.name = rest.substr(0, name_end);
                ann.description = trim(rest.substr(name_end + 1));
            } else {
                ann.name = rest;
            }
        } else {
            ann.description = rest;
        }
    }
    
    return ann;
}

std::vector<std::string> DocParser::extract_code_blocks(const std::string& text) {
    std::vector<std::string> blocks;
    
    std::regex code_block_regex(R"(```(?:aria)?\s*\n(.*?)```)");
    auto begin = std::sregex_iterator(text.begin(), text.end(), code_block_regex);
    auto end = std::sregex_iterator();
    
    for (auto it = begin; it != end; ++it) {
        blocks.push_back((*it)[1].str());
    }
    
    return blocks;
}

void DocParser::split_summary_description(const std::string& text, 
                                         std::string& summary, 
                                         std::string& description) {
    // First paragraph is summary, rest is description
    size_t double_newline = text.find("\n\n");
    
    if (double_newline != std::string::npos) {
        summary = trim(text.substr(0, double_newline));
        description = trim(text.substr(double_newline + 2));
    } else {
        summary = trim(text);
        description = "";
    }
}

std::shared_ptr<DocumentedItem> DocParser::parse_item_declaration(
    const std::string& code,
    const std::optional<DocComment>& doc,
    const std::string& file_path,
    int line_number
) {
    auto item = std::make_shared<DocumentedItem>();
    item->file_path = file_path;
    item->line_number = line_number;
    item->doc_comment = doc;
    
    // Determine item kind
    item->kind = determine_item_kind(code);
    
    // Extract visibility
    item->visibility = extract_visibility(code);
    
    // Extract name
    item->name = extract_item_name(code, item->kind);
    
    // Extract signature for functions
    if (item->kind == ItemKind::FUNCTION) {
        item->signature = extract_function_signature(code);
    }
    
    // Check for deprecated
    if (doc) {
        item->is_deprecated = doc->has_annotation(AnnotationTag::DEPRECATED);
        item->since_version = extract_since_version(*doc);
    }
    
    return item;
}

ItemKind DocParser::determine_item_kind(const std::string& code) {
    std::string trimmed = trim(code);
    
    if (trimmed.find("fn ") != std::string::npos) {
        return ItemKind::FUNCTION;
    } else if (trimmed.find("struct ") != std::string::npos) {
        return ItemKind::STRUCT;
    } else if (trimmed.find("enum ") != std::string::npos) {
        return ItemKind::ENUM;
    } else if (trimmed.find("trait ") != std::string::npos) {
        return ItemKind::TRAIT;
    } else if (trimmed.find("impl ") != std::string::npos) {
        return ItemKind::IMPL;
    } else if (trimmed.find("type ") != std::string::npos) {
        return ItemKind::TYPE_ALIAS;
    } else if (trimmed.find("const ") != std::string::npos) {
        return ItemKind::CONST;
    }
    
    return ItemKind::VARIABLE;
}

std::string DocParser::extract_item_name(const std::string& code, ItemKind kind) {
    std::regex name_regex;
    
    switch (kind) {
        case ItemKind::FUNCTION:
            name_regex = std::regex(R"(fn\s+([a-zA-Z_][a-zA-Z0-9_]*))");
            break;
        case ItemKind::STRUCT:
            name_regex = std::regex(R"(struct\s+([a-zA-Z_][a-zA-Z0-9_]*))");
            break;
        case ItemKind::ENUM:
            name_regex = std::regex(R"(enum\s+([a-zA-Z_][a-zA-Z0-9_]*))");
            break;
        case ItemKind::TRAIT:
            name_regex = std::regex(R"(trait\s+([a-zA-Z_][a-zA-Z0-9_]*))");
            break;
        default:
            return "unknown";
    }
    
    std::smatch match;
    if (std::regex_search(code, match, name_regex)) {
        return match[1].str();
    }
    
    return "unknown";
}

std::string DocParser::extract_function_signature(const std::string& code) {
    // Extract from 'fn' to closing ')'
    size_t fn_pos = code.find("fn ");
    if (fn_pos == std::string::npos) return code;
    
    size_t paren_count = 0;
    size_t end_pos = fn_pos;
    bool found_open = false;
    
    for (size_t i = fn_pos; i < code.length(); ++i) {
        if (code[i] == '(') {
            found_open = true;
            paren_count++;
        } else if (code[i] == ')') {
            paren_count--;
            if (paren_count == 0 && found_open) {
                // Continue to include return type
                size_t arrow = code.find("->", i);
                if (arrow != std::string::npos) {
                    size_t brace = code.find('{', arrow);
                    if (brace != std::string::npos) {
                        end_pos = brace;
                    } else {
                        end_pos = code.length();
                    }
                } else {
                    end_pos = i + 1;
                }
                break;
            }
        }
    }
    
    return trim(code.substr(fn_pos, end_pos - fn_pos));
}

Visibility DocParser::extract_visibility(const std::string& code) {
    std::string trimmed = trim(code);
    
    if (trimmed.rfind("pub ", 0) == 0) {  // starts_with equivalent
        return Visibility::PUBLIC;
    } else if (trimmed.rfind("internal ", 0) == 0) {
        return Visibility::INTERNAL;
    }
    
    return Visibility::PRIVATE;
}

std::optional<std::string> DocParser::extract_since_version(const DocComment& doc) {
    auto since = doc.get_first_annotation(AnnotationTag::SINCE);
    if (since) {
        return since->description;
    }
    return std::nullopt;
}

std::vector<std::shared_ptr<DocumentedItem>> DocParser::extract_members(
    const std::string& code, 
    ItemKind container_kind
) {
    // Simplified member extraction - would need full AST parsing for complete implementation
    std::vector<std::shared_ptr<DocumentedItem>> members;
    return members;
}

} // namespace doc
} // namespace aria
