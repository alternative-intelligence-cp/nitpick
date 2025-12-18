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
#include <vector>
#include <string>
#include <memory>
#include <cstdlib>

// Aria compiler components
#include "frontend/lexer/lexer.h"
#include "frontend/parser/parser.h"
#include "frontend/sema/type_checker.h"
#include "frontend/sema/borrow_checker.h"
#include "frontend/diagnostics.h"
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
    bool dump_ast = false;
    bool dump_tokens = false;
    bool verbose = false;
    int opt_level = 0;  // -O0, -O1, -O2, -O3
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
    std::cout << "  --emit-llvm       Emit LLVM IR text (.ll)\n";
    std::cout << "  --emit-llvm-bc    Emit LLVM bitcode (.bc)\n";
    std::cout << "  --emit-asm        Emit assembly (.s)\n";
    std::cout << "  --ast-dump        Dump AST and exit\n";
    std::cout << "  --tokens          Dump tokens and exit\n";
    std::cout << "  -O<level>         Optimization level (0-3)\n";
    std::cout << "  -v, --verbose     Verbose output\n";
    std::cout << "  --version         Show version\n";
    std::cout << "  --help            Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  ariac hello.aria -o hello\n";
    std::cout << "  ariac main.aria utils.aria -o program\n";
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
        } else if (arg == "--emit-llvm") {
            opts.emit_llvm_ir = true;
        } else if (arg == "--emit-llvm-bc") {
            opts.emit_llvm_bc = true;
        } else if (arg == "--emit-asm") {
            opts.emit_asm = true;
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
    // Phase 1: Lexical Analysis
    if (opts.verbose) {
        std::cout << "Phase 1: Lexical analysis...\n";
    }
    
    aria::frontend::Lexer lexer(source);
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
    
    // TODO: Type checker integration
    // aria::TypeChecker type_checker;
    // type_checker.check(module_node.get());
    
    // TODO: Borrow checker integration  
    // aria::BorrowChecker borrow_checker;
    // borrow_checker.check(module_node.get());

    // Phase 4: IR Generation
    if (opts.verbose) {
        std::cout << "Phase 4: IR generation...\n";
    }
    
    auto value = ir_gen.codegen(module_node.get());
    
    if (!value) {
        diags.error(aria::SourceLocation(filename, 0, 0), "IR generation failed");
        return nullptr;
    }

    // Return raw pointer - caller must keep IRGenerator alive
    return ir_gen.getModule();
}

/**
 * Emit LLVM IR to file
 */
bool emit_llvm_ir(llvm::Module* module, const std::string& output_file) {
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

    auto cpu = "generic";
    auto features = "";
    llvm::TargetOptions opt;
    auto target_machine = target->createTargetMachine(
        target_triple, cpu, features, opt, llvm::Reloc::PIC_
    );

    module->setDataLayout(target_machine->createDataLayout());

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
 * Link object file to executable (placeholder)
 */
bool link_executable(const std::string& object_file, const std::string& output_file) {
    // For now, use system linker
    std::string cmd = "clang " + object_file + " -o " + output_file;
    int result = std::system(cmd.c_str());
    
    if (result != 0) {
        std::cerr << "Error: Linking failed\n";
        return false;
    }
    
    return true;
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
        
        if (!link_executable(asm_file, opts.output_file)) {
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
