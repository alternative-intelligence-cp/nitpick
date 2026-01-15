/**
 * Aria Compiler Driver
 * 
 * Main entry point for the Aria compiler (ariac).
 * Handles command-line arguments, orchestrates compilation pipeline,
 * and produces executables or intermediate outputs.
 * 
 * Usage:
 *   ariac input.aria -o output           # Compile to executable
 *   ariac input.aria --emit-llvm         # Emit LLVM IR (.ll)
 *   ariac input.aria --emit-llvm-bc      # Emit LLVM bitcode (.bc)
 *   ariac input.aria --ast-dump          # Dump AST
 *   ariac input.aria --tokens            # Dump tokens
 *   ariac --version                      # Show version
 *   ariac --help                         # Show help
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <memory>
#include <cstdlib>
#include <unistd.h>  // For readlink
#include <filesystem> // For absolute path resolution

// Aria compiler components
#include "frontend/preprocessor/preprocessor.h"  // NEW: Phase 0
#include "frontend/lexer/lexer.h"
#include "frontend/parser/parser.h"
#include "frontend/sema/type_checker.h"
#include "frontend/sema/generic_resolver.h"
#include "frontend/sema/borrow_checker.h"
#include "frontend/sema/module_loader.h"  // Module system
#include "frontend/diagnostics.h"
#include "frontend/warnings.h"
#include "backend/ir/ir_generator.h"

// LLVM
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/CodeGen.h"  // CodeGenOptLevel for large integer support

// LLVM New Pass Manager (for coroutine lowering)
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Transforms/Coroutines/CoroEarly.h"
#include "llvm/Transforms/Coroutines/CoroSplit.h"
#include "llvm/Transforms/Coroutines/CoroElide.h"
#include "llvm/Transforms/Coroutines/CoroCleanup.h"

// Version information
#define ARIA_VERSION_MAJOR 0
#define ARIA_VERSION_MINOR 1
#define ARIA_VERSION_PATCH 0
#define ARIA_VERSION "0.1.0"

// Compiler options
struct CompilerOptions {
    std::vector<std::string> input_files;  // Support multiple source files
    std::string output_file;
    bool emit_llvm_ir = false;
    bool emit_llvm_bc = false;
    bool emit_asm = false;
    bool emit_deps = false;           // ARIA-011: Emit JSON dependency manifest
    bool dump_ast = false;
    bool dump_tokens = false;
    bool preprocess_only = false;  // NEW: -E flag for preprocessed output
    bool verbose = false;
    int opt_level = 0;  // -O0, -O1, -O2, -O3
    std::vector<std::string> warning_flags;  // -Wall, -Werror, -W<warning>, etc.
    std::vector<std::string> include_paths;   // -I<dir> module search paths
    std::vector<std::string> link_libraries;  // -l<lib> libraries to link
    std::vector<std::string> library_paths;   // -L<path> library search paths
    std::vector<std::string> linker_flags;    // -Wl,<option> flags passed to linker
};

/**
 * Print version information
 */
void print_version() {
    std::cout << "Aria Compiler (ariac) version " << ARIA_VERSION << "\n";
    std::cout << "Built with LLVM " << LLVM_VERSION_STRING << "\n";
}

/**
 * Print usage information
 */
void print_help() {
    std::cout << "Aria Compiler (ariac) - Compile Aria source files\n\n";
    std::cout << "Usage:\n";
    std::cout << "  ariac <input.aria> [<input2.aria> ...] [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -o <file>         Write output to <file>\n";
    std::cout << "  -I <dir>          Add directory to module search path\n";
    std::cout << "  -E                Preprocess only (output to stdout)\n";
    std::cout << "  --emit-llvm       Emit LLVM IR text (.ll)\n";
    std::cout << "  --emit-llvm-bc    Emit LLVM bitcode (.bc)\n";
    std::cout << "  --emit-asm        Emit assembly (.s)\n";
    std::cout << "  --emit-deps       Emit JSON dependency manifest (for aria_make)\n";
    std::cout << "  --ast-dump        Dump AST and exit\n";
    std::cout << "  --tokens          Dump tokens and exit\n";
    std::cout << "  -O<level>         Optimization level (0-3)\n";
    std::cout << "  -v, --verbose     Verbose output\n";
    std::cout << "  -l<library>       Link against library (e.g., -lm, -lpthread)\n";
    std::cout << "  -L<path>          Add library search path\n";
    std::cout << "  -Wl,<option>      Pass option to linker\n";
    std::cout << "  -Wall             Enable all warnings\n";
    std::cout << "  -Werror           Treat warnings as errors\n";
    std::cout << "  -W<warning>       Enable specific warning\n";
    std::cout << "  -Wno-<warning>    Disable specific warning\n";
    std::cout << "  --version         Show version\n";
    std::cout << "  --help            Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  ariac hello.aria -o hello\n";
    std::cout << "  ariac main.aria utils.aria -o program\n";
    std::cout << "  ariac program.aria -o program -lm -lpthread\n";
    std::cout << "  ariac program.aria --emit-llvm -o program.ll\n";
    std::cout << "  ariac test.aria --ast-dump\n";
}

/**
 * Parse command-line arguments
 */
bool parse_arguments(int argc, char** argv, CompilerOptions& opts) {
    if (argc < 2) {
        std::cerr << "Error: No input file specified\n";
        std::cerr << "Run 'ariac --help' for usage information\n";
        return false;
    }

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--version") {
            print_version();
            std::exit(0);
        } else if (arg == "--help" || arg == "-h") {
            print_help();
            std::exit(0);
        } else if (arg == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -o requires an argument\n";
                return false;
            }
            opts.output_file = argv[++i];
        } else if (arg == "-I") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -I requires an argument\n";
                return false;
            }
            opts.include_paths.push_back(argv[++i]);
        } else if (arg == "--emit-llvm") {
            opts.emit_llvm_ir = true;
        } else if (arg == "--emit-llvm-bc") {
            opts.emit_llvm_bc = true;
        } else if (arg == "--emit-asm") {
            opts.emit_asm = true;
        } else if (arg == "--emit-deps") {
            opts.emit_deps = true;
        } else if (arg == "-E") {
            opts.preprocess_only = true;
        } else if (arg == "--ast-dump") {
            opts.dump_ast = true;
        } else if (arg == "--tokens") {
            opts.dump_tokens = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg.substr(0, 2) == "-O") {
            if (arg.length() == 3 && arg[2] >= '0' && arg[2] <= '3') {
                opts.opt_level = arg[2] - '0';
            } else {
                std::cerr << "Error: Invalid optimization level: " << arg << "\n";
                return false;
            }
        } else if (arg.substr(0, 2) == "-l") {
            // Link library: -lm, -lpthread, etc.
            if (arg.length() > 2) {
                opts.link_libraries.push_back(arg.substr(2));
            } else {
                std::cerr << "Error: -l requires a library name\n";
                return false;
            }
        } else if (arg.substr(0, 2) == "-L") {
            // Library search path: -L/usr/local/lib, etc.
            if (arg.length() > 2) {
                opts.library_paths.push_back(arg.substr(2));
            } else if (i + 1 < argc) {
                opts.library_paths.push_back(argv[++i]);
            } else {
                std::cerr << "Error: -L requires a path\n";
                return false;
            }
        } else if (arg.substr(0, 4) == "-Wl,") {
            // Linker flag: -Wl,-rpath,/usr/local/lib, etc.
            opts.linker_flags.push_back(arg.substr(4));
        } else if (arg.substr(0, 2) == "-W") {
            // Warning flags: -Wall, -Werror, -Wunused-variable, -Wno-dead-code, etc.
            opts.warning_flags.push_back(arg);
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            return false;
        } else {
            // Collect input files
            opts.input_files.push_back(arg);
        }
    }

    if (opts.input_files.empty()) {
        std::cerr << "Error: No input file specified\n";
        return false;
    }

    // Set default output file if not specified
    if (opts.output_file.empty()) {
        if (opts.emit_llvm_ir) {
            opts.output_file = opts.input_files[0].substr(0, opts.input_files[0].find_last_of('.')) + ".ll";
        } else if (opts.emit_llvm_bc) {
            opts.output_file = opts.input_files[0].substr(0, opts.input_files[0].find_last_of('.')) + ".bc";
        } else if (opts.emit_asm) {
            opts.output_file = opts.input_files[0].substr(0, opts.input_files[0].find_last_of('.')) + ".s";
        } else {
            opts.output_file = "a.out";
        }
    }

    return true;
}

/**
 * Read source file
 */
bool read_source_file(const std::string& filename, std::string& source, aria::DiagnosticEngine& diags) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        diags.fatal(aria::SourceLocation(filename, 0, 0), "could not open source file");
        diags.addNote("check that the file exists and you have read permissions");
        return false;
    }

    source.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

/**
 * ARIA-011: Extract and emit dependencies as JSON
 *
 * Parses the source file and extracts all 'use' statements,
 * resolving them to file paths. Outputs JSON to stdout.
 *
 * Output format:
 * {
 *   "source": "/path/to/file.aria",
 *   "imports": [
 *     {"module": "std.io", "path": "/path/to/std/io.aria"},
 *     {"module": "./utils", "path": "/path/to/utils.aria"}
 *   ],
 *   "error": null
 * }
 */
bool emit_dependencies(
    const std::string& source,
    const std::string& filename,
    const CompilerOptions& opts,
    aria::DiagnosticEngine& diags
) {
    // Phase 0: Preprocessing
    std::string preprocessed_source = source;
    try {
        aria::frontend::Preprocessor preprocessor;
        preprocessed_source = preprocessor.process(source, filename);
    } catch (const std::exception& e) {
        // Output error as JSON
        std::cout << "{\n";
        std::cout << "  \"source\": \"" << filename << "\",\n";
        std::cout << "  \"imports\": [],\n";
        std::cout << "  \"error\": \"Preprocessor error: " << e.what() << "\"\n";
        std::cout << "}\n";
        return false;
    }

    // Phase 1: Lexical Analysis
    aria::frontend::Lexer lexer(preprocessed_source);
    auto tokens = lexer.tokenize();

    if (!lexer.getErrors().empty()) {
        std::cout << "{\n";
        std::cout << "  \"source\": \"" << filename << "\",\n";
        std::cout << "  \"imports\": [],\n";
        std::cout << "  \"error\": \"Lexer errors\"\n";
        std::cout << "}\n";
        return false;
    }

    // Phase 2: Parsing
    aria::Parser parser(tokens);
    auto module_node = parser.parse();

    if (!module_node || parser.hasErrors()) {
        std::cout << "{\n";
        std::cout << "  \"source\": \"" << filename << "\",\n";
        std::cout << "  \"imports\": [],\n";
        std::cout << "  \"error\": \"Parser errors\"\n";
        std::cout << "}\n";
        return false;
    }

    // Extract project root from filename
    std::string project_root = ".";
    size_t last_slash = filename.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        project_root = filename.substr(0, last_slash);
    }

    // Create module loader to discover and resolve imports
    aria::sema::ModuleLoader module_loader(project_root);

    // Get absolute path for the source file
    std::string abs_filename = filename;
    try {
        abs_filename = std::filesystem::canonical(filename).string();
    } catch (...) {
        // Keep relative path if canonical fails
    }

    // Output JSON header
    std::cout << "{\n";
    std::cout << "  \"source\": \"" << abs_filename << "\",\n";
    std::cout << "  \"imports\": [";

    // Cast to ProgramNode to access declarations
    auto* program = dynamic_cast<aria::ProgramNode*>(module_node.get());
    if (!program) {
        std::cout << "{\n";
        std::cout << "  \"source\": \"" << filename << "\",\n";
        std::cout << "  \"imports\": [],\n";
        std::cout << "  \"error\": \"Failed to parse program\"\n";
        std::cout << "}\n";
        return false;
    }

    // Scan for use statements in the AST declarations
    bool first = true;
    for (const auto& decl : program->declarations) {
        if (auto* use_stmt = dynamic_cast<aria::UseStmt*>(decl.get())) {
            // Resolve the import to a file path
            std::string resolved_path = module_loader.getResolver().resolveImport(use_stmt, filename);

            // Build module name from path components
            std::string module_name;
            for (size_t i = 0; i < use_stmt->path.size(); ++i) {
                if (i > 0) module_name += ".";
                module_name += use_stmt->path[i];
            }

            if (!first) std::cout << ",";
            first = false;

            std::cout << "\n    {";
            std::cout << "\"module\": \"" << module_name << "\", ";
            std::cout << "\"path\": \"" << resolved_path << "\"";
            std::cout << "}";
        }
    }

    std::cout << "\n  ],\n";
    std::cout << "  \"error\": null\n";
    std::cout << "}\n";

    return true;
}

/**
 * Compile source to LLVM Module
 * Note: Returns raw pointer - the IRGenerator must be kept alive!
 */
llvm::Module* compile_to_module(
    const std::string& source,
    const std::string& filename,
    const CompilerOptions& opts,
    aria::IRGenerator& ir_gen,
    aria::DiagnosticEngine& diags
) {
    // Phase 0: Preprocessing (NASM-style macros)
    std::string preprocessed_source = source;
    
    if (opts.verbose) {
        std::cout << "Phase 0: Preprocessing...\n";
    }
    
    try {
        aria::frontend::Preprocessor preprocessor;
        preprocessed_source = preprocessor.process(source, filename);
    } catch (const std::exception& e) {
        diags.error(aria::SourceLocation(filename, 0, 0), 
                   std::string("Preprocessor error: ") + e.what());
        return nullptr;
    }
    
    // If -E flag is set, output preprocessed source and exit
    if (opts.preprocess_only) {
        std::cout << preprocessed_source;
        return nullptr;
    }
    
    // Phase 1: Lexical Analysis
    if (opts.verbose) {
        std::cout << "Phase 1: Lexical analysis...\n";
    }
    
    aria::frontend::Lexer lexer(preprocessed_source);
    auto tokens = lexer.tokenize();
    
    if (!lexer.getErrors().empty()) {
        // Convert lexer errors to diagnostic engine format
        for (const auto& err : lexer.getErrors()) {
            // Parse line/column from error message format: "[Line X, Col Y] Error: message"
            int line = 0, column = 0;
            size_t line_pos = err.find("Line ");
            size_t col_pos = err.find("Col ");
            
            if (line_pos != std::string::npos && col_pos != std::string::npos) {
                try {
                    size_t line_start = line_pos + 5;  // Skip "Line "
                    size_t line_end = err.find(',', line_start);
                    line = std::stoi(err.substr(line_start, line_end - line_start));
                    
                    size_t col_start = col_pos + 4;  // Skip "Col "
                    size_t col_end = err.find(']', col_start);
                    column = std::stoi(err.substr(col_start, col_end - col_start));
                } catch (...) {
                    // If parsing fails, use 0,0
                }
            }
            
            // Extract just the error message (skip location prefix)
            std::string message = err;
            size_t error_pos = err.find("Error: ");
            if (error_pos != std::string::npos) {
                message = err.substr(error_pos + 7);  // Skip "Error: "
            }
            
            diags.error(aria::SourceLocation(filename, line, column), message);
        }
        return nullptr;
    }
    
    if (opts.dump_tokens) {
        std::cout << "Tokens:\n";
        for (const auto& token : tokens) {
            std::cout << "  " << token.toString() << "\n";
        }
        return nullptr;
    }

    // Phase 2: Parsing
    if (opts.verbose) {
        std::cout << "Phase 2: Parsing...\n";
    }
    
    aria::Parser parser(tokens);
    auto module_node = parser.parse();
    
    if (!module_node || parser.hasErrors()) {
        // Convert parser errors to diagnostic engine format
        for (const auto& err : parser.getErrors()) {
            // Parse line/column from error message format: "Parse error at line X, column Y:"
            int line = 0, column = 0;
            size_t line_pos = err.find("line ");
            size_t col_pos = err.find("column ");
            
            if (line_pos != std::string::npos && col_pos != std::string::npos) {
                try {
                    size_t line_start = line_pos + 5;  // Skip "line "
                    size_t line_end = err.find(',', line_start);
                    line = std::stoi(err.substr(line_start, line_end - line_start));
                    
                    size_t col_start = col_pos + 7;  // Skip "column "
                    size_t col_end = err.find(':', col_start);
                    column = std::stoi(err.substr(col_start, col_end - col_start));
                } catch (...) {
                    // If parsing fails, use 0,0
                }
            }
            
            // Extract just the error message (skip the location line)
            std::string message = err;
            size_t newline_pos = err.find('\n');
            if (newline_pos != std::string::npos) {
                message = err.substr(newline_pos + 1);
            }
            
            diags.error(aria::SourceLocation(filename, line, column), message);
        }
        return nullptr;
    }
    
    if (opts.dump_ast) {
        std::cout << "AST:\n";
        std::cout << module_node->toString() << "\n";
        return nullptr;
    }

    // Phase 3: Semantic Analysis
    if (opts.verbose) {
        std::cout << "Phase 3: Semantic analysis...\n";
    }
    
    // Create type system and symbol table
    aria::sema::TypeSystem type_system;
    aria::sema::SymbolTable symbol_table;
    
    // Create generic resolver and monomorphizer (Session 13: pass TypeSystem for struct specialization)
    aria::sema::GenericResolver generic_resolver;
    generic_resolver.setSymbolTable(&symbol_table);  // Task 4: Enable trait constraint checking
    aria::sema::Monomorphizer monomorphizer(&generic_resolver, &type_system);
    
    // Module system: Normalize input filename to absolute path for correct resolution
    std::string absolute_filename;
    try {
        absolute_filename = std::filesystem::canonical(filename).string();
    } catch (...) {
        // If canonical() fails (file doesn't exist yet), use absolute path
        std::filesystem::path p(filename);
        if (p.is_relative()) {
            absolute_filename = (std::filesystem::current_path() / p).lexically_normal().string();
        } else {
            absolute_filename = filename;
        }
    }
    
    // Extract project root from input file path
    std::string project_root = ".";
    size_t last_slash = absolute_filename.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        project_root = absolute_filename.substr(0, last_slash);
    }
    
    // Create module loader for handling use statements
    aria::sema::ModuleLoader module_loader(project_root);
    
    // Add compiler installation directory to search paths for stdlib
    // First try to find where this executable is located
    std::string compiler_dir = ".";
    try {
        char exe_path[4096];
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len != -1) {
            exe_path[len] = '\0';
            std::string exe_dir = std::string(exe_path);
            size_t last_slash_exe = exe_dir.find_last_of("/\\");
            if (last_slash_exe != std::string::npos) {
                compiler_dir = exe_dir.substr(0, last_slash_exe);
                // Go up one level from build/ to project root
                size_t parent_slash = compiler_dir.find_last_of("/\\");
                if (parent_slash != std::string::npos) {
                    compiler_dir = compiler_dir.substr(0, parent_slash);
                }
            }
        }
        module_loader.getResolver().addSearchPath(compiler_dir);
        // Add /lib subdirectory for stdlib
        module_loader.getResolver().addSearchPath(compiler_dir + "/lib");
    } catch (...) {
        // Fallback: current working directory
        module_loader.getResolver().addSearchPath(".");
    }
    
    // Add user-provided include paths from -I flags
    for (const auto& include_path : opts.include_paths) {
        module_loader.getResolver().addSearchPath(include_path);
    }
    
    // Type checker with generic support and module loading (use absolute path)
    aria::sema::TypeChecker type_checker(&type_system, &symbol_table, &generic_resolver, &monomorphizer, &module_loader, absolute_filename);
    
    // Run type checking on entire module (activates generic specialization)
    type_checker.check(module_node.get());
    
    if (type_checker.hasErrors()) {
        for (const auto& err : type_checker.getErrors()) {
            diags.error(aria::SourceLocation(filename, 0, 0), err);
        }
        return nullptr;
    }
    
    // Type check all loaded modules (for cross-module calls)
    const auto& loaded_modules = module_loader.getLoadedModules();
    for (const auto& [path, loaded_module] : loaded_modules) {
        if (loaded_module && loaded_module->ast) {
            if (opts.verbose) {
                std::cout << "  Type checking module: " << path << "\n";
            }
            // Create a separate type checker for each module
            // (They share the same type system but have separate symbol tables)
            aria::sema::TypeChecker module_type_checker(&type_system, loaded_module->moduleInfo->getSymbolTable(), 
                                                        &generic_resolver, &monomorphizer, &module_loader, path);
            module_type_checker.check(loaded_module->ast.get());
            
            if (module_type_checker.hasErrors()) {
                for (const auto& err : module_type_checker.getErrors()) {
                    diags.error(aria::SourceLocation(path, 0, 0), err);
                }
                return nullptr;
            }
        }
    }
    
    // Report symbol table warnings (e.g., shadowing)
    if (symbol_table.hasWarnings()) {
        for (const auto& warn : symbol_table.getWarnings()) {
            diags.warning(aria::SourceLocation(filename, 0, 0), warn);
        }
    }
    
    // Phase 3.5: Borrow Checker (Phase 5b in research)
    if (opts.verbose) {
        std::cout << "Phase 3.5: Borrow checking...\n";
    }
    
    aria::sema::BorrowChecker borrow_checker;
    auto borrow_errors = borrow_checker.analyze(module_node.get());
    
    if (!borrow_errors.empty()) {
        for (const auto& err : borrow_errors) {
            aria::SourceLocation loc(filename, err.line, err.column);
            diags.error(loc, err.message);
            
            // Print related information if available
            if (err.related_line >= 0) {
                aria::SourceLocation related_loc(filename, err.related_line, err.related_column);
                diags.note(related_loc, err.related_message);
            }
        }
        return nullptr;
    }

    // Phase 4: IR Generation
    if (opts.verbose) {
        std::cout << "Phase 4: IR generation...\n";
    }
    
    // Pass TypeSystem to IR generator for struct type lookups
    ir_gen.setTypeSystem(&type_system);
    
    // Generate IR for specialized generic functions FIRST (before main codegen)
    // This ensures specialized functions exist when regular code tries to call them
    const auto& specializations = monomorphizer.getSpecializations();
    if (!specializations.empty()) {
        if (opts.verbose) {
            std::cout << "  Generating " << specializations.size()
                     << " specialized generic function(s)...\n";
        }
        try {
            size_t generated = ir_gen.codegenSpecializedFunctions(specializations);
            if (opts.verbose) {
                std::cout << "  Generated " << generated << " specialization(s)\n";
            }
        } catch (const std::exception& e) {
            diags.error(aria::SourceLocation(filename, 0, 0),
                       std::string("Specialization IR error: ") + e.what());
            return nullptr;
        }
    }
    
    // Generate IR for loaded modules (needed for cross-module function calls)
    for (const auto& [path, loaded_module] : loaded_modules) {
        if (loaded_module && loaded_module->ast) {
            if (opts.verbose) {
                std::cout << "  Generating IR for module: " << path << "\n";
            }
            
            // Extract module name from path (e.g., "math_utils" from "tests/module_loading/math_utils.aria")
            std::string module_name = path;
            size_t last_slash = module_name.find_last_of("/\\");
            if (last_slash != std::string::npos) {
                module_name = module_name.substr(last_slash + 1);
            }
            size_t last_dot = module_name.find_last_of('.');
            if (last_dot != std::string::npos) {
                module_name = module_name.substr(0, last_dot);
            }
            
            // Set the current module name so functions get external linkage
            ir_gen.setCurrentModuleName(module_name);

            try {
                auto module_value = ir_gen.codegen(loaded_module->ast.get());
                if (!module_value) {
                    diags.error(aria::SourceLocation(path, 0, 0), "Module IR generation failed");
                    return nullptr;
                }
            } catch (const std::exception& e) {
                diags.error(aria::SourceLocation(path, 0, 0),
                           std::string("IR generation error: ") + e.what());
                return nullptr;
            }
            
            // Clear module name for main module generation
            ir_gen.setCurrentModuleName("");
        }
    }
    
    // Now generate the main module code (which can call specialized functions and imported modules)
    try {
        auto value = ir_gen.codegen(module_node.get());

        if (!value) {
            diags.error(aria::SourceLocation(filename, 0, 0), "IR generation failed");
            return nullptr;
        }
    } catch (const std::exception& e) {
        diags.error(aria::SourceLocation(filename, 0, 0),
                   std::string("IR generation error: ") + e.what());
        return nullptr;
    }

    // Return raw pointer - caller must keep IRGenerator alive
    return ir_gen.getModule();
}

/**
 * Emit LLVM IR to file
 */
bool emit_llvm_ir(llvm::Module* module, const std::string& output_file) {
    // DEBUG: Dump module to stderr before writing to file
    std::cerr << "\n=== MODULE DUMP BEFORE WRITE ===\n";
    module->print(llvm::errs(), nullptr);
    std::cerr << "=== END MODULE DUMP ===\n\n";
    
    std::error_code ec;
    llvm::raw_fd_ostream out(output_file, ec, llvm::sys::fs::OF_None);
    
    if (ec) {
        std::cerr << "Error: Could not open output file: " << output_file << "\n";
        std::cerr << "  " << ec.message() << "\n";
        return false;
    }
    
    module->print(out, nullptr);
    return true;
}

/**
 * Emit LLVM bitcode to file
 */
bool emit_llvm_bitcode(llvm::Module* module, const std::string& output_file) {
    std::error_code ec;
    llvm::raw_fd_ostream out(output_file, ec, llvm::sys::fs::OF_None);
    
    if (ec) {
        std::cerr << "Error: Could not open output file: " << output_file << "\n";
        std::cerr << "  " << ec.message() << "\n";
        return false;
    }
    
    llvm::WriteBitcodeToFile(*module, out);
    return true;
}

/**
 * ARIA-021: Check if module contains large integer types (>=128 bits)
 * LLVM has multiple bugs with large integers (128-bit+):
 * 1. IPSCCP pass crashes in ConstantRange APInt operations for 128+ bits
 * 2. DAGCombiner crashes in KnownBits APInt operations for 256+ bits
 * We detect these cases and use a safer optimization level (O0).
 *
 * Note: 128-bit integers work in SOME contexts (e.g., if statements, for loops)
 * but fail in while loops. We use O0 for all 128+ bit code to be safe.
 */
static bool hasLargeIntegerTypes(llvm::Module* module) {
    for (auto& F : *module) {
        for (auto& BB : F) {
            for (auto& I : BB) {
                // Check instruction result type (128+ bits needs O0)
                llvm::Type* ty = I.getType();
                if (ty->isIntegerTy() && ty->getIntegerBitWidth() >= 128) {
                    return true;
                }
                // Check operand types
                for (unsigned i = 0; i < I.getNumOperands(); ++i) {
                    llvm::Type* opTy = I.getOperand(i)->getType();
                    if (opTy->isIntegerTy() && opTy->getIntegerBitWidth() >= 128) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/**
 * Emit assembly to file
 */
bool emit_assembly(llvm::Module* module, const std::string& output_file) {
    // Initialize native target only
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();

    std::string target_triple = llvm::sys::getDefaultTargetTriple();
    module->setTargetTriple(target_triple);

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(target_triple, error);

    if (!target) {
        std::cerr << "Error: " << error << "\n";
        return false;
    }

    // ARIA-021: Check for large integers (>128 bits) BEFORE creating target machine
    // LLVM has multiple bugs with large integers (256-bit+):
    // 1. IPSCCP pass crashes in ConstantRange APInt operations
    // 2. DAGCombiner crashes in KnownBits APInt operations
    // We use O0 for both IR optimization and codegen to avoid these bugs.
    bool useLargeIntMode = hasLargeIntegerTypes(module);
    if (useLargeIntMode) {
        std::cerr << "[WARN] Large integer types (128+ bit) detected. "
                  << "Using unoptimized codegen to avoid LLVM bugs.\n";
    }

    auto cpu = "generic";
    auto features = "";
    llvm::TargetOptions opt;

    // ARIA-021: For large integers, try GlobalISel to avoid SelectionDAG bugs
    // The SelectionDAG's DAGCombiner has bugs with 128+ bit integers.
    // GlobalISel uses a different code path that may avoid these issues.
    if (useLargeIntMode) {
        opt.EnableGlobalISel = true;
        opt.GlobalISelAbort = llvm::GlobalISelAbortMode::Disable;  // Fall back to DAG if needed
    }

    // Create target machine with appropriate codegen optimization level
    llvm::CodeGenOptLevel codegenOpt = useLargeIntMode ?
        llvm::CodeGenOptLevel::None : llvm::CodeGenOptLevel::Default;

    auto target_machine = target->createTargetMachine(
        target_triple, cpu, features, opt, llvm::Reloc::PIC_,
        std::nullopt,  // CodeModel
        codegenOpt     // CodeGenOptLevel
    );

    module->setDataLayout(target_machine->createDataLayout());

    // =========================================================================
    // COROUTINE LOWERING (Required for async/await support)
    // =========================================================================
    // LLVM coroutines must be lowered before code generation.
    // This uses the new pass manager to run coroutine passes.

    // ARIA-021: For large integers (128+ bits), skip optimization entirely
    // The LLVM optimization passes (IPSCCP, SCCP, DAGCombiner) have bugs with
    // large integer arithmetic that cause crashes. We only skip optimization
    // when large integers are detected; otherwise run normal O1 pipeline.
    if (!useLargeIntMode) {
        // Create analysis managers
        llvm::LoopAnalysisManager LAM;
        llvm::FunctionAnalysisManager FAM;
        llvm::CGSCCAnalysisManager CGAM;
        llvm::ModuleAnalysisManager MAM;

        // Create pass builder and register analyses
        llvm::PassBuilder PB(target_machine);
        PB.registerModuleAnalyses(MAM);
        PB.registerCGSCCAnalyses(CGAM);
        PB.registerFunctionAnalyses(FAM);
        PB.registerLoopAnalyses(LAM);
        PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

        // Use the default O1 optimization pipeline which includes proper coroutine handling
        // The coroutine passes need to be interleaved with optimization passes to work correctly
        llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O1);

        // Run optimization pipeline (includes coroutine lowering)
        MPM.run(*module, MAM);
    }
    // For large integers: no optimization passes at all, direct codegen

    std::error_code ec;
    llvm::raw_fd_ostream out(output_file, ec, llvm::sys::fs::OF_None);

    if (ec) {
        std::cerr << "Error: Could not open file: " << output_file << "\n";
        return false;
    }

    llvm::legacy::PassManager pass;
    auto file_type = llvm::CodeGenFileType::AssemblyFile;

    if (target_machine->addPassesToEmitFile(pass, out, nullptr, file_type)) {
        std::cerr << "Error: Target machine can't emit a file of this type\n";
        return false;
    }


    pass.run(*module);
    out.flush();

    return true;
}

/**
 * Link object file to executable
 */
bool link_executable(const std::string& object_file, const std::string& output_file, const CompilerOptions& opts) {
    // Try to find the runtime library in several locations
    std::vector<std::string> search_paths = {
        "build/libaria_runtime.a",           // Build directory (development)
        "../build/libaria_runtime.a",        // Relative to executable
        "libaria_runtime.a",                 // Current directory
        "/usr/local/lib/libaria_runtime.a",  // Installation directory
        "/usr/lib/libaria_runtime.a"         // System installation
    };
    
    std::string runtime_lib;
    for (const auto& path : search_paths) {
        if (std::system(("test -f " + path + " 2>/dev/null").c_str()) == 0) {
            runtime_lib = path;
            break;
        }
    }
    
    if (runtime_lib.empty()) {
        std::cerr << "Warning: Could not find libaria_runtime.a, linking without runtime library\n";
        std::cerr << "  Searched paths:\n";
        for (const auto& path : search_paths) {
            std::cerr << "    " << path << "\n";
        }
    }
    
    // Build linker command
    std::ostringstream cmd;
    cmd << "clang++";
    
    // Add object file
    cmd << " " << object_file;
    
    // Add runtime library if found
    if (!runtime_lib.empty()) {
        cmd << " " << runtime_lib;
    }
    
    // Add library search paths (-L)
    for (const auto& lib_path : opts.library_paths) {
        cmd << " -L" << lib_path;
    }
    
    // Add libraries to link (-l)
    for (const auto& lib : opts.link_libraries) {
        cmd << " -l" << lib;
    }
    
    // Add linker flags (-Wl,...)
    for (const auto& flag : opts.linker_flags) {
        cmd << " -Wl," << flag;
    }
    
    // Add output file
    cmd << " -o " << output_file;
    
    if (opts.verbose) {
        std::cout << "[LINK] " << cmd.str() << "\n";
    }
    
    int result = std::system(cmd.str().c_str());
    return result == 0;
}

/**
 * Main entry point
 */
int main(int argc, char** argv) {
    // Parse command-line arguments
    CompilerOptions opts;
    if (!parse_arguments(argc, argv, opts)) {
        return 1;
    }

    if (opts.verbose) {
        std::cout << "Aria Compiler " << ARIA_VERSION << "\n";
        std::cout << "Input files (" << opts.input_files.size() << "):" << "\n";
        for (const auto& file : opts.input_files) {
            std::cout << "  " << file << "\n";
        }
        std::cout << "Output: " << opts.output_file << "\n";
    }

    // Create diagnostic engine
    aria::DiagnosticEngine diags;

    // For --dump-tokens or --dump-ast, only process first file
    if (opts.dump_tokens || opts.dump_ast) {
        std::string source;
        if (!read_source_file(opts.input_files[0], source, diags)) {
            diags.printAll();
            return 1;
        }
        aria::IRGenerator ir_gen(opts.input_files[0]);
        llvm::Module* module = compile_to_module(source, opts.input_files[0], opts, ir_gen, diags);
        // compile_to_module returns nullptr for early exits (dump modes)
        if (diags.hasErrors()) {
            diags.printAll();
            return 1;
        }
        return module ? 1 : 0;
    }

    // ARIA-011: For --emit-deps, extract and output dependencies as JSON
    // Used by aria_make for accurate dependency scanning using the proper parser
    if (opts.emit_deps) {
        // Process first input file only (aria_make calls per-file)
        const auto& input_file = opts.input_files[0];

        std::string source;
        if (!read_source_file(input_file, source, diags)) {
            // Output error JSON
            std::cout << "{\"source\": \"" << input_file << "\", \"imports\": [], \"error\": \"Could not read file\"}\n";
            return 1;
        }

        bool success = emit_dependencies(source, input_file, opts, diags);
        return success ? 0 : 1;
    }

    // Compile each input file into a separate module
    std::vector<std::unique_ptr<aria::IRGenerator>> ir_generators;
    std::vector<llvm::Module*> modules;
    
    for (size_t i = 0; i < opts.input_files.size(); i++) {
        const auto& input_file = opts.input_files[i];
        
        if (opts.verbose) {
            std::cout << "\nCompiling [" << (i+1) << "/" << opts.input_files.size() << "]: " << input_file << "\n";
        }
        
        // Read source file
        std::string source;
        if (!read_source_file(input_file, source, diags)) {
            diags.printAll();
            return 1;
        }
        
        // Create IR generator (must stay alive)
        ir_generators.push_back(std::make_unique<aria::IRGenerator>(input_file));
        
        // Compile to LLVM module
        llvm::Module* module = compile_to_module(source, input_file, opts, *ir_generators.back(), diags);
        if (!module) {
            diags.printAll();
            return 1;
        }
        

        
        modules.push_back(module);
    }
    
    // Print any warnings that were collected during compilation
    if (diags.hasWarnings()) {
        diags.printAll();
    }
    
    // Link all modules together if we have more than one
    llvm::Module* final_module = modules[0];
    if (modules.size() > 1) {
        if (opts.verbose) {
            std::cout << "\nLinking " << modules.size() << " modules...\n";
        }
        
        // Create a linker for the first module
        llvm::Linker linker(*modules[0]);
        
        // Link in all other modules
        for (size_t i = 1; i < modules.size(); i++) {
            // Clone module before linking (linker takes ownership)
            std::unique_ptr<llvm::Module> module_copy = llvm::CloneModule(*modules[i]);
            if (linker.linkInModule(std::move(module_copy))) {
                std::cerr << "Error: Failed to link module " << opts.input_files[i] << "\n";
                return 1;
            }
        }
        
        if (opts.verbose) {
            std::cout << "Linking complete\n";
        }
    }

    // Verify module - TODO: Fix verification crash in Phase 7.1.1
    // std::string verify_error;
    // llvm::raw_string_ostream verify_stream(verify_error);
    // if (llvm::verifyModule(*module, &verify_stream)) {
    //     std::cerr << "Error: Module verification failed:\n";
    //     std::cerr << verify_error << "\n";
    //     return 1;
    // }

    // Emit output based on options
    if (opts.emit_llvm_ir) {
        if (!emit_llvm_ir(final_module, opts.output_file)) {
            return 1;
        }
        if (opts.verbose) {
            std::cout << "LLVM IR written to: " << opts.output_file << "\n";
        }
    } else if (opts.emit_llvm_bc) {
        if (!emit_llvm_bitcode(final_module, opts.output_file)) {
            return 1;
        }
        if (opts.verbose) {
            std::cout << "LLVM bitcode written to: " << opts.output_file << "\n";
        }
    } else if (opts.emit_asm) {
        if (!emit_assembly(final_module, opts.output_file)) {
            return 1;
        }
        if (opts.verbose) {
            std::cout << "Assembly written to: " << opts.output_file << "\n";
        }
    } else {
        // Generate executable
        std::string asm_file = opts.output_file + ".s";
        if (!emit_assembly(final_module, asm_file)) {
            return 1;
        }
        
        if (!link_executable(asm_file, opts.output_file, opts)) {
            diags.printAll();
            return 1;
        }
        
        // Clean up temporary assembly file
        std::remove(asm_file.c_str());
        
        if (opts.verbose) {
            std::cout << "Executable written to: " << opts.output_file << "\n";
        }
    }

    if (opts.verbose) {
        std::cout << "Compilation successful!\n";
    }

    return 0;
}
