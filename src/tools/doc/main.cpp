#include "tools/doc/parser.h"
#include "tools/doc/generator.h"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

void print_usage(const char* program_name) {
    std::cout << "aria-doc - Aria Documentation Generator\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS] <files...>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o, --output DIR        Output directory (default: ./docs)\n";
    std::cout << "  --document-private      Include private items in documentation\n";
    std::cout << "  -v, --verbose           Enable verbose output\n";
    std::cout << "  -h, --help              Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " src/main.aria\n";
    std::cout << "  " << program_name << " src/*.aria -o api_docs\n";
    std::cout << "  " << program_name << " -v --document-private src/*.aria\n";
}

int main(int argc, char* argv[]) {
    // Parse arguments
    std::vector<std::string> input_files;
    std::string output_dir = "./docs";
    bool document_private = false;
    bool verbose = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        }
        else if (arg == "--document-private") {
            document_private = true;
        }
        else if (arg == "-o" || arg == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires an argument" << std::endl;
                return 1;
            }
            output_dir = argv[++i];
        }
        else if (arg[0] == '-') {
            std::cerr << "Error: unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
        else {
            // Input file or glob pattern
            input_files.push_back(arg);
        }
    }
    
    // Validate input
    if (input_files.empty()) {
        std::cerr << "Error: no input files specified" << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    // Expand file patterns and validate files exist
    std::vector<std::string> expanded_files;
    for (const auto& pattern : input_files) {
        // Check if it's a direct file
        if (fs::exists(pattern) && fs::is_regular_file(pattern)) {
            expanded_files.push_back(pattern);
        }
        // Check if it's a directory (process all .aria files)
        else if (fs::exists(pattern) && fs::is_directory(pattern)) {
            for (const auto& entry : fs::recursive_directory_iterator(pattern)) {
                if (entry.is_regular_file() &&
                (entry.path().extension() == ".aria" || entry.path().extension() == ".npk")) {
                    expanded_files.push_back(entry.path().string());
                }
            }
        }
        // Handle glob patterns (basic wildcard support)
        else if (pattern.find('*') != std::string::npos) {
            // Get directory and filename pattern
            fs::path p(pattern);
            fs::path dir = p.parent_path();
            if (dir.empty()) dir = ".";
            
            std::string filename_pattern = p.filename().string();
            
            try {
                for (const auto& entry : fs::directory_iterator(dir)) {
                    if (entry.is_regular_file()) {
                        std::string filename = entry.path().filename().string();
                        
                        // Simple wildcard matching (*.aria)
                        if (filename_pattern == "*" || 
                            (filename_pattern.find("*.") == 0 && 
                             entry.path().extension() == filename_pattern.substr(1))) {
                            expanded_files.push_back(entry.path().string());
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Warning: could not expand pattern '" << pattern << "': " << e.what() << std::endl;
            }
        }
        else {
            std::cerr << "Error: file not found: " << pattern << std::endl;
            return 1;
        }
    }
    
    if (expanded_files.empty()) {
        std::cerr << "Error: no valid .aria files found" << std::endl;
        return 1;
    }
    
    // Filter to only .aria files
    std::vector<std::string> aria_files;
    for (const auto& file : expanded_files) {
        if ((file.size() >= 5 && file.substr(file.size() - 5) == ".aria") ||
            (file.size() >= 4 && file.substr(file.size() - 4) == ".npk")) {
            aria_files.push_back(file);
        }
    }
    
    if (aria_files.empty()) {
        std::cerr << "Error: no .aria files found in input" << std::endl;
        return 1;
    }
    
    if (verbose) {
        std::cout << "aria-doc - Aria Documentation Generator\n";
        std::cout << "========================================\n\n";
        std::cout << "Input files (" << aria_files.size() << "):\n";
        for (const auto& file : aria_files) {
            std::cout << "  - " << file << "\n";
        }
        std::cout << "\nOutput directory: " << output_dir << "\n";
        std::cout << "Document private items: " << (document_private ? "yes" : "no") << "\n";
        std::cout << "\nParsing source files...\n";
    }
    
    // Parse source files
    npk::doc::DocParser parser;
    parser.set_verbose(verbose);
    
    std::shared_ptr<npk::doc::Module> module;
    
    if (aria_files.size() == 1) {
        // Single file
        module = parser.parse_file(aria_files[0]);
    } else {
        // Multiple files (package)
        module = parser.parse_package(aria_files);
    }
    
    if (!module) {
        std::cerr << "Error: failed to parse source files" << std::endl;
        return 1;
    }
    
    if (verbose) {
        std::cout << "\nGenerating HTML documentation...\n";
    }
    
    // Generate documentation
    npk::doc::DocGenerator generator;
    generator.set_verbose(verbose);
    generator.set_document_private(document_private);
    
    if (!generator.generate(module, output_dir)) {
        std::cerr << "Error: failed to generate documentation" << std::endl;
        return 1;
    }
    
    std::cout << "\n✓ Documentation generated successfully!\n";
    std::cout << "  Output: " << fs::absolute(output_dir).string() << "\n";
    std::cout << "  Open: " << fs::absolute(output_dir).string() << "/index.html\n";
    
    return 0;
}
