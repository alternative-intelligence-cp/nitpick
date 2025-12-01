/**
 * src/driver/main.cpp
 * 
 * Aria Compiler Driver (ariac)
 * Version: 0.0.6
 * 
 * Entry point for the Aria Compiler. Orchestrates the compilation pipeline:
 * 1. Command Line Parsing (LLVM cl)
 * 2. Source File Reading
 * 3. Lexical Analysis (AriaLexer)
 * 4. Syntactic Analysis (Parser)
 * 5. Semantic Analysis (Borrow Checker)
 * 6. Code Generation (LLVM IR / Object Emission)
 * 
 * Dependencies: LLVM 18, Aria Frontend, Aria Backend
 */

#include <iostream>
#include <string>
#include <vector>
#include <memory>

// LLVM Includes
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Error.h>
#include <llvm/Target/TargetMachine.h>

// Aria Includes
// Note: These headers are implied by the snippets provided in the research material
#include "../frontend/lexer.h"
#include "../frontend/parser.h"
#include "../frontend/sema/borrow_checker.h"
#include "../frontend/sema/escape_analysis.h"
#include "../backend/codegen.h"

using namespace llvm;

// CLI Option Definitions
// We use LLVM's CommandLine library for robust argument handling
static cl::OptionCategory AriaCategory("Aria Compiler Options");

static cl::opt<std::string> InputFilename(
    cl::Positional, 
    cl::desc("<input file>"), 
    cl::Required, 
    cl::cat(AriaCategory)
);

static cl::opt<std::string> OutputFilename(
    "o", 
    cl::desc("Specify output filename"), 
    cl::value_desc("filename"), 
    cl::cat(AriaCategory)
);

static cl::opt<bool> EmitLLVM(
    "emit-llvm", 
    cl::desc("Emit LLVM IR instead of object code"), 
    cl::cat(AriaCategory)
);

static cl::opt<bool> Verbose(
    "v", 
    cl::desc("Enable verbose output"), 
    cl::cat(AriaCategory)
);

static cl::opt<bool> StrictMode(
    "strict", 
    cl::desc("Enable strict mode (stricter borrow checking)"), 
    cl::init(true),
    cl::cat(AriaCategory)
);

// Helper to extract source content
std::string readFile(const std::string& path) {
    auto result = MemoryBuffer::getFile(path);
    if (auto error = result.getError()) {
        errs() << "Error reading file '" << path << "': " << error.message() << "\n";
        exit(1);
    }
    return result.get()->getBuffer().str();
}

int main(int argc, char** argv) {
    // 1. Initialize LLVM Infrastructure
    // Required for TargetMachine lookup and AVX-512 backend support 
    InitLLVM X(argc, argv);
    
    // Initialize all targets to ensure we can target the host architecture
    // specifically for the AVX-512 instructions required by the spec.
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

    // 2. Parse Command Line Arguments
    cl::HideUnrelatedOptions(AriaCategory);
    cl::ParseCommandLineOptions(argc, argv, "Aria Systems Compiler\n");

    if (Verbose) {
        outs() << "Compiling " << InputFilename << "...\n";
    }

    // 3. Read Source Code
    std::string sourceCode = readFile(InputFilename);

    // 4. Frontend: Lexical Analysis
    // The Lexer handles sanitization (e.g., banning @tesla symbols )
    if (Verbose) outs() << "[Phase 1] Lexing...\n";
    std::unique_ptr<aria::AriaLexer> lexer = std::make_unique<aria::AriaLexer>(sourceCode);

    // 5. Frontend: Parsing
    // Builds the AST (Abstract Syntax Tree)
    // Context holds compilation settings like strict mode
    if (Verbose) outs() << "[Phase 2] Parsing...\n";
    aria::ParserContext parserCtx;
    parserCtx.strictMode = StrictMode;
    
    // Instantiate Parser with the lexer
    aria::Parser parser(std::move(lexer), &parserCtx);
    
    // We expect the top level to be a block of statements (module level)
    // In the v0.0.6 spec, the file is treated as an implicit main block.
    // The parseBlock() function is defined in.
    std::unique_ptr<aria::Block> astRoot;
    try {
        astRoot = parser.parseBlock(); 
    } catch (const std::exception& e) {
        errs() << "Parse Error: " << e.what() << "\n";
        return 1;
    }

    if (!astRoot) {
        errs() << "Error: Failed to generate AST.\n";
        return 1;
    }

    // 6. Semantic Analysis: Borrow Checker
    // Enforces the "Appendage Theory" rules (pinning, wild pointers)
    // See src/frontend/sema/borrow_checker.cpp
    if (Verbose) outs() << "[Phase 3] Semantic Analysis (Borrow Check)...\n";
    bool safe = aria::sema::check_borrow_rules(astRoot.get());

    if (!safe) {
        errs() << "Compilation Failed: Memory Safety Violations Detected.\n";
        // In Aria, safety violations are fatal errors, not warnings.
        return 1;
    }

    // 6.5 Semantic Analysis: Escape Analysis
    // Prevents stack pointers from escaping function scope (dangling references)
    // See src/frontend/sema/escape_analysis.cpp
    if (Verbose) outs() << "[Phase 3b] Escape Analysis...\n";
    bool escapesSafe = aria::sema::run_escape_analysis(astRoot.get());

    if (!escapesSafe) {
        errs() << "Compilation Failed: Escape Analysis Violations Detected.\n";
        errs() << "Wild pointers cannot escape their scope - this would create dangling references.\n";
        return 1;
    }

    // 7. Backend: Code Generation
    // Lowers AST to LLVM IR, handling exotic types (int512, trit)
    // See src/backend/codegen.cpp
    if (Verbose) outs() << "[Phase 4] Generating Code...\n";

    // Determine output path with clear priority
    std::string outPath;
    if (!OutputFilename.empty()) {
        // User specified output file
        outPath = OutputFilename;
    } else if (EmitLLVM) {
        // Default for --emit-llvm flag
        outPath = "output.ll";
    } else {
        // Default for object/executable output
        outPath = "a.out";
    }

    if (Verbose) outs() << "Output file: " << outPath << "\n";

    // Generate code
    aria::backend::generate_code(astRoot.get(), outPath);

    if (Verbose && !EmitLLVM) {
        outs() << "Note: Object emission requires linking phase.\n";
        outs() << "Run 'clang " << outPath << " -o a.out' to link.\n";
    }

    if (Verbose) outs() << "Build Complete.\n";
    return 0;
}
