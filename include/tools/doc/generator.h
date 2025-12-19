#ifndef ARIA_TOOLS_DOC_GENERATOR_H
#define ARIA_TOOLS_DOC_GENERATOR_H

#include "tools/doc/parser.h"
#include <string>
#include <map>

namespace aria {
namespace doc {

/**
 * HTML documentation generator
 */
class DocGenerator {
public:
    DocGenerator();
    ~DocGenerator();
    
    /**
     * Generate HTML documentation for a module
     * @param module The parsed module
     * @param output_dir Output directory for HTML files
     * @return true if generation succeeded
     */
    bool generate(const std::shared_ptr<Module>& module, const std::string& output_dir);
    
    /**
     * Enable/disable verbose output
     */
    void set_verbose(bool verbose) { m_verbose = verbose; }
    
    /**
     * Set whether to include private items
     */
    void set_document_private(bool include) { m_document_private = include; }
    
private:
    bool m_verbose;
    bool m_document_private;
    
    /**
     * Generate index.html for module
     */
    bool generate_module_index(const std::shared_ptr<Module>& module, 
                               const std::string& output_dir);
    
    /**
     * Generate page for a documented item
     */
    bool generate_item_page(const std::shared_ptr<DocumentedItem>& item,
                           const std::string& output_dir);
    
    /**
     * Generate HTML header
     */
    std::string generate_header(const std::string& title);
    
    /**
     * Generate HTML footer
     */
    std::string generate_footer();
    
    /**
     * Generate HTML for doc comment
     */
    std::string generate_doc_html(const DocComment& doc);
    
    /**
     * Convert markdown to HTML (simplified)
     */
    std::string markdown_to_html(const std::string& markdown);
    
    /**
     * Generate HTML for item list
     */
    std::string generate_item_list(const std::vector<std::shared_ptr<DocumentedItem>>& items,
                                   const std::string& title);
};

} // namespace doc
} // namespace aria

#endif // ARIA_TOOLS_DOC_GENERATOR_H
