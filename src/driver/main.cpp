/**
 * src/driver/main.cpp
 *
 * Aria Compiler Driver (ariac)
 * Version: 0.0.6
 *
 * Entry point for the Aria Compiler. Orchestrates the compilation pipeline:
 * 1. Command Line Parsing (LLVM cl)
 * 2. Source File Reading
 * 3. Preprocessing (Macros, Includes, Conditionals)
 * 4. Lexical Analysis (AriaLexer)
 * 5. Syntactic Analysis (Parser)
 * 6. Semantic Analysis (Borrow Checker, Escape Analysis, Type Checker)
 * 7. Code Generation (LLVM IR / Object Emission)
 *
 * ERROR HANDLING STRATEGY:
 * - I/O errors (file read): exit(1) immediately (unrecoverable)
 * - Parse errors: throw exceptions, caught in main with return 1
 * - Semantic errors: return bool, main checks and returns 1
 * - All errors print to stderr before terminating
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
#include "../frontend/preprocessor.h"
#include "../frontend/lexer.h"
#include "../frontend/parser.h"
#include "../frontend/sema/borrow_checker.h"
#include "../frontend/sema/escape_analysis.h"
#include "../frontend/sema/type_checker.h"
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

static cl::list<std::string> IncludePaths(
    "I",
    cl::desc("Add directory to include search path"),
    cl::value_desc("directory"),
    cl::cat(AriaCategory)
);

static cl::list<std::string> Defines(
    "D",
    cl::desc("Define preprocessor constant (e.g., -DDEBUG=1)"),
    cl::value_desc("name=value"),
    cl::cat(AriaCategory)
);

static cl::opt<bool> PreprocessOnly(
    "E",
    cl::desc("Run preprocessor only, output to stdout"),
    cl::cat(AriaCategory)
);

static cl::opt<bool> NoVerify(
    "no-verify",
    cl::desc("Skip LLVM IR verification (not recommended)"),
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
    
    // Initialize native target (host architecture) for AVX-512 instructions
    // This is sufficient for compiling to the host platform
    InitializeNativeTarget();
    InitializeNativeTargetAsmParser();
    InitializeNativeTargetAsmPrinter();

    // 2. Parse Command Line Arguments
    cl::HideUnrelatedOptions(AriaCategory);
    cl::ParseCommandLineOptions(argc, argv, "Aria Systems Compiler\n");

    if (Verbose) {
        outs() << "Compiling " << InputFilename << "...\n";
    }

    // 3. Read Source Code
    std::string sourceCode = readFile(InputFilename);

    // 4. Frontend: Preprocessing
    // Handle macros, includes, conditional compilation, and repetition
    // The preprocessor expands all directives and produces clean source
    if (Verbose) outs() << "[Phase 1] Preprocessing...\n";
    
    aria::frontend::Preprocessor preprocessor;
    
    // Add include paths from command line
    for (const auto& path : IncludePaths) {
        preprocessor.addIncludePath(path);
        if (Verbose) outs() << "  Include path: " << path << "\n";
    }
    
    // Add default include paths
    preprocessor.addIncludePath(".");  // Current directory
    preprocessor.addIncludePath("./include");  // Local include directory
    
    // Define constants from command line (-D flags)
    for (const auto& define : Defines) {
        size_t eq_pos = define.find('=');
        if (eq_pos != std::string::npos) {
            std::string name = define.substr(0, eq_pos);
            std::string value = define.substr(eq_pos + 1);
            preprocessor.defineConstant(name, value);
            if (Verbose) outs() << "  Defined: " << name << " = " << value << "\n";
        } else {
            // No value, define as 1 (like gcc -DFLAG)
            preprocessor.defineConstant(define, "1");
            if (Verbose) outs() << "  Defined: " << define << " = 1\n";
        }
    }
    
    std::string preprocessedCode;
    try {
        preprocessedCode = preprocessor.process(sourceCode, InputFilename);
    } catch (const std::exception& e) {
        errs() << "Preprocessor Error: " << e.what() << "\n";
        return 1;
    }
    
    if (Verbose) {
        outs() << "Preprocessing complete. Source size: " 
               << sourceCode.length() << " -> " 
               << preprocessedCode.length() << " bytes\n";
    }
    
    // If -E flag is set, just output preprocessed code and exit
    if (PreprocessOnly) {
        outs() << preprocessedCode;
        return 0;
    }

    // 5. Frontend: Lexical Analysis
    // The Lexer handles sanitization (e.g., banning @tesla symbols)
    if (Verbose) outs() << "[Phase 2] Lexing...\n";
    std::unique_ptr<aria::frontend::AriaLexer> lexer = std::make_unique<aria::frontend::AriaLexer>(preprocessedCode);

    // 6. Frontend: Parsing
    // Builds the AST (Abstract Syntax Tree)
    // Context holds compilation settings like strict mode
    if (Verbose) outs() << "[Phase 3] Parsing...\n";
    aria::frontend::ParserContext parserCtx;
    parserCtx.strictMode = StrictMode;

    // TODO: Strict mode should enable stricter checks in parser:
    // - Require explicit type annotations (no type inference)
    // - Disallow implicit conversions
    // - Enforce stricter borrow checking rules
    // - Warn on unused variables/imports
    // - Require explicit wild pointer free() calls
    // The parser will check this flag when implementing these features.
    
    // Instantiate Parser with the lexer
    aria::frontend::Parser parser(*lexer, parserCtx);
    
    // Parse top-level program (file contains function declarations, etc.)
    // In the v0.0.6 spec, files contain top-level declarations without { } wrapper.
    // The parseProgram() function handles this.
    std::unique_ptr<aria::frontend::Block> astRoot;
    try {
        astRoot = parser.parseProgram(); 
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
    if (Verbose) outs() << "[Phase 4] Semantic Analysis (Borrow Check)...\n";
    bool safe = aria::sema::check_borrow_rules(astRoot.get());

    if (!safe) {
        errs() << "Compilation Failed: Memory Safety Violations Detected.\n";
        // In Aria, safety violations are fatal errors, not warnings.
        return 1;
    }

    // 6.5 Semantic Analysis: Escape Analysis
    // Prevents stack pointers from escaping function scope (dangling references)
    // See src/frontend/sema/escape_analysis.cpp
    if (Verbose) outs() << "[Phase 4b] Escape Analysis...\n";
    aria::sema::EscapeAnalysisResult escapeResult = aria::sema::run_escape_analysis(astRoot.get());

    if (escapeResult.has_escapes) {
        errs() << "Compilation Failed: Escape Analysis Violations Detected.\n";
        errs() << "Wild pointers cannot escape their scope - this would create dangling references.\n";
        errs() << "Found " << escapeResult.escaped_count << " escaped pointer(s).\n";
        return 1;
    }

    // 6.6 Semantic Analysis: Type Checking
    // Verifies that all operations use compatible types
    // See src/frontend/sema/type_checker.cpp
    if (Verbose) outs() << "[Phase 4c] Type Checking...\n";
    aria::sema::TypeCheckResult typeResult = aria::sema::checkTypes(astRoot.get());
    
    if (!typeResult.success) {
        errs() << "Compilation Failed: Type Errors Detected.\n";
        for (const auto& error : typeResult.errors) {
            errs() << "  Error: " << error << "\n";
        }
        return 1;
    }

    // 7. Backend: Code Generation
    // Lowers AST to LLVM IR, handling exotic types (int512, trit)
    // See src/backend/codegen.cpp
    if (Verbose) outs() << "[Phase 5] Generating Code...\n";

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
    bool verifyIR = !NoVerify;  // Verify by default, skip if --no-verify
    bool codegenSuccess = aria::backend::generate_code(astRoot.get(), outPath, verifyIR);
    
    if (!codegenSuccess) {
        errs() << "\nCode generation failed due to IR verification errors.\n";
        errs() << "Use --no-verify to generate IR anyway (not recommended).\n";
        return 1;
    }

    if (Verbose && !EmitLLVM) {
        outs() << "Note: Object emission requires linking phase.\n";
        outs() << "Run 'clang " << outPath << " -o a.out' to link.\n";
    }

    if (Verbose) outs() << "Build Complete.\n";
    return 0;
}
