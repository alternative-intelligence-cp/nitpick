#include "tools/doc/generator.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace npk {
namespace doc {

DocGenerator::DocGenerator()
    : m_verbose(false)
    , m_document_private(false)
{}

DocGenerator::~DocGenerator() {}

bool DocGenerator::generate(const std::shared_ptr<Module>& module, const std::string& output_dir) {
    if (!module) {
        std::cerr << "Error: null module" << std::endl;
        return false;
    }
    
    // Create output directory if it doesn't exist
    try {
        fs::create_directories(output_dir);
    } catch (const std::exception& e) {
        std::cerr << "Error creating output directory: " << e.what() << std::endl;
        return false;
    }
    
    if (m_verbose) {
        std::cout << "Generating documentation for module: " << module->name << std::endl;
        std::cout << "Output directory: " << output_dir << std::endl;
    }
    
    // Generate module index page
    if (!generate_module_index(module, output_dir)) {
        return false;
    }
    
    // Generate pages for each item
    for (const auto& item : module->items) {
        // Skip private items unless requested
        if (!m_document_private && item->visibility == Visibility::PRIVATE) {
            continue;
        }
        
        // Skip IMPL items from standalone pages — their content belongs with their type
        if (item->kind == ItemKind::IMPL) {
            continue;
        }
        
        if (!generate_item_page(item, output_dir)) {
            std::cerr << "Warning: failed to generate page for " << item->name << std::endl;
        }
    }
    
    // Recurse into submodules
    for (const auto& submodule : module->submodules) {
        // Generate submodule's items using the same output directory
        for (const auto& item : submodule->items) {
            if (!m_document_private && item->visibility == Visibility::PRIVATE) {
                continue;
            }
            if (item->kind == ItemKind::IMPL) {
                continue;
            }
            if (!generate_item_page(item, output_dir)) {
                std::cerr << "Warning: failed to generate page for " << item->name << std::endl;
            }
        }
    }
    
    if (m_verbose) {
        std::cout << "Documentation generated successfully!" << std::endl;
    }
    
    return true;
}

bool DocGenerator::generate_module_index(const std::shared_ptr<Module>& module, 
                                         const std::string& output_dir) {
    std::string file_path = output_dir + "/index.html";
    std::ofstream out(file_path);
    
    if (!out) {
        std::cerr << "Error: cannot create " << file_path << std::endl;
        return false;
    }
    
    // Generate page
    out << generate_header(module->name + " - API Documentation");
    
    // Module description
    out << "<div class=\"module-header\">\n";
    out << "<h1>Module: " << module->name << "</h1>\n";
    
    if (module->doc_comment.has_value()) {
        out << generate_doc_html(module->doc_comment.value());
    }
    
    out << "</div>\n\n";
    
    // Group items by kind
    std::map<ItemKind, std::vector<std::shared_ptr<DocumentedItem>>> grouped;
    
    // Collect items from the module itself
    for (const auto& item : module->items) {
        if (!m_document_private && item->visibility == Visibility::PRIVATE) {
            continue;
        }
        if (item->kind == ItemKind::IMPL) continue;
        grouped[item->kind].push_back(item);
    }
    
    // Also collect items from submodules
    for (const auto& submodule : module->submodules) {
        for (const auto& item : submodule->items) {
            if (!m_document_private && item->visibility == Visibility::PRIVATE) {
                continue;
            }
            if (item->kind == ItemKind::IMPL) continue;
            grouped[item->kind].push_back(item);
        }
    }
    
    // Generate sections for each kind
    const std::map<ItemKind, std::string> kind_names = {
        {ItemKind::FUNCTION, "Functions"},
        {ItemKind::STRUCT, "Structs"},
        {ItemKind::ENUM, "Enums"},
        {ItemKind::TRAIT, "Traits"},
        {ItemKind::TYPE_ALIAS, "Type Aliases"},
        {ItemKind::CONST, "Constants"},
    };
    
    for (const auto& [kind, name] : kind_names) {
        if (grouped.count(kind) && !grouped[kind].empty()) {
            out << generate_item_list(grouped[kind], name);
        }
    }
    
    out << generate_footer();
    out.close();
    
    if (m_verbose) {
        std::cout << "Generated: " << file_path << std::endl;
    }
    
    return true;
}

bool DocGenerator::generate_item_page(const std::shared_ptr<DocumentedItem>& item,
                                      const std::string& output_dir) {
    std::string file_path = output_dir + "/" + item->name + ".html";
    std::ofstream out(file_path);
    
    if (!out) {
        return false;
    }
    
    out << generate_header(item->name);
    
    // Item header
    out << "<div class=\"item-header\">\n";
    
    // Item kind badge
    const std::map<ItemKind, std::string> kind_labels = {
        {ItemKind::FUNCTION, "Function"},
        {ItemKind::STRUCT, "Struct"},
        {ItemKind::ENUM, "Enum"},
        {ItemKind::TRAIT, "Trait"},
        {ItemKind::TYPE_ALIAS, "Type Alias"},
        {ItemKind::CONST, "Constant"},
    };
    
    if (kind_labels.count(item->kind)) {
        out << "<span class=\"kind-badge\">" << kind_labels.at(item->kind) << "</span>\n";
    }
    
    out << "<h1>" << item->name << "</h1>\n";
    
    // Signature
    if (!item->signature.empty()) {
        out << "<pre class=\"signature\"><code>" << item->signature << "</code></pre>\n";
    }
    
    out << "</div>\n\n";
    
    // Documentation
    if (item->doc_comment.has_value()) {
        out << generate_doc_html(item->doc_comment.value());
    }
    
    // Members (for structs, enums, traits)
    if (!item->members.empty()) {
        out << "<h2>Members</h2>\n";
        out << "<div class=\"members\">\n";
        
        for (const auto& member : item->members) {
            out << "<div class=\"member\">\n";
            out << "<h3>" << member->name << "</h3>\n";
            
            if (!member->signature.empty()) {
                out << "<pre><code>" << member->signature << "</code></pre>\n";
            }
            
            if (member->doc_comment.has_value()) {
                out << generate_doc_html(member->doc_comment.value());
            }
            
            out << "</div>\n";
        }
        
        out << "</div>\n";
    }
    
    // Source location
    out << "<div class=\"source-location\">\n";
    out << "<p>Defined in: <code>" << item->file_path << ":" << item->line_number << "</code></p>\n";
    out << "</div>\n";
    
    out << generate_footer();
    out.close();
    
    if (m_verbose) {
        std::cout << "Generated: " << file_path << std::endl;
    }
    
    return true;
}

std::string DocGenerator::generate_header(const std::string& title) {
    std::ostringstream ss;
    ss << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)" << title << R"(</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            line-height: 1.6;
            color: #333;
            background: #f5f5f5;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
            background: white;
            min-height: 100vh;
            box-shadow: 0 0 10px rgba(0,0,0,0.1);
        }
        h1 { color: #6a0dad; margin: 20px 0; font-size: 2.5em; }
        h2 { color: #7a1db8; margin: 30px 0 15px 0; border-bottom: 2px solid #6a0dad; padding-bottom: 10px; }
        h3 { color: #8a2dc9; margin: 20px 0 10px 0; }
        .kind-badge {
            display: inline-block;
            background: linear-gradient(135deg, #6a0dad, #9d4edd);
            color: white;
            padding: 5px 15px;
            border-radius: 20px;
            font-size: 0.85em;
            font-weight: bold;
            margin-bottom: 10px;
        }
        pre {
            background: #f8f8f8;
            border: 1px solid #ddd;
            border-left: 4px solid #6a0dad;
            padding: 15px;
            margin: 15px 0;
            overflow-x: auto;
            border-radius: 4px;
        }
        code {
            font-family: 'Courier New', Courier, monospace;
            font-size: 0.95em;
        }
        .signature {
            background: linear-gradient(135deg, #f8f8f8, #f0f0f0);
            border-left-color: #6a0dad;
            border-left-width: 5px;
        }
        .doc-summary {
            font-size: 1.2em;
            color: #555;
            margin: 20px 0;
            padding: 15px;
            background: #f9f9f9;
            border-left: 4px solid #6a0dad;
        }
        .doc-description {
            margin: 20px 0;
            line-height: 1.8;
        }
        .annotation {
            margin: 15px 0;
            padding: 10px;
            background: #f9f9f9;
            border-radius: 4px;
        }
        .annotation-tag {
            font-weight: bold;
            color: #6a0dad;
            display: inline-block;
            min-width: 100px;
        }
        .item-list {
            margin: 20px 0;
        }
        .item-entry {
            padding: 15px;
            margin: 10px 0;
            background: #fafafa;
            border-left: 3px solid #6a0dad;
            border-radius: 4px;
            transition: all 0.3s;
        }
        .item-entry:hover {
            background: #f0f0f0;
            border-left-width: 5px;
            transform: translateX(2px);
        }
        .item-entry a {
            color: #6a0dad;
            text-decoration: none;
            font-weight: bold;
            font-size: 1.1em;
        }
        .item-entry a:hover {
            text-decoration: underline;
        }
        .item-entry .signature-preview {
            color: #666;
            font-family: 'Courier New', Courier, monospace;
            font-size: 0.9em;
            margin-top: 5px;
        }
        .item-entry .summary {
            color: #666;
            margin-top: 8px;
        }
        .source-location {
            margin-top: 30px;
            padding-top: 20px;
            border-top: 1px solid #ddd;
            color: #999;
            font-size: 0.9em;
        }
        .members {
            margin-left: 20px;
        }
        .member {
            margin: 20px 0;
            padding: 15px;
            background: #f9f9f9;
            border-radius: 4px;
        }
        footer {
            margin-top: 50px;
            padding-top: 20px;
            border-top: 2px solid #ddd;
            text-align: center;
            color: #999;
            font-size: 0.9em;
        }
    </style>
</head>
<body>
<div class="container">
)";
    return ss.str();
}

std::string DocGenerator::generate_footer() {
    return R"(
<footer>
    <p>Generated by npk-doc | <a href="https://aria.ai-liberation-platform.org">Aria Language</a></p>
</footer>
</div>
</body>
</html>
)";
}

std::string DocGenerator::generate_doc_html(const DocComment& doc) {
    std::ostringstream ss;
    
    ss << "<div class=\"documentation\">\n";
    
    // Summary
    if (!doc.summary.empty()) {
        ss << "<div class=\"doc-summary\">\n";
        ss << markdown_to_html(doc.summary) << "\n";
        ss << "</div>\n";
    }
    
    // Description
    if (!doc.description.empty()) {
        ss << "<div class=\"doc-description\">\n";
        ss << markdown_to_html(doc.description) << "\n";
        ss << "</div>\n";
    }
    
    // Annotations
    if (!doc.annotations.empty()) {
        ss << "<div class=\"annotations\">\n";
        
        for (const auto& ann : doc.annotations) {
            ss << "<div class=\"annotation\">\n";
            
            const std::map<AnnotationTag, std::string> tag_names = {
                {AnnotationTag::PARAM, "Parameter"},
                {AnnotationTag::RETURN, "Returns"},
                {AnnotationTag::THROWS, "Throws"},
                {AnnotationTag::EXAMPLE, "Example"},
                {AnnotationTag::DEPRECATED, "Deprecated"},
                {AnnotationTag::SEE, "See Also"},
                {AnnotationTag::SINCE, "Since"},
                {AnnotationTag::NOTE, "Note"},
                {AnnotationTag::WARNING, "Warning"},
                {AnnotationTag::TYPE, "Type"},
            };
            
            std::string tag_name = "Info";
            if (tag_names.count(ann.tag)) {
                tag_name = tag_names.at(ann.tag);
            }
            
            ss << "<span class=\"annotation-tag\">" << tag_name;
            if (!ann.name.empty()) {
                ss << " <code>" << ann.name << "</code>";
            }
            ss << ":</span> ";
            
            ss << markdown_to_html(ann.description) << "\n";
            ss << "</div>\n";
        }
        
        ss << "</div>\n";
    }
    
    // Examples
    if (!doc.examples.empty()) {
        ss << "<h2>Examples</h2>\n";
        for (const auto& example : doc.examples) {
            ss << "<pre><code>" << example << "</code></pre>\n";
        }
    }
    
    ss << "</div>\n";
    
    return ss.str();
}

std::string DocGenerator::markdown_to_html(const std::string& markdown) {
    // Simple markdown to HTML conversion
    // This is a basic implementation - for production, use a real markdown library
    
    std::string html = markdown;
    
    // Code blocks (```...```)
    size_t pos = 0;
    while ((pos = html.find("```", pos)) != std::string::npos) {
        size_t end = html.find("```", pos + 3);
        if (end == std::string::npos) break;
        
        std::string code = html.substr(pos + 3, end - pos - 3);
        html.replace(pos, end - pos + 3, "<pre><code>" + code + "</code></pre>");
        pos = end + 20;
    }
    
    // Inline code (`...`)
    pos = 0;
    while ((pos = html.find('`', pos)) != std::string::npos) {
        size_t end = html.find('`', pos + 1);
        if (end == std::string::npos) break;
        
        std::string code = html.substr(pos + 1, end - pos - 1);
        html.replace(pos, end - pos + 1, "<code>" + code + "</code>");
        pos = end + 10;
    }
    
    // Bold (**text**)
    pos = 0;
    while ((pos = html.find("**", pos)) != std::string::npos) {
        size_t end = html.find("**", pos + 2);
        if (end == std::string::npos) break;
        
        std::string text = html.substr(pos + 2, end - pos - 2);
        html.replace(pos, end - pos + 2, "<strong>" + text + "</strong>");
        pos = end + 15;
    }
    
    // Italic (*text*)
    pos = 0;
    while ((pos = html.find('*', pos)) != std::string::npos) {
        size_t end = html.find('*', pos + 1);
        if (end == std::string::npos) break;
        
        std::string text = html.substr(pos + 1, end - pos - 1);
        html.replace(pos, end - pos + 1, "<em>" + text + "</em>");
        pos = end + 9;
    }
    
    // Line breaks
    pos = 0;
    while ((pos = html.find('\n', pos)) != std::string::npos) {
        html.replace(pos, 1, "<br>\n");
        pos += 5;
    }
    
    return html;
}

std::string DocGenerator::generate_item_list(const std::vector<std::shared_ptr<DocumentedItem>>& items,
                                            const std::string& title) {
    std::ostringstream ss;
    
    ss << "<h2>" << title << "</h2>\n";
    ss << "<div class=\"item-list\">\n";
    
    for (const auto& item : items) {
        ss << "<div class=\"item-entry\">\n";
        ss << "<a href=\"" << item->name << ".html\">" << item->name << "</a>\n";
        
        if (!item->signature.empty()) {
            ss << "<div class=\"signature-preview\">" << item->signature << "</div>\n";
        }
        
        if (item->doc_comment.has_value() && !item->doc_comment->summary.empty()) {
            ss << "<div class=\"summary\">" << item->doc_comment->summary << "</div>\n";
        }
        
        ss << "</div>\n";
    }
    
    ss << "</div>\n";
    
    return ss.str();
}

} // namespace doc
} // namespace npk
