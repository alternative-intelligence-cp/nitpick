/**
 * Nitpick Compiler Driver
 * 
 * Main entry point for the Nitpick compiler (npkc).
 * Handles command-line arguments, orchestrates compilation pipeline,
 * and produces executables or intermediate outputs.
 * 
 * Usage:
 *   npkc input.npk -o output           # Compile to executable
 *   npkc input.npk --emit-llvm         # Emit LLVM IR (.ll)
 *   npkc input.npk --emit-llvm-bc      # Emit LLVM bitcode (.bc)
 *   npkc input.npk --ast-dump          # Dump AST
 *   npkc input.npk --tokens            # Dump tokens
 *   npkc --version                      # Show version
 *   npkc --help                         # Show help
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <memory>
#include <set>
#include <map>
#include <cstdlib>
#include <chrono>   // For verification timing (v0.14.3)
#include <cerrno>   // For errno
#include <cstring>  // For strerror
#include <climits>  // For PATH_MAX
#include <unistd.h>  // For readlink, access
#include <sys/types.h>
#include <sys/wait.h>  // For waitpid, WIFEXITED, WEXITSTATUS
#include <filesystem> // For absolute path resolution

// Nitpick compiler components
#include "frontend/preprocessor/preprocessor.h"  // NEW: Phase 0
#include "frontend/lexer/lexer.h"
#include "frontend/parser/parser.h"
#include "frontend/sema/type_checker.h"
#include "frontend/sema/generic_resolver.h"
#include "frontend/sema/borrow_checker.h"
#include "frontend/sema/module_loader.h"  // Module system
#include "frontend/ast/type.h"            // ArrayType AST node (for bounds check elimination)
#include "frontend/diagnostics.h"
#include "frontend/warnings.h"
#include "backend/ir/ir_generator.h"
#ifdef ARIA_HAS_Z3
#include "analysis/z3_verifier.h"
#include "analysis/range_analysis.h"
#endif

// LLVM
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Bitcode/BitcodeReader.h"  // v0.8.2: Incremental compilation cache
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/CodeGen.h"  // CodeGenOptLevel for large integer support

// GPU/PTX Backend Support (Phase 1: NVPTX Target)
extern "C" {
    void LLVMInitializeNVPTXTarget();
    void LLVMInitializeNVPTXTargetInfo();
    void LLVMInitializeNVPTXTargetMC();
    void LLVMInitializeNVPTXAsmPrinter();
}

// WASM Backend Support (Phase 2: WebAssembly Target)
extern "C" {
    void LLVMInitializeWebAssemblyTarget();
    void LLVMInitializeWebAssemblyTargetInfo();
    void LLVMInitializeWebAssemblyTargetMC();
    void LLVMInitializeWebAssemblyAsmPrinter();
    void LLVMInitializeWebAssemblyAsmParser();
}

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
#define NPK_VERSION_MAJOR 0
#define NPK_VERSION_MINOR 8
#define NPK_VERSION_PATCH 2
#define NPK_VERSION "0.8.4"

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
    std::vector<std::string> link_args_ordered;  // All -L/-l/-Wl, in user's order
    bool build_library = false;               // -c: Compile library (no failsafe required)
    bool build_shared = false;                // --shared: Compile to shared library (.so)
    bool build_static = false;                // --static: Link as static executable
    bool debug_info = false;                  // -g: Emit DWARF debug info
    
    // GPU/PTX Backend Options (NVIDIA CUDA)
    bool emit_ptx = false;            // --emit-ptx: Generate PTX assembly
    std::string target_arch;          // --target=gpu|cpu|gpu+cpu
    std::string gpu_arch = "sm_50";   // --gpu-arch=sm_XX: CUDA compute capability
    int gpu_opt_level = 3;            // --gpu-opt=<0-3>: GPU optimization (default O3)
    bool enable_gpu_debug = false;    // --gpu-debug: Embed debug info in PTX
    
    // WASM Backend Options (WebAssembly)
    bool emit_wasm = false;           // --emit-wasm: Generate WebAssembly
    bool wasm_standalone = false;     // --emit-wasm without linking (just .o)
    std::string wasm_target = "wasm32-wasi";  // WASM target triple
    
    // Z3 SMT Verification Options (v0.2.45+)
    bool verify = false;              // --verify: Enable Z3 static verification pass
    bool verify_report = false;       // --verify-report: Emit detailed proof results
    bool verify_contracts = false;    // --verify-contracts: Verify requires/ensures contracts
    bool verify_overflow = false;     // --verify-overflow: Verify integer arithmetic overflow
    bool verify_concurrency = false;  // --verify-concurrency: Verify data race & deadlock freedom
    bool verify_memory = false;       // --verify-memory: Verify use-after-free & recursion bounds
    bool smt_opt = false;             // --smt-opt: Enable SMT-guided optimizations (ustack fast path)
    bool prove_report = false;        // --prove-report: Emit report of prove/assert_static outcomes
    bool borrow_debug = false;        // --borrow-debug: Emit borrow checker debug diagnostics to stderr
    bool borrow_dump = false;         // --borrow-dump: Dump borrow state visualization after analysis
    bool wild_stats = false;          // --wild-stats: Print wild memory stats at program exit
    bool guard_pages = false;         // --guard-pages: Enable guard pages around wild allocations
    bool wildx_audit = false;         // --wildx-audit: Log WildX alloc/seal/exec/free events
    bool wildx_guard_pages = false;   // --wildx-guard-pages: Guard pages around exec regions
    bool gc_stats = false;            // --gc-stats: Print GC statistics at program exit
    size_t gc_nursery_size = 0;       // --gc-nursery-size=N: Nursery size in bytes (0=default 4MB)
    size_t gc_threshold = 0;          // --gc-threshold=N: Major GC trigger threshold (0=default 64MB)
    size_t gc_max_heap = 0;           // --gc-max-heap=N: Maximum heap size in bytes (0=unlimited)
    int smt_timeout = 5000;           // --smt-timeout=N: Per-query Z3 solver timeout in ms (default: 5000)
    int verify_level = -1;            // --verify-level=N: 0=none, 1=fast, 2=standard, 3=thorough (-1=unset)
    
    // v0.8.2: Incremental compilation
    bool incremental = false;         // --incremental: Cache per-module IR, only recompile changed files
    std::string cache_dir = ".npk-cache";  // --cache-dir=<path>: IR cache directory

    // v0.23.7: Macro expansion debug
    bool expand_macros = false;       // --expand-macros: Dump post-expansion macro trace to stdout and exit
};

/**
 * Initialize GPU targets (NVPTX for NVIDIA CUDA)
 * Phase 1: NVPTX Target Initialization
 */
void initialize_gpu_targets() {
    // Initialize NVPTX backend for CUDA/PTX generation
    LLVMInitializeNVPTXTarget();
    LLVMInitializeNVPTXTargetInfo();
    LLVMInitializeNVPTXTargetMC();
    LLVMInitializeNVPTXAsmPrinter();
}

/**
 * Initialize WASM targets (WebAssembly)
 * Phase 2: WebAssembly Target Initialization
 */
void initialize_wasm_targets() {
    LLVMInitializeWebAssemblyTarget();
    LLVMInitializeWebAssemblyTargetInfo();
    LLVMInitializeWebAssemblyTargetMC();
    LLVMInitializeWebAssemblyAsmPrinter();
    LLVMInitializeWebAssemblyAsmParser();
}

/**
 * Print version information
 */
void print_version() {
    std::cout << "Nitpick Compiler (npkc) version " << NPK_VERSION << "\n";
    std::cout << "Built with LLVM " << LLVM_VERSION_STRING << "\n";
    std::cout << "(ariac was a compatibility alias — use npkc)\n";
}

/**
 * Print usage information
 */
void print_help() {
    std::cout << "Nitpick Compiler (npkc) - Compile Nitpick source files\n\n";
    std::cout << "Usage:\n";
    std::cout << "  npkc <input.npk> [<input2.npk> ...] [options]\n\n";
    std::cout << "CPU Target Options:\n";
    std::cout << "  -o <file>         Write output to <file>\n";
    std::cout << "  -I <dir>          Add directory to module search path\n";
    std::cout << "  -E                Preprocess only (output to stdout)\n";
    std::cout << "  --emit-llvm       Emit LLVM IR text (.ll)\n";
    std::cout << "  --emit-llvm-bc    Emit LLVM bitcode (.bc)\n";
    std::cout << "  --emit-asm        Emit assembly (.s)\n";
    std::cout << "  --emit-deps       Emit JSON dependency manifest (for npkbld)\n";
    std::cout << "  --ast-dump        Dump AST and exit\n";
    std::cout << "  --tokens          Dump tokens and exit\n";
    std::cout << "  --expand-macros   Dump post-expansion macro trace to stdout and exit\n";
    std::cout << "  -c                Compile library (no failsafe required)\n";
    std::cout << "  --shared          Compile to shared library (.so)\n";
    std::cout << "  --static          Link as fully static executable\n";
    std::cout << "  -g                Emit DWARF debug info (for npk-dap)\n";
    std::cout << "  -O<level>         Optimization level (0-3)\n";
    std::cout << "  -v, --verbose     Verbose output\n\n";
    std::cout << "Verification Options (Z3 SMT Solver):\n";
    std::cout << "  --verify          Enable Z3 static verification of Rules/limit constraints\n";
    std::cout << "  --verify-contracts Verify requires/ensures function contracts\n";
    std::cout << "  --verify-overflow  Verify integer arithmetic cannot overflow\n";
    std::cout << "  --verify-report   Emit detailed proof results (implies --verify)\n";
    std::cout << "  --verify-concurrency Verify data race & deadlock freedom\n";
    std::cout << "  --verify-memory   Verify use-after-free & recursion bounds\n";
    std::cout << "  --smt-opt         Enable SMT-guided optimizations (eliminates proven-safe checks)\n";
    std::cout << "  --smt-timeout=N   Per-query Z3 solver timeout in ms (default: 5000)\n";
    std::cout << "  --verify-level=N  Verification depth: 0=none, 1=fast, 2=standard, 3=thorough\n";
    std::cout << "  --prove-report    Emit report of prove/assert_static outcomes (implies --verify)\n";
    std::cout << "  --borrow-debug    Emit borrow checker debug diagnostics to stderr\n";
    std::cout << "  --borrow-dump     Dump borrow state visualization after analysis\n";
    std::cout << "  --wild-stats      Print wild memory statistics at program exit\n";
    std::cout << "  --guard-pages     Enable guard pages around wild allocations (debug)\n";
    std::cout << "  --wildx-audit     Log WildX alloc/seal/exec/free events to stderr\n";
    std::cout << "  --wildx-guard-pages Enable guard pages around executable regions\n";
    std::cout << "  --gc-stats        Print GC statistics at program exit\n";
    std::cout << "  --gc-nursery-size=N Nursery size in bytes (default: 4MB, suffixes: K/M/G)\n";
    std::cout << "  --gc-threshold=N  Major GC trigger threshold (default: 64MB)\n";
    std::cout << "  --gc-max-heap=N   Maximum total heap size (default: unlimited)\n\n";
    std::cout << "Build Performance (v0.8.2):\n";
    std::cout << "  --incremental     Cache per-module IR, skip unchanged files on rebuild\n";
    std::cout << "  --cache-dir=<dir> IR cache directory (default: .aria-cache)\n\n";
    std::cout << "GPU Target Options (NVIDIA CUDA/PTX):\n";
    std::cout << "  --emit-ptx        Emit PTX assembly for GPU execution\n";
    std::cout << "  --target=<arch>   Target architecture (cpu, gpu, gpu+cpu)\n";
    std::cout << "  --gpu-arch=<sm>   CUDA compute capability (default: sm_50)\n";
    std::cout << "                    Examples: sm_50, sm_75, sm_86, sm_89, sm_90\n";
    std::cout << "  --gpu-opt=<lvl>   GPU optimization level 0-3 (default: 3)\n";
    std::cout << "  --gpu-debug       Embed debug info in PTX\n\n";
    std::cout << "Linking Options:\n";
    std::cout << "  -l<library>       Link against library (e.g., -lm, -lpthread)\n";
    std::cout << "  -L<path>          Add library search path\n";
    std::cout << "  -Wl,<option>      Pass option to linker\n\n";
    std::cout << "Warning Options:\n";
    std::cout << "  -Wall             Enable all warnings\n";
    std::cout << "  -Werror           Treat warnings as errors\n";
    std::cout << "  -W<warning>       Enable specific warning\n";
    std::cout << "  -Wno-<warning>    Disable specific warning\n\n";
    std::cout << "General:\n";
    std::cout << "  --version         Show version\n";
    std::cout << "  --help            Show this help\n\n";
    std::cout << "Examples (CPU):\n";
    std::cout << "  npkc hello.npk -o hello\n";
    std::cout << "  npkc main.npk utils.npk -o program\n";
    std::cout << "  npkc program.npk -o program -lm -lpthread\n";
    std::cout << "  npkc program.npk --emit-llvm -o program.ll\n";
    std::cout << "  npkc mylib.npk --shared -o libmylib.so\n";
    std::cout << "  npkc program.npk --static -o program -lm\n";
    std::cout << "  npkc test.npk --ast-dump\n\n";
    std::cout << "Examples (GPU):\n";
    std::cout << "  npkc physics.npk --emit-ptx -o physics.ptx\n";
    std::cout << "  npkc kernel.npk --emit-ptx --gpu-arch=sm_86 -o kernel.ptx\n";
    std::cout << "  npkc compute.npk --target=gpu --gpu-opt=3 -o compute.ptx\n\n";
    std::cout << "WASM Target Options (WebAssembly):\n";
    std::cout << "  --emit-wasm       Compile to WebAssembly (.wasm)\n";
    std::cout << "  --target=wasm32-wasi\n";
    std::cout << "                    Target wasm32-wasi (WASI system interface)\n\n";
    std::cout << "Examples (WASM):\n";
    std::cout << "  npkc hello.npk --emit-wasm -o hello.wasm\n";
    std::cout << "  npkc program.npk --target=wasm32-wasi -o program.wasm\n";
    std::cout << "  wasmtime hello.wasm                    # Run with wasmtime\n";
}

/**
 * Parse command-line arguments
 */
bool parse_arguments(int argc, char** argv, CompilerOptions& opts) {
    if (argc < 2) {
        std::cerr << "Error: No input file specified\n";
        std::cerr << "Run 'npkc --help' for usage information\n";
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
        } else if (arg == "--emit-ptx") {
            opts.emit_ptx = true;
        } else if (arg == "--emit-wasm") {
            opts.emit_wasm = true;
        } else if (arg.substr(0, 9) == "--target=") {
            opts.target_arch = arg.substr(9);
            if (opts.target_arch == "gpu") {
                opts.emit_ptx = true;  // GPU target implies PTX emission
            } else if (opts.target_arch == "wasm32-wasi" || opts.target_arch == "wasm32") {
                opts.emit_wasm = true;
                opts.wasm_target = "wasm32-wasi";
            } else if (opts.target_arch == "wasm32-unknown-unknown") {
                opts.emit_wasm = true;
                opts.wasm_target = "wasm32-unknown-unknown";
            } else if (opts.target_arch != "cpu" && opts.target_arch != "gpu+cpu") {
                std::cerr << "Error: Invalid target architecture: " << opts.target_arch << "\n";
                std::cerr << "Valid targets: cpu, gpu, gpu+cpu, wasm32-wasi, wasm32-unknown-unknown\n";
                return false;
            }
        } else if (arg.substr(0, 11) == "--gpu-arch=") {
            opts.gpu_arch = arg.substr(11);
            // Validate CUDA compute capability format (sm_XX)
            if (opts.gpu_arch.substr(0, 3) != "sm_" || opts.gpu_arch.length() < 5) {
                std::cerr << "Error: Invalid GPU architecture: " << opts.gpu_arch << "\n";
                std::cerr << "Format: sm_XX (e.g., sm_50, sm_75, sm_86)\n";
                return false;
            }
        } else if (arg.substr(0, 10) == "--gpu-opt=") {
            opts.gpu_opt_level = arg[10] - '0';
            if (opts.gpu_opt_level < 0 || opts.gpu_opt_level > 3) {
                std::cerr << "Error: Invalid GPU optimization level: " << arg << "\n";
                std::cerr << "Valid range: 0-3\n";
                return false;
            }
        } else if (arg == "--gpu-debug") {
            opts.enable_gpu_debug = true;
        } else if (arg == "-c") {
            opts.build_library = true;
        } else if (arg == "--shared") {
            opts.build_shared = true;
            opts.build_library = true;  // shared implies library mode (no failsafe)
        } else if (arg == "--static") {
            opts.build_static = true;
        } else if (arg == "-g") {
            opts.debug_info = true;
        } else if (arg == "-E") {
            opts.preprocess_only = true;
        } else if (arg == "--verify") {
            opts.verify = true;
        } else if (arg == "--verify-report") {
            opts.verify = true;
            opts.verify_report = true;
        } else if (arg == "--verify-contracts") {
            opts.verify = true;
            opts.verify_contracts = true;
        } else if (arg == "--verify-overflow") {
            opts.verify = true;
            opts.verify_overflow = true;
        } else if (arg == "--verify-concurrency") {
            opts.verify = true;
            opts.verify_concurrency = true;
        } else if (arg == "--verify-memory") {
            opts.verify = true;
            opts.verify_memory = true;
        } else if (arg == "--smt-opt") {
            opts.smt_opt = true;
            opts.verify = true;  // SMT optimization implies verification
        } else if (arg == "--prove-report") {
            opts.prove_report = true;
            opts.verify = true;  // prove-report implies verification
        } else if (arg == "--borrow-debug") {
            opts.borrow_debug = true;
        } else if (arg == "--borrow-dump") {
            opts.borrow_dump = true;
        } else if (arg == "--wild-stats") {
            opts.wild_stats = true;
        } else if (arg == "--guard-pages") {
            opts.guard_pages = true;
        } else if (arg == "--wildx-audit") {
            opts.wildx_audit = true;
        } else if (arg == "--wildx-guard-pages") {
            opts.wildx_guard_pages = true;
        } else if (arg == "--gc-stats") {
            opts.gc_stats = true;
        } else if (arg.substr(0, 18) == "--gc-nursery-size=") {
            std::string val = arg.substr(18);
            size_t multiplier = 1;
            if (!val.empty()) {
                char suffix = val.back();
                if (suffix == 'K' || suffix == 'k') { multiplier = 1024; val.pop_back(); }
                else if (suffix == 'M' || suffix == 'm') { multiplier = 1024*1024; val.pop_back(); }
                else if (suffix == 'G' || suffix == 'g') { multiplier = 1024ULL*1024*1024; val.pop_back(); }
            }
            opts.gc_nursery_size = std::stoull(val) * multiplier;
        } else if (arg.substr(0, 15) == "--gc-threshold=") {
            std::string val = arg.substr(15);
            size_t multiplier = 1;
            if (!val.empty()) {
                char suffix = val.back();
                if (suffix == 'K' || suffix == 'k') { multiplier = 1024; val.pop_back(); }
                else if (suffix == 'M' || suffix == 'm') { multiplier = 1024*1024; val.pop_back(); }
                else if (suffix == 'G' || suffix == 'g') { multiplier = 1024ULL*1024*1024; val.pop_back(); }
            }
            opts.gc_threshold = std::stoull(val) * multiplier;
        } else if (arg.substr(0, 14) == "--gc-max-heap=") {
            std::string val = arg.substr(14);
            size_t multiplier = 1;
            if (!val.empty()) {
                char suffix = val.back();
                if (suffix == 'K' || suffix == 'k') { multiplier = 1024; val.pop_back(); }
                else if (suffix == 'M' || suffix == 'm') { multiplier = 1024*1024; val.pop_back(); }
                else if (suffix == 'G' || suffix == 'g') { multiplier = 1024ULL*1024*1024; val.pop_back(); }
            }
            opts.gc_max_heap = std::stoull(val) * multiplier;
        } else if (arg.substr(0, 14) == "--smt-timeout=") {
            opts.smt_timeout = std::stoi(arg.substr(14));
            if (opts.smt_timeout < 0) opts.smt_timeout = 5000;
        } else if (arg.substr(0, 15) == "--verify-level=") {
            opts.verify_level = std::stoi(arg.substr(15));
            if (opts.verify_level < 0 || opts.verify_level > 3) {
                std::cerr << "Error: --verify-level must be 0, 1, 2, or 3\n";
                return 1;
            }
            // Level > 0 implies verification enabled
            if (opts.verify_level > 0) {
                opts.verify = true;
                if (opts.verify_level >= 2) {
                    opts.smt_opt = true;
                    opts.verify_overflow = true;
                    opts.verify_contracts = true;
                }
                if (opts.verify_level >= 3) {
                    opts.verify_concurrency = true;
                    opts.verify_memory = true;
                }
            }
        } else if (arg == "--incremental") {
            opts.incremental = true;
        } else if (arg.substr(0, 12) == "--cache-dir=") {
            opts.cache_dir = arg.substr(12);
            opts.incremental = true;  // Implies --incremental
        } else if (arg == "--ast-dump") {
            opts.dump_ast = true;
        } else if (arg == "--expand-macros") {
            opts.expand_macros = true;
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
                opts.link_args_ordered.push_back("-l" + arg.substr(2));
            } else {
                std::cerr << "Error: -l requires a library name\n";
                return false;
            }
        } else if (arg.substr(0, 2) == "-L") {
            // Library search path: -L/usr/local/lib, etc.
            if (arg.length() > 2) {
                opts.library_paths.push_back(arg.substr(2));
                opts.link_args_ordered.push_back("-L" + arg.substr(2));
            } else if (i + 1 < argc) {
                opts.library_paths.push_back(argv[++i]);
                opts.link_args_ordered.push_back("-L" + std::string(argv[i]));
            } else {
                std::cerr << "Error: -L requires a path\n";
                return false;
            }
        } else if (arg.substr(0, 4) == "-Wl,") {
            // Linker flag: -Wl,-rpath,/usr/local/lib, etc.
            opts.linker_flags.push_back(arg.substr(4));
            opts.link_args_ordered.push_back("-Wl," + arg.substr(4));
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
        } else if (opts.emit_ptx) {
            opts.output_file = opts.input_files[0].substr(0, opts.input_files[0].find_last_of('.')) + ".ptx";
        } else if (opts.emit_wasm) {
            opts.output_file = opts.input_files[0].substr(0, opts.input_files[0].find_last_of('.')) + ".wasm";
        } else if (opts.build_shared) {
            std::string base = opts.input_files[0].substr(0, opts.input_files[0].find_last_of('.'));
            opts.output_file = "lib" + base + ".so";
        } else if (opts.build_library) {
            opts.output_file = opts.input_files[0].substr(0, opts.input_files[0].find_last_of('.')) + ".o";
        } else {
            opts.output_file = "a.out";
        }
    }

    return true;
}

/**
 * Read source file
 */
bool read_source_file(const std::string& filename, std::string& source, npk::DiagnosticEngine& diags) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        diags.fatal(npk::SourceLocation(filename, 0, 0), "could not open source file");
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
 *   "source": "/path/to/file.npk",
 *   "imports": [
 *     {"module": "std.io", "path": "/path/to/std/io.npk"},
 *     {"module": "./utils", "path": "/path/to/utils.npk"}
 *   ],
 *   "error": null
 * }
 */
bool emit_dependencies(
    const std::string& source,
    const std::string& filename,
    const CompilerOptions& opts,
    npk::DiagnosticEngine& diags
) {
    // Phase 0: Preprocessing
    std::string preprocessed_source = source;
    try {
        npk::frontend::Preprocessor preprocessor;
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
    npk::frontend::Lexer lexer(preprocessed_source);
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
    npk::Parser parser(tokens);
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
    npk::sema::ModuleLoader module_loader(project_root);

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
    auto* program = dynamic_cast<npk::ProgramNode*>(module_node.get());
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
        if (auto* use_stmt = dynamic_cast<npk::UseStmt*>(decl.get())) {
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

// ============================================================================
// v0.8.2: Incremental Compilation — IR Cache
// ============================================================================
// Caches LLVM bitcode per source file. On recompilation, if the source hash
// matches the cached entry, the bitcode is loaded directly (skipping parse,
// sema, and codegen). Cache is stored in .aria-cache/<hex-hash>.bc alongside
// a .meta file recording the source path and hash.

#include <fstream>
#include <iomanip>

/// Compute a fast, collision-resistant hash of a string (FNV-1a 64-bit).
static uint64_t fnv1a_hash(const std::string& data) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (unsigned char c : data) {
        hash ^= c;
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

/// Get cache path for a given source hash.
static std::string get_cache_path(const std::string& cache_dir, uint64_t hash) {
    std::ostringstream oss;
    oss << cache_dir << "/" << std::hex << std::setfill('0') << std::setw(16) << hash << ".bc";
    return oss.str();
}

/// Try to load cached IR for a source file. Returns nullptr on miss.
static std::unique_ptr<llvm::Module> load_cached_ir(
    const std::string& cache_dir, const std::string& source,
    llvm::LLVMContext& context, bool verbose
) {
    uint64_t hash = fnv1a_hash(source);
    std::string bc_path = get_cache_path(cache_dir, hash);
    
    // Check if cache file exists
    if (!std::filesystem::exists(bc_path)) {
        if (verbose) std::cerr << "[cache] MISS: " << bc_path << "\n";
        return nullptr;
    }
    
    // Load the bitcode
    auto buf_or_err = llvm::MemoryBuffer::getFile(bc_path);
    if (!buf_or_err) {
        if (verbose) std::cerr << "[cache] read error: " << bc_path << "\n";
        return nullptr;
    }
    
    auto mod_or_err = llvm::parseBitcodeFile(buf_or_err->get()->getMemBufferRef(), context);
    if (!mod_or_err) {
        if (verbose) std::cerr << "[cache] parse error: " << bc_path << "\n";
        llvm::consumeError(mod_or_err.takeError());
        return nullptr;
    }
    
    if (verbose) std::cerr << "[cache] HIT: " << bc_path << "\n";
    return std::move(*mod_or_err);
}

/// Save compiled IR to cache.
static void save_ir_to_cache(
    const std::string& cache_dir, const std::string& source,
    llvm::Module* module, bool verbose
) {
    // Create cache directory if needed
    std::error_code ec;
    std::filesystem::create_directories(cache_dir, ec);
    if (ec) return;
    
    uint64_t hash = fnv1a_hash(source);
    std::string bc_path = get_cache_path(cache_dir, hash);
    
    std::error_code write_ec;
    llvm::raw_fd_ostream out(bc_path, write_ec);
    if (write_ec) {
        if (verbose) std::cerr << "[cache] write error: " << bc_path << "\n";
        return;
    }
    
    llvm::WriteBitcodeToFile(*module, out);
    if (verbose) std::cerr << "[cache] SAVED: " << bc_path << "\n";
}

/**
 * Compile source to LLVM Module
 * Note: Returns raw pointer - the IRGenerator must be kept alive!
 */
llvm::Module* compile_to_module(
    const std::string& source,
    const std::string& filename,
    const CompilerOptions& opts,
    npk::IRGenerator& ir_gen,
    npk::DiagnosticEngine& diags
) {
    // Phase 0: Preprocessing (Aria macros)
    std::string preprocessed_source = source;
    
    if (opts.verbose) {
        std::cout << "Phase 0: Preprocessing...\n";
    }
    
    try {
        npk::frontend::Preprocessor preprocessor;
        preprocessed_source = preprocessor.process(source, filename);
    } catch (const std::exception& e) {
        diags.error(npk::SourceLocation(filename, 0, 0), 
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
    
    npk::frontend::Lexer lexer(preprocessed_source);
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
            
            diags.error(npk::SourceLocation(filename, line, column), message);
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
    
    npk::Parser parser(tokens);
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
            
            diags.error(npk::SourceLocation(filename, line, column), message);
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
    npk::sema::TypeSystem type_system;
    npk::sema::SymbolTable symbol_table;
    
    // Create generic resolver and monomorphizer (Session 13: pass TypeSystem for struct specialization)
    npk::sema::GenericResolver generic_resolver;
    generic_resolver.setSymbolTable(&symbol_table);  // Task 4: Enable trait constraint checking
    generic_resolver.setTypeSystem(&type_system);    // Enable type resolution in generics
    npk::sema::Monomorphizer monomorphizer(&generic_resolver, &type_system);
    
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
    npk::sema::ModuleLoader module_loader(project_root);
    
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
        // Add stdlib/ and lib/ subdirectories relative to project root
        module_loader.getResolver().addSearchPath(compiler_dir + "/stdlib");
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
    npk::sema::TypeChecker type_checker(&type_system, &symbol_table, &generic_resolver, &monomorphizer, &module_loader, absolute_filename);
    
    // Run type checking on entire module (activates generic specialization)
    type_checker.check(module_node.get());
    
    // v0.8.4: Inject derive-generated synthetic nodes into the AST for IR codegen
    // Must be inserted BEFORE function declarations so impl methods are
    // generated before function bodies that call them.
    const auto& syntheticNodes = type_checker.getSyntheticNodes();
    if (!syntheticNodes.empty()) {
        if (module_node->type == npk::ASTNode::NodeType::PROGRAM) {
            auto* program = static_cast<npk::ProgramNode*>(module_node.get());
            // Find the first FUNC_DECL and insert before it
            auto it = program->declarations.begin();
            for (; it != program->declarations.end(); ++it) {
                if (*it && (*it)->type == npk::ASTNode::NodeType::FUNC_DECL) break;
            }
            program->declarations.insert(it, syntheticNodes.begin(), syntheticNodes.end());
            if (opts.verbose) {
                std::cout << "  Injected " << syntheticNodes.size()
                         << " synthetic derive node(s) into AST\n";
            }
        } else if (module_node->type == npk::ASTNode::NodeType::BLOCK) {
            auto* block = static_cast<npk::BlockStmt*>(module_node.get());
            for (const auto& node : syntheticNodes) {
                block->statements.push_back(node);
            }
        }
    }
    
    if (type_checker.hasErrors()) {
        for (const auto& err : type_checker.getErrors()) {
            diags.error(npk::SourceLocation(filename, 0, 0), err);
        }
        return nullptr;
    }
    
    // Print type checker warnings (v0.4.3) — non-fatal
    for (const auto& warn : type_checker.getWarnings()) {
        diags.warning(npk::SourceLocation(filename, 0, 0), warn);
    }

    // v0.23.7: MACRO-012 — --expand-macros: dump expansion trace collected during type-check
    if (opts.expand_macros) {
        const auto& log = type_checker.getMacroExpansionLog();
        if (log.empty()) {
            std::cout << "// no macro invocations found in " << filename << "\n";
        } else {
            for (const auto& rec : log) {
                std::cout << "// macro expansion at " << filename
                          << ":" << rec.line << ":" << rec.column << "\n";
                std::cout << "// call:     " << rec.callSite << "\n";
                std::cout << "// expanded: " << rec.expanded << "\n";
                std::cout << "\n";
            }
            std::cout << "// total: " << log.size() << " expansion(s)\n";
        }
        return nullptr;
    }

    // Validate that the mandatory failsafe() function exists
    // Every Aria executable must define failsafe(int32) for error handling accountability
    // Libraries (-c flag) are exempt as they're components, not final programs
    if (!opts.build_library && !type_checker.validateFailsafeExists()) {
        for (const auto& err : type_checker.getErrors()) {
            diags.error(npk::SourceLocation(filename, 0, 0), err);
        }
        return nullptr;
    }
    
    // Validate that the mandatory main() function exists
    // Every Aria executable must define main() as the program entry point
    // Libraries (-c flag) are exempt as they're components, not final programs
    if (!opts.build_library && !type_checker.validateMainExists()) {
        for (const auto& err : type_checker.getErrors()) {
            diags.error(npk::SourceLocation(filename, 0, 0), err);
        }
        return nullptr;
    }
    
    // Type check all loaded modules (for cross-module calls)
    const auto& loaded_modules = module_loader.getLoadedModules();
    for (const auto& [path, loaded_module] : loaded_modules) {
        if (loaded_module && loaded_module->ast) {
            // Skip modules already type-checked during import processing
            if (loaded_module->state == npk::sema::ModuleState::CHECKED) {
                if (opts.verbose) {
                    std::cout << "  Skipping already-checked module: " << path << "\n";
                }
                continue;
            }
            
            if (opts.verbose) {
                std::cout << "  Type checking module: " << path << "\n";
            }
            // Create a separate type checker for each module
            // (They share the same type system but have separate symbol tables)
            npk::sema::TypeChecker module_type_checker(&type_system, loaded_module->moduleInfo->getSymbolTable(), 
                                                        &generic_resolver, &monomorphizer, &module_loader, path);
            module_type_checker.check(loaded_module->ast.get());
            loaded_module->state = npk::sema::ModuleState::CHECKED;
            
            if (module_type_checker.hasErrors()) {
                for (const auto& err : module_type_checker.getErrors()) {
                    diags.error(npk::SourceLocation(path, 0, 0), err);
                }
                return nullptr;
            }
        }
    }
    
    // Report symbol table warnings (e.g., shadowing)
    if (symbol_table.hasWarnings()) {
        for (const auto& warn : symbol_table.getWarnings()) {
            diags.warning(npk::SourceLocation(filename, 0, 0), warn);
        }
    }

    // Warning analysis pass (v0.20.0): UNUSED_FUNCTION, EMPTY_BLOCK, etc.
    {
        npk::WarningConfig warn_config;
        // Parse -Wall / -Wno-* / -Werror flags from CLI
        for (const auto& flag : opts.warning_flags) {
            npk::WarningFlagParser::parseFlag(flag, warn_config);
        }
        npk::WarningAnalyzer warn_analyzer(diags, warn_config);
        warn_analyzer.analyze(module_node.get());
    }
    
    // Dead branch elimination sets — declared outside #ifdef so they're available to codegen
    std::set<npk::ASTNode*> dead_branch_true_set;
    std::set<npk::ASTNode*> dead_branch_false_set;

    // Bounds check elimination set — declared outside #ifdef so it's available to codegen
    std::set<npk::ASTNode*> bounds_safe_set;

    // Overflow check elimination set — declared outside #ifdef so it's available to codegen
    std::set<npk::ASTNode*> overflow_safe_set;

    // Null check elimination set — declared outside #ifdef so it's available to codegen
    std::set<npk::ASTNode*> null_check_safe_set;

    // Division-by-zero elimination set — declared outside #ifdef so it's available to codegen (v0.14.4)
    std::set<npk::ASTNode*> div_safe_set;

    // Loop invariant hoisting map — declared outside #ifdef so it's available to codegen
    std::map<npk::ASTNode*, std::vector<npk::ASTNode*>> loop_hoist_map;

    // Limit check elimination set — declared outside #ifdef so it's available to codegen
    std::set<npk::ASTNode*> limit_check_safe_set;

    // Defaults fallback elimination set — declared outside #ifdef so it's available to codegen
    std::set<npk::ASTNode*> defaults_safe_set;

#ifdef ARIA_HAS_Z3
    // v0.24.6 (COMPTIME-011): Always-on comptime limit verification.
    // For `limit<Rules> T:x = init;` where `init` resolves to a comptime
    // constant (literal, unary-minus literal, or `fixed` identifier), evaluate
    // the rule against the resolved value at compile time. Violations become
    // hard compile errors with no need for `--verify`.
    {
        auto* program_ct = dynamic_cast<npk::ProgramNode*>(module_node.get());
        const auto& rules_table_ct = type_checker.getRulesTable();
        if (program_ct && !rules_table_ct.empty()) {
            std::map<std::string, int64_t> ctInt;
            std::map<std::string, double>  ctFloat;
            std::function<bool(npk::ASTNode*, int64_t&, double&, bool&, bool&)> resolveCT;
            resolveCT = [&](npk::ASTNode* expr, int64_t& iv, double& fv,
                            bool& isI, bool& isF) -> bool {
                if (!expr) return false;
                if (expr->type == npk::ASTNode::NodeType::LITERAL) {
                    auto* lit = static_cast<npk::LiteralExpr*>(expr);
                    if (std::holds_alternative<int64_t>(lit->value)) {
                        iv = std::get<int64_t>(lit->value); isI = true; return true;
                    }
                    if (std::holds_alternative<double>(lit->value)) {
                        fv = std::get<double>(lit->value); isF = true; return true;
                    }
                    return false;
                }
                if (expr->type == npk::ASTNode::NodeType::UNARY_OP) {
                    auto* unary = static_cast<npk::UnaryExpr*>(expr);
                    if (unary->op.type == npk::frontend::TokenType::TOKEN_MINUS) {
                        int64_t ii_v = 0; double ff_v = 0.0; bool ii = false, ff = false;
                        if (resolveCT(unary->operand.get(), ii_v, ff_v, ii, ff)) {
                            if (ii) { iv = -ii_v; isI = true; return true; }
                            if (ff) { fv = -ff_v; isF = true; return true; }
                        }
                    }
                    return false;
                }
                if (expr->type == npk::ASTNode::NodeType::IDENTIFIER) {
                    auto* id = static_cast<npk::IdentifierExpr*>(expr);
                    auto it = ctInt.find(id->name);
                    if (it != ctInt.end()) { iv = it->second; isI = true; return true; }
                    auto fit = ctFloat.find(id->name);
                    if (fit != ctFloat.end()) { fv = fit->second; isF = true; return true; }
                }
                return false;
            };

            npk::Z3Verifier z3vCT(opts.smt_timeout);
            z3vCT.setTypeSystem(&type_system);
            for (const auto& [name, rp] : rules_table_ct) z3vCT.registerRules(name, rp);

            std::function<void(npk::ASTNode*)> walkCT = [&](npk::ASTNode* node) {
                if (!node) return;
                if (node->type == npk::ASTNode::NodeType::VAR_DECL) {
                    auto* var = static_cast<npk::VarDeclStmt*>(node);
                    if (var->isFixed && var->initializer && !var->varName.empty()) {
                        int64_t cIv = 0; double cFv = 0.0; bool cI = false, cF = false;
                        if (resolveCT(var->initializer.get(), cIv, cFv, cI, cF)) {
                            if (cI) ctInt[var->varName] = cIv;
                            else if (cF) ctFloat[var->varName] = cFv;
                        }
                    }
                    if (!var->limitRulesName.empty() && var->initializer) {
                        int64_t iv = 0; double fv = 0.0; bool isI = false, isF = false;
                        resolveCT(var->initializer.get(), iv, fv, isI, isF);
                        if (isI || isF) {
                            std::vector<npk::VerifyOutcome> outs;
                            if (isI) z3vCT.verifyLimitInt(var->limitRulesName, iv, outs,
                                                         var->line, var->column);
                            else     z3vCT.verifyLimitFloat(var->limitRulesName, fv, outs,
                                                           var->line, var->column);
                            for (const auto& out : outs) {
                                if (out.result == npk::VerifyResult::DISPROVEN) {
                                    diags.error(npk::SourceLocation(filename, out.line, out.column),
                                                "[comptime] limit<" + var->limitRulesName +
                                                "> violated by comptime-known value: " + out.detail);
                                }
                            }
                        }
                    }
                }
                if (node->type == npk::ASTNode::NodeType::PROGRAM) {
                    for (const auto& d : static_cast<npk::ProgramNode*>(node)->declarations) walkCT(d.get());
                } else if (node->type == npk::ASTNode::NodeType::FUNC_DECL) {
                    auto* f = static_cast<npk::FuncDeclStmt*>(node);
                    if (f->body) walkCT(f->body.get());
                } else if (node->type == npk::ASTNode::NodeType::BLOCK) {
                    for (const auto& s : static_cast<npk::BlockStmt*>(node)->statements) walkCT(s.get());
                } else if (node->type == npk::ASTNode::NodeType::TYPE_DECL) {
                    auto* td = static_cast<npk::TypeDeclStmt*>(node);
                    if (td->createFunc) walkCT(td->createFunc.get());
                    if (td->destroyFunc) walkCT(td->destroyFunc.get());
                    for (const auto& m : td->methods) walkCT(m.get());
                }
            };
            walkCT(program_ct);
        }
        if (diags.hasErrors()) {
            diags.printAll();
            return nullptr;
        }
    }

    // Phase 3.25: Z3 SMT Verification (static contract verification)
    // v0.14.3: --verify-level gating: 0=none, 1=fast, 2=standard, 3=thorough
    std::set<std::string> ustack_opt_funcs;  // Functions eligible for SMT-optimized user stack
    std::set<std::string> uhash_opt_funcs;   // Functions eligible for SMT-optimized user hash
    if (opts.verify && opts.verify_level != 0) {
        auto verify_start = std::chrono::steady_clock::now();
        if (opts.verbose) {
            std::cout << "Phase 3.25: Z3 SMT verification...\n";
            if (opts.verify_level >= 0) {
                std::cout << "  Verify level: " << opts.verify_level
                          << (opts.verify_level == 1 ? " (fast)" :
                              opts.verify_level == 2 ? " (standard)" : " (thorough)")
                          << "\n";
            }
        }
        
        npk::Z3Verifier z3v(opts.smt_timeout);
        z3v.setTypeSystem(&type_system);
        z3v.setVerbose(opts.verbose);
        
        // Register all Rules declarations from the type checker
        const auto& rules_table = type_checker.getRulesTable();
        for (const auto& [name, rules_ptr] : rules_table) {
            z3v.registerRules(name, rules_ptr);
        }
        
        // Verify Rules consistency (check for contradictory constraints)
        for (const auto& [name, rules_ptr] : rules_table) {
            std::vector<npk::VerifyOutcome> cons_outcomes;
            z3v.verifyRulesConsistency(name, cons_outcomes);
            for (const auto& out : cons_outcomes) {
                if (out.result == npk::VerifyResult::DISPROVEN) {
                    diags.error(npk::SourceLocation(filename, rules_ptr->line, rules_ptr->column),
                                "[z3] " + out.detail);
                } else if (out.result == npk::VerifyResult::UNKNOWN && opts.verbose) {
                    diags.warning(npk::SourceLocation(filename, rules_ptr->line, rules_ptr->column),
                                  "[z3] Could not determine consistency of Rules '" + name + "'");
                }
            }
        }
        
        // Walk AST to find limit<> variable declarations with literal initializers
        // and verify them against their Rules constraints
        auto* program = dynamic_cast<npk::ProgramNode*>(module_node.get());
        if (program) {
            // v0.24.6 (COMPTIME-011): track `fixed` variables with literal
            // initializers as comptime-known constants so that `limit<Rules>`
            // applied to identifier-initializers (which reference these
            // constants) can be CTFE-evaluated and proven without Z3.
            std::map<std::string, int64_t> comptimeInt;
            std::map<std::string, double>  comptimeFloat;

            // Resolve an initializer expression to a comptime int/float if
            // possible. Recurses through LITERAL, UNARY_OP(MINUS,...) and
            // IDENTIFIER bindings to comptimeInt/comptimeFloat.
            std::function<bool(npk::ASTNode*, int64_t&, double&, bool&, bool&)>
                resolveComptime;
            resolveComptime = [&](npk::ASTNode* expr, int64_t& iv, double& fv,
                                  bool& isI, bool& isF) -> bool {
                if (!expr) return false;
                if (expr->type == npk::ASTNode::NodeType::LITERAL) {
                    auto* lit = static_cast<npk::LiteralExpr*>(expr);
                    if (std::holds_alternative<int64_t>(lit->value)) {
                        iv = std::get<int64_t>(lit->value); isI = true; return true;
                    }
                    if (std::holds_alternative<double>(lit->value)) {
                        fv = std::get<double>(lit->value); isF = true; return true;
                    }
                    return false;
                }
                if (expr->type == npk::ASTNode::NodeType::UNARY_OP) {
                    auto* unary = static_cast<npk::UnaryExpr*>(expr);
                    if (unary->op.type == npk::frontend::TokenType::TOKEN_MINUS) {
                        int64_t inner_i = 0; double inner_f = 0.0;
                        bool ii = false, ff = false;
                        if (resolveComptime(unary->operand.get(), inner_i, inner_f, ii, ff)) {
                            if (ii) { iv = -inner_i; isI = true; return true; }
                            if (ff) { fv = -inner_f; isF = true; return true; }
                        }
                    }
                    return false;
                }
                if (expr->type == npk::ASTNode::NodeType::IDENTIFIER) {
                    auto* id = static_cast<npk::IdentifierExpr*>(expr);
                    auto it = comptimeInt.find(id->name);
                    if (it != comptimeInt.end()) { iv = it->second; isI = true; return true; }
                    auto fit = comptimeFloat.find(id->name);
                    if (fit != comptimeFloat.end()) { fv = fit->second; isF = true; return true; }
                }
                return false;
            };

            // Recursive lambda to walk statements
            std::function<void(npk::ASTNode*)> walkNode = [&](npk::ASTNode* node) {
                if (!node) return;
                
                if (node->type == npk::ASTNode::NodeType::VAR_DECL) {
                    auto* var = static_cast<npk::VarDeclStmt*>(node);

                    // v0.24.6 (COMPTIME-011): record `fixed` variables with
                    // comptime-known initializer values for later resolution.
                    if (var->isFixed && var->initializer && !var->varName.empty()) {
                        int64_t cIv = 0; double cFv = 0.0;
                        bool cI = false, cF = false;
                        if (resolveComptime(var->initializer.get(), cIv, cFv, cI, cF)) {
                            if (cI) comptimeInt[var->varName] = cIv;
                            else if (cF) comptimeFloat[var->varName] = cFv;
                        }
                    }

                    if (!var->limitRulesName.empty() && var->initializer) {
                        int64_t intVal = 0;
                        double floatVal = 0.0;
                        bool isInt = false;
                        bool isFloat = false;

                        // v0.24.6 (COMPTIME-011): try comptime resolution first
                        // (handles literals, unary-minus literals, and `fixed`
                        // identifier references — all routed through CTFE-style
                        // evaluation, no Z3 needed).
                        resolveComptime(var->initializer.get(), intVal, floatVal, isInt, isFloat);

                        if (isInt) {
                            std::vector<npk::VerifyOutcome> outcomes;
                            z3v.verifyLimitInt(var->limitRulesName, intVal, outcomes,
                                               var->line, var->column);
                            for (const auto& out : outcomes) {
                                if (out.result == npk::VerifyResult::DISPROVEN) {
                                    diags.error(npk::SourceLocation(filename, out.line, out.column),
                                                "[z3] " + out.detail);
                                } else if (opts.verify_report && out.result == npk::VerifyResult::PROVEN) {
                                    diags.note(npk::SourceLocation(filename, out.line, out.column),
                                               "[z3] " + out.detail);
                                }
                            }
                        } else if (isFloat) {
                            std::vector<npk::VerifyOutcome> outcomes;
                            z3v.verifyLimitFloat(var->limitRulesName, floatVal, outcomes,
                                                 var->line, var->column);
                            for (const auto& out : outcomes) {
                                if (out.result == npk::VerifyResult::DISPROVEN) {
                                    diags.error(npk::SourceLocation(filename, out.line, out.column),
                                                "[z3] " + out.detail);
                                } else if (opts.verify_report && out.result == npk::VerifyResult::PROVEN) {
                                    diags.note(npk::SourceLocation(filename, out.line, out.column),
                                               "[z3] " + out.detail);
                                }
                            }
                        }
                    }
                }
                
                // Recurse into child nodes
                if (node->type == npk::ASTNode::NodeType::PROGRAM) {
                    auto* prog = static_cast<npk::ProgramNode*>(node);
                    for (const auto& decl : prog->declarations) {
                        walkNode(decl.get());
                    }
                } else if (node->type == npk::ASTNode::NodeType::FUNC_DECL) {
                    auto* func = static_cast<npk::FuncDeclStmt*>(node);
                    if (func->body) walkNode(func->body.get());
                } else if (node->type == npk::ASTNode::NodeType::BLOCK) {
                    auto* block = static_cast<npk::BlockStmt*>(node);
                    for (const auto& stmt : block->statements) {
                        walkNode(stmt.get());
                    }
                } else if (node->type == npk::ASTNode::NodeType::TYPE_DECL) {
                    auto* typeDecl = static_cast<npk::TypeDeclStmt*>(node);
                    if (typeDecl->createFunc) walkNode(typeDecl->createFunc.get());
                    if (typeDecl->destroyFunc) walkNode(typeDecl->destroyFunc.get());
                    for (const auto& method : typeDecl->methods) {
                        walkNode(method.get());
                    }
                }
            };
            
            walkNode(program);
        }
        
        // Phase 2: Verify requires/ensures contracts (v0.3.4)
        // v0.14.3: requires verify-level >= 2 (standard)
        if (opts.verify_contracts && program && opts.verify_level != 1) {
            if (opts.verbose) {
                std::cout << "  Phase 2: Verifying function contracts...\n";
            }
            
            std::function<void(npk::ASTNode*)> walkContracts = [&](npk::ASTNode* node) {
                if (!node) return;
                
                if (node->type == npk::ASTNode::NodeType::FUNC_DECL) {
                    auto* func = static_cast<npk::FuncDeclStmt*>(node);
                    if (!func->preconditions.empty() || !func->postconditions.empty()) {
                        std::vector<npk::VerifyOutcome> outcomes;
                        z3v.verifyFunctionContract(func, outcomes);
                        for (const auto& out : outcomes) {
                            if (out.result == npk::VerifyResult::DISPROVEN) {
                                diags.error(npk::SourceLocation(filename, out.line, out.column),
                                            "[z3-contract] " + out.detail);
                            } else if (out.result == npk::VerifyResult::UNKNOWN && opts.verbose) {
                                diags.note(npk::SourceLocation(filename, out.line, out.column),
                                           "[z3-contract] " + out.detail);
                            } else if (opts.verify_report && out.result == npk::VerifyResult::PROVEN) {
                                diags.note(npk::SourceLocation(filename, out.line, out.column),
                                           "[z3-contract] " + out.detail);
                            }
                        }
                    }
                    // Recurse into function body
                    if (func->body) walkContracts(func->body.get());
                }
                
                // Recurse into containers
                if (node->type == npk::ASTNode::NodeType::PROGRAM) {
                    auto* prog = static_cast<npk::ProgramNode*>(node);
                    for (const auto& decl : prog->declarations) {
                        walkContracts(decl.get());
                    }
                } else if (node->type == npk::ASTNode::NodeType::BLOCK) {
                    auto* block = static_cast<npk::BlockStmt*>(node);
                    for (const auto& stmt : block->statements) {
                        walkContracts(stmt.get());
                    }
                } else if (node->type == npk::ASTNode::NodeType::TYPE_DECL) {
                    auto* typeDecl = static_cast<npk::TypeDeclStmt*>(node);
                    if (typeDecl->createFunc) walkContracts(typeDecl->createFunc.get());
                    if (typeDecl->destroyFunc) walkContracts(typeDecl->destroyFunc.get());
                    for (const auto& method : typeDecl->methods) {
                        walkContracts(method.get());
                    }
                }
            };
            
            walkContracts(program);
        }

        // Phase 2b: Cross-function contract propagation (v0.5.2)
        // + Rules<T> narrowing verification (v0.5.3)
        // Walk each function body for calls to contracted functions and verify
        // that call-site arguments satisfy the callee's requires clauses,
        // using the caller's own constraints as assumptions.
        // Also verify Rules narrowing at call sites and pick exhaustiveness.
        // v0.14.3: requires verify-level >= 2 (standard)
        if (opts.verify_contracts && program && opts.verify_level != 1) {
            // Build lookup tables
            std::map<std::string, npk::FuncDeclStmt*> contract_funcs;  // has requires
            std::map<std::string, npk::FuncDeclStmt*> ensures_funcs;   // has ensures
            std::map<std::string, npk::FuncDeclStmt*> all_funcs;       // all functions
            for (const auto& decl : program->declarations) {
                if (decl->type == npk::ASTNode::NodeType::FUNC_DECL) {
                    auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                    all_funcs[func->funcName] = func;
                    if (!func->preconditions.empty()) {
                        contract_funcs[func->funcName] = func;
                    }
                    if (!func->postconditions.empty()) {
                        ensures_funcs[func->funcName] = func;
                    }
                }
            }

            if (!contract_funcs.empty() || !rules_table.empty()) {
                if (opts.verbose) {
                    std::cout << "  Phase 2b: Cross-function contract propagation...\n";
                }

                for (const auto& decl : program->declarations) {
                    if (decl->type != npk::ASTNode::NodeType::FUNC_DECL) continue;
                    auto* caller = static_cast<npk::FuncDeclStmt*>(decl.get());
                    if (!caller->body) continue;

                    // Collect caller's Rules-constrained variables
                    std::map<std::string, std::pair<npk::RulesDeclStmt*, std::string>> callerVarRules;

                    // From parameters
                    for (auto& param : caller->parameters) {
                        if (param->type == npk::ASTNode::NodeType::PARAMETER) {
                            auto* pn = static_cast<npk::ParameterNode*>(param.get());
                            // Check if type implies a Rules constraint
                            if (pn->typeNode) {
                                std::string tname = pn->typeNode->toString();
                                auto it = rules_table.find(tname);
                                if (it != rules_table.end()) {
                                    callerVarRules[pn->paramName] = {it->second, tname};
                                }
                            }
                        }
                        // Also check VarDecl-style params with limit<>
                        if (param->type == npk::ASTNode::NodeType::VAR_DECL) {
                            auto* vd = static_cast<npk::VarDeclStmt*>(param.get());
                            if (!vd->limitRulesName.empty()) {
                                auto it = rules_table.find(vd->limitRulesName);
                                if (it != rules_table.end()) {
                                    callerVarRules[vd->varName] = {it->second, vd->typeName};
                                }
                            }
                        }
                    }

                    // Walk body for CALL expressions and limit<> locals
                    std::vector<std::pair<std::string, npk::FuncDeclStmt*>> ensuresFacts;
                    std::function<void(npk::ASTNode*)> walkCalls;
                    walkCalls = [&](npk::ASTNode* node) {
                        if (!node) return;
                        using NT = npk::ASTNode::NodeType;

                        switch (node->type) {
                            case NT::CALL: {
                                auto* call = static_cast<npk::CallExpr*>(node);
                                if (call->callee && call->callee->type == NT::IDENTIFIER) {
                                    auto* ident = static_cast<npk::IdentifierExpr*>(
                                        call->callee.get());
                                    auto it = contract_funcs.find(ident->name);
                                    if (it != contract_funcs.end()) {
                                        npk::FuncDeclStmt* callee = it->second;
                                        std::vector<npk::ASTNode*> args;
                                        for (auto& arg : call->arguments) {
                                            args.push_back(arg.get());
                                        }
                                        std::vector<npk::VerifyOutcome> outcomes;
                                        z3v.verifyCallSiteContract(
                                            callee, caller, args, callerVarRules,
                                            ensuresFacts,
                                            outcomes, call->line, call->column);

                                        for (const auto& out : outcomes) {
                                            if (out.result == npk::VerifyResult::DISPROVEN) {
                                                std::cerr << filename << ":"
                                                          << out.line << ":" << out.column
                                                          << ": error: [contract] " << out.detail << "\n";
                                            } else if (out.result == npk::VerifyResult::PROVEN) {
                                                if (opts.verify_report || opts.verbose) {
                                                    std::cout << "  [contract] " << out.detail << "\n";
                                                }
                                            } else if (opts.verbose) {
                                                std::cout << "  [contract] " << out.detail << "\n";
                                            }
                                        }
                                    }
                                }
                                // v0.5.3: Rules narrowing check for any function
                                // with Rules-constrained parameters
                                if (call->callee && call->callee->type == NT::IDENTIFIER) {
                                    auto* ident = static_cast<npk::IdentifierExpr*>(
                                        call->callee.get());
                                    auto fit = all_funcs.find(ident->name);
                                    if (fit != all_funcs.end()) {
                                        npk::FuncDeclStmt* callee = fit->second;
                                        std::vector<npk::ASTNode*> args;
                                        for (auto& arg : call->arguments) {
                                            args.push_back(arg.get());
                                        }
                                        std::vector<npk::VerifyOutcome> narrowOutcomes;
                                        z3v.verifyRulesNarrowing(
                                            callee, args, callerVarRules,
                                            narrowOutcomes, call->line, call->column);

                                        for (const auto& out : narrowOutcomes) {
                                            if (out.result == npk::VerifyResult::DISPROVEN) {
                                                std::cerr << filename << ":"
                                                          << out.line << ":" << out.column
                                                          << ": error: [narrowing] " << out.detail << "\n";
                                            } else if (out.result == npk::VerifyResult::PROVEN) {
                                                if (opts.verify_report || opts.verbose) {
                                                    std::cout << "  [narrowing] " << out.detail << "\n";
                                                }
                                            } else if (opts.verbose) {
                                                std::cout << "  [narrowing] " << out.detail << "\n";
                                            }
                                        }
                                    }
                                }
                                // Recurse into args
                                for (auto& arg : call->arguments) {
                                    walkCalls(arg.get());
                                }
                                break;
                            }

                            case NT::VAR_DECL: {
                                auto* vd = static_cast<npk::VarDeclStmt*>(node);
                                if (!vd->limitRulesName.empty()) {
                                    auto it = rules_table.find(vd->limitRulesName);
                                    if (it != rules_table.end()) {
                                        callerVarRules[vd->varName] = {it->second, vd->typeName};

                                        // v0.5.3: Check if Rules excludes null
                                        std::vector<npk::VerifyOutcome> nullOutcomes;
                                        z3v.proveRulesExcludesNull(
                                            vd->limitRulesName, vd->typeName,
                                            nullOutcomes, vd->line, vd->column);
                                        for (const auto& out : nullOutcomes) {
                                            if (out.result == npk::VerifyResult::PROVEN) {
                                                if (opts.verify_report || opts.verbose) {
                                                    std::cout << "  [null-safety] " << out.detail << "\n";
                                                }
                                            } else if (out.result == npk::VerifyResult::DISPROVEN) {
                                                if (opts.verbose) {
                                                    std::cout << "  [null-safety] " << out.detail << "\n";
                                                }
                                            }
                                        }
                                    }
                                }
                                // Track ensures-derived facts from call initializers
                                // Unwrap raw/drop wrappers: raw f(x) → CALL("raw",[CALL("f",...)])
                                if (vd->initializer && vd->initializer->type == NT::CALL) {
                                    npk::ASTNode* initNode = vd->initializer.get();
                                    auto* call = static_cast<npk::CallExpr*>(initNode);
                                    // Unwrap raw/drop wrappers
                                    if (call->callee && call->callee->type == NT::IDENTIFIER) {
                                        auto* wrapIdent = static_cast<npk::IdentifierExpr*>(call->callee.get());
                                        if ((wrapIdent->name == "raw" || wrapIdent->name == "drop") &&
                                            !call->arguments.empty() &&
                                            call->arguments[0]->type == NT::CALL) {
                                            call = static_cast<npk::CallExpr*>(call->arguments[0].get());
                                        }
                                    }
                                    if (call->callee && call->callee->type == NT::IDENTIFIER) {
                                        auto* ident = static_cast<npk::IdentifierExpr*>(call->callee.get());
                                        auto eit = ensures_funcs.find(ident->name);
                                        if (eit != ensures_funcs.end()) {
                                            ensuresFacts.push_back({vd->varName, eit->second});
                                        }
                                    }
                                }
                                if (vd->initializer) walkCalls(vd->initializer.get());
                                break;
                            }

                            case NT::BLOCK: {
                                auto* blk = static_cast<npk::BlockStmt*>(node);
                                for (auto& s : blk->statements) walkCalls(s.get());
                                break;
                            }

                            case NT::IF: {
                                auto* s = static_cast<npk::IfStmt*>(node);
                                if (s->condition) walkCalls(s->condition.get());
                                walkCalls(s->thenBranch.get());
                                walkCalls(s->elseBranch.get());
                                break;
                            }

                            case NT::WHILE: {
                                auto* s = static_cast<npk::WhileStmt*>(node);
                                if (!s->invariants.empty()) {
                                    std::vector<npk::ASTNode*> invPtrs;
                                    for (auto& inv : s->invariants) invPtrs.push_back(inv.get());
                                    std::vector<npk::VerifyOutcome> invOutcomes;
                                    z3v.verifyLoopInvariant(caller, invPtrs, invOutcomes, s->line, s->column);
                                }
                                if (s->condition) walkCalls(s->condition.get());
                                walkCalls(s->body.get());
                                break;
                            }

                            case NT::FOR: {
                                auto* s = static_cast<npk::ForStmt*>(node);
                                if (!s->invariants.empty()) {
                                    std::vector<npk::ASTNode*> invPtrs;
                                    for (auto& inv : s->invariants) invPtrs.push_back(inv.get());
                                    std::vector<npk::VerifyOutcome> invOutcomes;
                                    z3v.verifyLoopInvariant(caller, invPtrs, invOutcomes, s->line, s->column);
                                }
                                walkCalls(s->body.get());
                                break;
                            }

                            case NT::LOOP: {
                                auto* s = static_cast<npk::LoopStmt*>(node);
                                if (!s->invariants.empty()) {
                                    std::vector<npk::ASTNode*> invPtrs;
                                    for (auto& inv : s->invariants) invPtrs.push_back(inv.get());
                                    std::vector<npk::VerifyOutcome> invOutcomes;
                                    z3v.verifyLoopInvariant(caller, invPtrs, invOutcomes, s->line, s->column);
                                }
                                walkCalls(s->body.get());
                                break;
                            }

                            case NT::TILL: {
                                auto* s = static_cast<npk::TillStmt*>(node);
                                if (!s->invariants.empty()) {
                                    std::vector<npk::ASTNode*> invPtrs;
                                    for (auto& inv : s->invariants) invPtrs.push_back(inv.get());
                                    std::vector<npk::VerifyOutcome> invOutcomes;
                                    z3v.verifyLoopInvariant(caller, invPtrs, invOutcomes, s->line, s->column);
                                }
                                walkCalls(s->body.get());
                                break;
                            }

                            case NT::WHEN: {
                                auto* s = static_cast<npk::WhenStmt*>(node);
                                if (!s->invariants.empty()) {
                                    std::vector<npk::ASTNode*> invPtrs;
                                    for (auto& inv : s->invariants) invPtrs.push_back(inv.get());
                                    std::vector<npk::VerifyOutcome> invOutcomes;
                                    z3v.verifyLoopInvariant(caller, invPtrs, invOutcomes, s->line, s->column);
                                }
                                walkCalls(s->body.get());
                                walkCalls(s->then_block.get());
                                walkCalls(s->end_block.get());
                                break;
                            }

                            case NT::EXPRESSION_STMT: {
                                auto* s = static_cast<npk::ExpressionStmt*>(node);
                                walkCalls(s->expression.get());
                                break;
                            }

                            case NT::RETURN: {
                                auto* s = static_cast<npk::ReturnStmt*>(node);
                                if (s->value) walkCalls(s->value.get());
                                break;
                            }

                            case NT::PASS: {
                                auto* s = static_cast<npk::PassStmt*>(node);
                                if (s->value) walkCalls(s->value.get());
                                break;
                            }

                            case NT::FAIL: {
                                auto* s = static_cast<npk::FailStmt*>(node);
                                if (s->errorCode) walkCalls(s->errorCode.get());
                                break;
                            }

                            case NT::PICK: {
                                // v0.5.3: Pick exhaustiveness via SMT
                                auto* pick = static_cast<npk::PickStmt*>(node);
                                // Collect patterns for exhaustiveness check
                                std::vector<npk::ASTNode*> patterns;
                                bool hasUnreachable = false;
                                for (auto& c : pick->cases) {
                                    auto* pc = static_cast<npk::PickCase*>(c.get());
                                    if (pc->is_unreachable) {
                                        hasUnreachable = true;
                                    } else if (pc->pattern) {
                                        patterns.push_back(pc->pattern.get());
                                    }
                                }
                                // Only check if there's no unreachable marker
                                // (unreachable means programmer asserts it can't happen)
                                if (!hasUnreachable && !patterns.empty()) {
                                    std::vector<npk::VerifyOutcome> pickOutcomes;
                                    z3v.provePickExhaustiveness(
                                        pick->selector.get(), patterns,
                                        callerVarRules, pickOutcomes,
                                        pick->line, pick->column);

                                    for (const auto& out : pickOutcomes) {
                                        if (out.result == npk::VerifyResult::DISPROVEN) {
                                            std::cerr << filename << ":"
                                                      << out.line << ":" << out.column
                                                      << ": warning: [exhaustiveness] " << out.detail << "\n";
                                        } else if (out.result == npk::VerifyResult::PROVEN) {
                                            if (opts.verify_report || opts.verbose) {
                                                std::cout << "  [exhaustiveness] " << out.detail << "\n";
                                            }
                                        } else if (opts.verbose) {
                                            std::cout << "  [exhaustiveness] " << out.detail << "\n";
                                        }
                                    }
                                }
                                // Recurse into selector and case bodies
                                walkCalls(pick->selector.get());
                                for (auto& c : pick->cases) {
                                    auto* pc = static_cast<npk::PickCase*>(c.get());
                                    walkCalls(pc->body.get());
                                }
                                break;
                            }

                            default:
                                break;
                        }
                    };
                    walkCalls(caller->body.get());
                }
            }
        }

        // Phase 2c: Automatic Rules widening/narrowing (v0.5.3)
        // For functions with Rules-constrained parameters that return integer types,
        // infer the tightest return value bounds using Z3 optimization.
        // v0.14.3: requires verify-level >= 2 (standard)
        if (opts.verify_contracts && program && !rules_table.empty() && opts.verify_level != 1) {
            if (opts.verbose) {
                std::cout << "  Phase 2c: Automatic return bounds inference...\n";
            }

            for (const auto& decl : program->declarations) {
                if (decl->type != npk::ASTNode::NodeType::FUNC_DECL) continue;
                auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                if (!func->body) continue;

                // Build paramRules for this function
                std::map<std::string, std::pair<npk::RulesDeclStmt*, std::string>> paramRules;

                // Collect parameter names for body scanning
                std::set<std::string> paramNameSet;
                for (auto& param : func->parameters) {
                    if (param->type == npk::ASTNode::NodeType::PARAMETER) {
                        auto* pn = static_cast<npk::ParameterNode*>(param.get());
                        paramNameSet.insert(pn->paramName);
                        if (pn->typeNode) {
                            std::string tname = pn->typeNode->toString();
                            auto it = rules_table.find(tname);
                            if (it != rules_table.end()) {
                                paramRules[pn->paramName] = {it->second, tname};
                            }
                        }
                    }
                    if (param->type == npk::ASTNode::NodeType::VAR_DECL) {
                        auto* vd = static_cast<npk::VarDeclStmt*>(param.get());
                        paramNameSet.insert(vd->varName);
                        if (!vd->limitRulesName.empty()) {
                            auto it = rules_table.find(vd->limitRulesName);
                            if (it != rules_table.end()) {
                                paramRules[vd->varName] = {it->second, vd->typeName};
                            }
                        }
                    }
                }

                // Also scan function body for limit<> VarDecls referencing parameters.
                // e.g. limit<Percentage> int32:safe_x = x; where x is a parameter
                // This maps the limit<> variable to the callee's Rules + its base type
                if (func->body && func->body->type == npk::ASTNode::NodeType::BLOCK) {
                    auto* blk = static_cast<npk::BlockStmt*>(func->body.get());
                    for (auto& s : blk->statements) {
                        if (s->type == npk::ASTNode::NodeType::VAR_DECL) {
                            auto* vd = static_cast<npk::VarDeclStmt*>(s.get());
                            if (!vd->limitRulesName.empty() && vd->initializer &&
                                vd->initializer->type == npk::ASTNode::NodeType::IDENTIFIER) {
                                auto* init = static_cast<npk::IdentifierExpr*>(vd->initializer.get());
                                if (paramNameSet.count(init->name)) {
                                    auto it = rules_table.find(vd->limitRulesName);
                                    if (it != rules_table.end()) {
                                        // Map the limit<> variable name (used in return exprs)
                                        paramRules[vd->varName] = {it->second, vd->typeName};
                                    }
                                }
                            }
                        }
                    }
                }

                if (paramRules.empty()) continue;

                int64_t inferredMin = 0, inferredMax = 0;
                std::vector<npk::VerifyOutcome> boundsOutcomes;
                z3v.inferReturnBounds(
                    func, paramRules, inferredMin, inferredMax,
                    boundsOutcomes, func->line, func->column);

                for (const auto& out : boundsOutcomes) {
                    if (out.result == npk::VerifyResult::PROVEN) {
                        if (opts.verify_report || opts.verbose) {
                            std::cout << "  [bounds] " << out.detail << "\n";
                        }
                    } else if (opts.verbose) {
                        std::cout << "  [bounds] " << out.detail << "\n";
                    }
                }
            }
        }
        
        // Phase 2d: Data race detection & Deadlock freedom (v0.5.4)
        // Walk each function for npk_thread_create calls, verify shared
        // variable accesses are properly synchronized; build lock acquisition
        // graph and verify consistent ordering.
        if (opts.verify_concurrency && program) {
            if (opts.verbose) {
                std::cout << "  Phase 2d: Concurrency verification (data race & deadlock)...\n";
            }

            // Build function lookup
            std::map<std::string, npk::FuncDeclStmt*> all_funcs_2d;
            for (const auto& decl : program->declarations) {
                if (decl->type == npk::ASTNode::NodeType::FUNC_DECL) {
                    auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                    all_funcs_2d[func->funcName] = func;
                }
            }

            // Scan for thread spawns and verify data race freedom
            for (const auto& [fname, func] : all_funcs_2d) {
                if (!func->body) continue;

                // Find npk_thread_create calls in this function
                std::vector<npk::FuncDeclStmt*> threadFuncs;
                std::function<void(npk::ASTNode*)> findSpawns;
                findSpawns = [&](npk::ASTNode* node) {
                    if (!node) return;
                    if (node->type == npk::ASTNode::NodeType::CALL) {
                        auto* call = static_cast<npk::CallExpr*>(node);
                        if (call->callee && call->callee->type == npk::ASTNode::NodeType::IDENTIFIER) {
                            auto* ident = static_cast<npk::IdentifierExpr*>(call->callee.get());
                            if ((ident->name == "npk_thread_create" ||
                                 ident->name == "npk_shim_thread_spawn" ||
                                 ident->name == "npk_libc_thread_spawn") &&
                                !call->arguments.empty()) {
                                // First argument is the thread function (may be @func or bare func)
                                npk::ASTNode* arg0 = call->arguments[0].get();
                                // Unwrap address-of operator (@func)
                                if (arg0->type == npk::ASTNode::NodeType::UNARY_OP) {
                                    auto* unary = static_cast<npk::UnaryExpr*>(arg0);
                                    if (unary->operand) arg0 = unary->operand.get();
                                }
                                if (arg0->type == npk::ASTNode::NodeType::IDENTIFIER) {
                                    auto* tfIdent = static_cast<npk::IdentifierExpr*>(arg0);
                                    auto tfIt = all_funcs_2d.find(tfIdent->name);
                                    if (tfIt != all_funcs_2d.end()) {
                                        threadFuncs.push_back(tfIt->second);
                                    }
                                }
                            }
                        }
                        for (auto& arg : call->arguments) findSpawns(arg.get());
                    } else if (node->type == npk::ASTNode::NodeType::BLOCK) {
                        auto* blk = static_cast<npk::BlockStmt*>(node);
                        for (auto& s : blk->statements) findSpawns(s.get());
                    } else if (node->type == npk::ASTNode::NodeType::IF) {
                        auto* s = static_cast<npk::IfStmt*>(node);
                        findSpawns(s->thenBranch.get());
                        findSpawns(s->elseBranch.get());
                    } else if (node->type == npk::ASTNode::NodeType::EXPRESSION_STMT) {
                        auto* s = static_cast<npk::ExpressionStmt*>(node);
                        findSpawns(s->expression.get());
                    } else if (node->type == npk::ASTNode::NodeType::VAR_DECL) {
                        auto* s = static_cast<npk::VarDeclStmt*>(node);
                        if (s->initializer) findSpawns(s->initializer.get());
                    } else if (node->type == npk::ASTNode::NodeType::WHILE) {
                        auto* s = static_cast<npk::WhileStmt*>(node);
                        findSpawns(s->body.get());
                    } else if (node->type == npk::ASTNode::NodeType::FOR) {
                        auto* s = static_cast<npk::ForStmt*>(node);
                        findSpawns(s->body.get());
                    }
                };
                findSpawns(func->body.get());

                if (!threadFuncs.empty()) {
                    std::vector<npk::VerifyOutcome> raceOutcomes;
                    z3v.verifyDataRaceFreedom(
                        func, threadFuncs, all_funcs_2d,
                        raceOutcomes, func->line, func->column);

                    for (const auto& out : raceOutcomes) {
                        if (out.result == npk::VerifyResult::DISPROVEN) {
                            std::cerr << "  [race] " << out.detail << " (line "
                                      << out.line << ")\n";
                        } else if (opts.verify_report || opts.verbose) {
                            std::cout << "  [race] " << out.detail << "\n";
                        }
                    }
                }
            }

            // Verify deadlock freedom across all functions
            std::vector<npk::VerifyOutcome> dlOutcomes;
            z3v.verifyDeadlockFreedom(all_funcs_2d, dlOutcomes, 0, 0);

            for (const auto& out : dlOutcomes) {
                if (out.result == npk::VerifyResult::DISPROVEN) {
                    std::cerr << "  [deadlock] " << out.detail << "\n";
                } else if (opts.verify_report || opts.verbose) {
                    std::cout << "  [deadlock] " << out.detail << "\n";
                }
            }
        }

        // Phase 2e: Use-after-free proofs & Recursion bounding (v0.5.4)
        // For variables with wild pointers, prove no use-after-free.
        // For recursive functions with Rules constraints, prove bounded depth.
        if (opts.verify_memory && program) {
            if (opts.verbose) {
                std::cout << "  Phase 2e: Memory safety verification (UAF & recursion bounds)...\n";
            }

            for (const auto& decl : program->declarations) {
                if (decl->type != npk::ASTNode::NodeType::FUNC_DECL) continue;
                auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                if (!func->body) continue;

                // Build paramRules for this function
                std::map<std::string, std::pair<npk::RulesDeclStmt*, std::string>> paramRules;
                std::map<std::string, npk::FuncDeclStmt*> funcMap;

                for (const auto& d : program->declarations) {
                    if (d->type == npk::ASTNode::NodeType::FUNC_DECL) {
                        auto* f = static_cast<npk::FuncDeclStmt*>(d.get());
                        funcMap[f->funcName] = f;
                    }
                }

                for (auto& param : func->parameters) {
                    if (param->type == npk::ASTNode::NodeType::PARAMETER) {
                        auto* pn = static_cast<npk::ParameterNode*>(param.get());
                        if (pn->typeNode) {
                            std::string tname = pn->typeNode->toString();
                            auto it = rules_table.find(tname);
                            if (it != rules_table.end()) {
                                paramRules[pn->paramName] = {it->second, tname};
                            }
                        }
                    }
                    if (param->type == npk::ASTNode::NodeType::VAR_DECL) {
                        auto* vd = static_cast<npk::VarDeclStmt*>(param.get());
                        if (!vd->limitRulesName.empty()) {
                            auto it = rules_table.find(vd->limitRulesName);
                            if (it != rules_table.end()) {
                                paramRules[vd->varName] = {it->second, vd->typeName};
                            }
                        }
                    }
                }

                // Also scan function body for limit<> VarDecls referencing parameters
                std::set<std::string> paramNameSet2e;
                for (auto& param : func->parameters) {
                    if (param->type == npk::ASTNode::NodeType::PARAMETER) {
                        paramNameSet2e.insert(static_cast<npk::ParameterNode*>(param.get())->paramName);
                    }
                    if (param->type == npk::ASTNode::NodeType::VAR_DECL) {
                        paramNameSet2e.insert(static_cast<npk::VarDeclStmt*>(param.get())->varName);
                    }
                }
                if (func->body && func->body->type == npk::ASTNode::NodeType::BLOCK) {
                    auto* blk = static_cast<npk::BlockStmt*>(func->body.get());
                    for (auto& s : blk->statements) {
                        if (s->type == npk::ASTNode::NodeType::VAR_DECL) {
                            auto* vd = static_cast<npk::VarDeclStmt*>(s.get());
                            if (!vd->limitRulesName.empty()) {
                                auto it = rules_table.find(vd->limitRulesName);
                                if (it != rules_table.end()) {
                                    paramRules[vd->varName] = {it->second, vd->typeName};
                                }
                            }
                        }
                    }
                }

                // Phase 18: Use-after-free — scan for wild pointers in this function
                // Look for variables that are freed and then possibly used
                std::set<std::string> freedVars;
                std::function<void(npk::ASTNode*)> findFrees;
                findFrees = [&](npk::ASTNode* node) {
                    if (!node) return;
                    if (node->type == npk::ASTNode::NodeType::CALL) {
                        auto* call = static_cast<npk::CallExpr*>(node);
                        if (call->callee && call->callee->type == npk::ASTNode::NodeType::IDENTIFIER) {
                            auto* ident = static_cast<npk::IdentifierExpr*>(call->callee.get());
                            if ((ident->name == "free" || ident->name == "npk_free" ||
                                 ident->name == "_release" || ident->name == "_destroy") &&
                                !call->arguments.empty() &&
                                call->arguments[0]->type == npk::ASTNode::NodeType::IDENTIFIER) {
                                auto* arg = static_cast<npk::IdentifierExpr*>(call->arguments[0].get());
                                freedVars.insert(arg->name);
                            }
                        }
                        for (auto& arg : call->arguments) findFrees(arg.get());
                    } else if (node->type == npk::ASTNode::NodeType::BLOCK) {
                        auto* blk = static_cast<npk::BlockStmt*>(node);
                        for (auto& s : blk->statements) findFrees(s.get());
                    } else if (node->type == npk::ASTNode::NodeType::IF) {
                        auto* s = static_cast<npk::IfStmt*>(node);
                        findFrees(s->thenBranch.get());
                        findFrees(s->elseBranch.get());
                    } else if (node->type == npk::ASTNode::NodeType::EXPRESSION_STMT) {
                        auto* s = static_cast<npk::ExpressionStmt*>(node);
                        findFrees(s->expression.get());
                    }
                };
                findFrees(func->body.get());

                for (const auto& varName : freedVars) {
                    std::vector<npk::VerifyOutcome> uafOutcomes;
                    z3v.proveNoUseAfterFree(
                        func, varName, paramRules,
                        uafOutcomes, func->line, func->column);

                    for (const auto& out : uafOutcomes) {
                        if (out.result == npk::VerifyResult::DISPROVEN) {
                            std::cerr << "  [uaf] " << out.detail << " (line "
                                      << out.line << ")\n";
                        } else if (opts.verify_report || opts.verbose) {
                            std::cout << "  [uaf] " << out.detail << "\n";
                        }
                    }
                }

                // Phase 19: Recursion bounding
                if (!paramRules.empty()) {
                    std::vector<npk::VerifyOutcome> recOutcomes;
                    z3v.proveRecursionBounded(
                        func, funcMap, paramRules,
                        recOutcomes, func->line, func->column);

                    for (const auto& out : recOutcomes) {
                        if (out.result == npk::VerifyResult::UNKNOWN) {
                            if (opts.verbose) {
                                std::cout << "  [recursion] " << out.detail << "\n";
                            }
                        } else if (opts.verify_report || opts.verbose) {
                            std::cout << "  [recursion] " << out.detail << "\n";
                        }
                    }
                }
            }
        }

        // Phase 3: Verify integer arithmetic overflow (v0.3.4)
        // v0.14.3: requires verify-level >= 2 (standard)
        if (opts.verify_overflow && program && opts.verify_level != 1) {
            if (opts.verbose) {
                std::cout << "  Phase 3: Verifying integer overflow safety...\n";
            }
            
            std::function<void(npk::ASTNode*)> walkOverflow = [&](npk::ASTNode* node) {
                if (!node) return;
                
                if (node->type == npk::ASTNode::NodeType::BINARY_OP) {
                    auto* binary = static_cast<npk::BinaryExpr*>(node);
                    char op = 0;
                    switch (binary->op.type) {
                        case npk::frontend::TokenType::TOKEN_PLUS:  op = '+'; break;
                        case npk::frontend::TokenType::TOKEN_MINUS: op = '-'; break;
                        case npk::frontend::TokenType::TOKEN_STAR:  op = '*'; break;
                        default: break;
                    }
                    
                    if (op != 0) {
                        // Check if both operands are integer literals (concrete overflow check)
                        int64_t lhsVal = 0, rhsVal = 0;
                        bool lhsConst = false, rhsConst = false;
                        int bitWidth = 32;
                        
                        auto extractInt = [](npk::ASTNode* n, int64_t& val) -> bool {
                            if (!n) return false;
                            if (n->type == npk::ASTNode::NodeType::LITERAL) {
                                auto* lit = static_cast<npk::LiteralExpr*>(n);
                                if (std::holds_alternative<int64_t>(lit->value)) {
                                    val = std::get<int64_t>(lit->value);
                                    return true;
                                }
                            } else if (n->type == npk::ASTNode::NodeType::UNARY_OP) {
                                auto* un = static_cast<npk::UnaryExpr*>(n);
                                if (un->op.type == npk::frontend::TokenType::TOKEN_MINUS &&
                                    un->operand && un->operand->type == npk::ASTNode::NodeType::LITERAL) {
                                    auto* lit = static_cast<npk::LiteralExpr*>(un->operand.get());
                                    if (std::holds_alternative<int64_t>(lit->value)) {
                                        val = -std::get<int64_t>(lit->value);
                                        return true;
                                    }
                                }
                            }
                            return false;
                        };
                        
                        lhsConst = extractInt(binary->left.get(), lhsVal);
                        rhsConst = extractInt(binary->right.get(), rhsVal);
                        
                        if (lhsConst && rhsConst) {
                            // Both constants: check with tight ranges
                            std::vector<npk::VerifyOutcome> outcomes;
                            z3v.verifyNoOverflow(op, bitWidth, true,
                                                 lhsVal, lhsVal, rhsVal, rhsVal,
                                                 outcomes, binary->line, binary->column);
                            for (const auto& out : outcomes) {
                                if (out.result == npk::VerifyResult::DISPROVEN) {
                                    diags.error(npk::SourceLocation(filename, out.line, out.column),
                                                "[z3-overflow] " + out.detail);
                                } else if (opts.verify_report && out.result == npk::VerifyResult::PROVEN) {
                                    diags.note(npk::SourceLocation(filename, out.line, out.column),
                                               "[z3-overflow] " + out.detail);
                                }
                            }
                        }
                    }
                    
                    // Recurse into sub-expressions
                    if (binary->left) walkOverflow(binary->left.get());
                    if (binary->right) walkOverflow(binary->right.get());
                    return;
                }
                
                // Generic recursion
                if (node->type == npk::ASTNode::NodeType::PROGRAM) {
                    auto* prog = static_cast<npk::ProgramNode*>(node);
                    for (const auto& decl : prog->declarations) {
                        walkOverflow(decl.get());
                    }
                } else if (node->type == npk::ASTNode::NodeType::FUNC_DECL) {
                    auto* func = static_cast<npk::FuncDeclStmt*>(node);
                    if (func->body) walkOverflow(func->body.get());
                } else if (node->type == npk::ASTNode::NodeType::BLOCK) {
                    auto* block = static_cast<npk::BlockStmt*>(node);
                    for (const auto& stmt : block->statements) {
                        walkOverflow(stmt.get());
                    }
                } else if (node->type == npk::ASTNode::NodeType::TYPE_DECL) {
                    auto* typeDecl = static_cast<npk::TypeDeclStmt*>(node);
                    if (typeDecl->createFunc) walkOverflow(typeDecl->createFunc.get());
                    if (typeDecl->destroyFunc) walkOverflow(typeDecl->destroyFunc.get());
                    for (const auto& method : typeDecl->methods) {
                        walkOverflow(method.get());
                    }
                } else if (node->type == npk::ASTNode::NodeType::EXPRESSION_STMT) {
                    auto* exprStmt = static_cast<npk::ExpressionStmt*>(node);
                    if (exprStmt->expression) walkOverflow(exprStmt->expression.get());
                } else if (node->type == npk::ASTNode::NodeType::VAR_DECL) {
                    auto* var = static_cast<npk::VarDeclStmt*>(node);
                    if (var->initializer) walkOverflow(var->initializer.get());
                } else if (node->type == npk::ASTNode::NodeType::RETURN) {
                    auto* ret = static_cast<npk::ReturnStmt*>(node);
                    if (ret->value) walkOverflow(ret->value.get());
                } else if (node->type == npk::ASTNode::NodeType::PASS) {
                    auto* pass = static_cast<npk::PassStmt*>(node);
                    if (pass->value) walkOverflow(pass->value.get());
                }
            };
            
            walkOverflow(program);
        }

        // Phase 4: SMT-guided user stack optimization (v0.4.3+)
        // Walk functions to find those with type-homogeneous apush() calls.
        // When Z3 proves all pushes use the same type, codegen uses unchecked fast variants.
        if (opts.smt_opt && program) {
            if (opts.verbose) {
                std::cout << "  Phase 4: SMT user stack optimization analysis...\n";
            }

            // Map Aria type name → ustack tag
            auto ariaTypeToTag = [](const std::string& t) -> int64_t {
                if (t == "int8")   return 0;
                if (t == "int16")  return 1;
                if (t == "int32")  return 2;
                if (t == "int" || t == "int64") return 3;
                if (t == "flt32")  return 4;
                if (t == "flt64" || t == "flt") return 5;
                if (t == "bool")   return 6;
                if (t == "string") return 7;
                return -1;  // unknown
            };

            // Per-function: collect variable types, find apush calls, determine tags
            std::function<void(npk::ASTNode*, std::map<std::string, std::string>&,
                              std::vector<int64_t>&, bool&)>
            collectPushTags = [&](npk::ASTNode* node,
                                  std::map<std::string, std::string>& varTypes,
                                  std::vector<int64_t>& tags,
                                  bool& hasAstack) {
                if (!node) return;

                // Track variable declarations
                if (node->type == npk::ASTNode::NodeType::VAR_DECL) {
                    auto* var = static_cast<npk::VarDeclStmt*>(node);
                    if (!var->typeName.empty()) {
                        varTypes[var->varName] = var->typeName;
                    }
                    if (var->initializer) collectPushTags(var->initializer.get(), varTypes, tags, hasAstack);
                    return;
                }

                // Check for astack()/apush() calls
                if (node->type == npk::ASTNode::NodeType::CALL) {
                    auto* call = static_cast<npk::CallExpr*>(node);
                    if (call->callee && call->callee->type == npk::ASTNode::NodeType::IDENTIFIER) {
                        auto* ident = static_cast<npk::IdentifierExpr*>(call->callee.get());
                        if (ident->name == "astack") {
                            hasAstack = true;
                        } else if (ident->name == "apush" && call->arguments.size() == 1) {
                            auto* arg = call->arguments[0].get();
                            int64_t tag = -1;

                            // Literal argument
                            if (arg->type == npk::ASTNode::NodeType::LITERAL) {
                                auto* lit = static_cast<npk::LiteralExpr*>(arg);
                                if (std::holds_alternative<int64_t>(lit->value)) tag = 3; // int64
                                else if (std::holds_alternative<double>(lit->value)) tag = 5; // flt64
                                else if (std::holds_alternative<bool>(lit->value)) tag = 6; // bool
                                else if (std::holds_alternative<std::string>(lit->value)) tag = 7; // string
                            }
                            // Variable reference
                            else if (arg->type == npk::ASTNode::NodeType::IDENTIFIER) {
                                auto* varRef = static_cast<npk::IdentifierExpr*>(arg);
                                auto vit = varTypes.find(varRef->name);
                                if (vit != varTypes.end()) {
                                    tag = ariaTypeToTag(vit->second);
                                }
                            }
                            // Binary expression — recursively find a typed variable
                            else if (arg->type == npk::ASTNode::NodeType::BINARY_OP) {
                                // Walk leftmost chain to find an identifier whose type we know
                                std::function<int64_t(npk::ASTNode*)> inferBinType = [&](npk::ASTNode* n) -> int64_t {
                                    if (!n) return -1;
                                    if (n->type == npk::ASTNode::NodeType::IDENTIFIER) {
                                        auto* id = static_cast<npk::IdentifierExpr*>(n);
                                        auto vit = varTypes.find(id->name);
                                        if (vit != varTypes.end()) return ariaTypeToTag(vit->second);
                                    } else if (n->type == npk::ASTNode::NodeType::LITERAL) {
                                        auto* lit = static_cast<npk::LiteralExpr*>(n);
                                        if (std::holds_alternative<int64_t>(lit->value)) return 3;
                                        if (std::holds_alternative<double>(lit->value)) return 5;
                                    } else if (n->type == npk::ASTNode::NodeType::BINARY_OP) {
                                        auto* bin = static_cast<npk::BinaryExpr*>(n);
                                        int64_t lt = inferBinType(bin->left.get());
                                        if (lt >= 0) return lt;
                                        return inferBinType(bin->right.get());
                                    }
                                    return -1;
                                };
                                tag = inferBinType(arg);
                            }

                            if (tag >= 0) {
                                tags.push_back(tag);
                            } else {
                                tags.push_back(-1); // unknown → cannot optimize
                            }
                        }
                    }
                    // Recurse into call arguments
                    for (const auto& a : call->arguments) {
                        collectPushTags(a.get(), varTypes, tags, hasAstack);
                    }
                    return;
                }

                // Recurse into containers
                if (node->type == npk::ASTNode::NodeType::BLOCK) {
                    auto* block = static_cast<npk::BlockStmt*>(node);
                    for (const auto& s : block->statements) {
                        collectPushTags(s.get(), varTypes, tags, hasAstack);
                    }
                } else if (node->type == npk::ASTNode::NodeType::EXPRESSION_STMT) {
                    auto* es = static_cast<npk::ExpressionStmt*>(node);
                    if (es->expression) collectPushTags(es->expression.get(), varTypes, tags, hasAstack);
                } else if (node->type == npk::ASTNode::NodeType::WHILE) {
                    auto* ws = static_cast<npk::WhileStmt*>(node);
                    if (ws->condition) collectPushTags(ws->condition.get(), varTypes, tags, hasAstack);
                    if (ws->body) collectPushTags(ws->body.get(), varTypes, tags, hasAstack);
                } else if (node->type == npk::ASTNode::NodeType::IF) {
                    auto* is = static_cast<npk::IfStmt*>(node);
                    if (is->condition) collectPushTags(is->condition.get(), varTypes, tags, hasAstack);
                    if (is->thenBranch) collectPushTags(is->thenBranch.get(), varTypes, tags, hasAstack);
                    if (is->elseBranch) collectPushTags(is->elseBranch.get(), varTypes, tags, hasAstack);
                } else if (node->type == npk::ASTNode::NodeType::RETURN) {
                    auto* ret = static_cast<npk::ReturnStmt*>(node);
                    if (ret->value) collectPushTags(ret->value.get(), varTypes, tags, hasAstack);
                } else if (node->type == npk::ASTNode::NodeType::FOR) {
                    auto* fs = static_cast<npk::ForStmt*>(node);
                    if (fs->initializer) collectPushTags(fs->initializer.get(), varTypes, tags, hasAstack);
                    if (fs->condition) collectPushTags(fs->condition.get(), varTypes, tags, hasAstack);
                    if (fs->update) collectPushTags(fs->update.get(), varTypes, tags, hasAstack);
                    if (fs->body) collectPushTags(fs->body.get(), varTypes, tags, hasAstack);
                }
            };

            // Walk top-level functions (including main)
            std::function<void(npk::ASTNode*)> walkUStack = [&](npk::ASTNode* node) {
                if (!node) return;
                if (node->type == npk::ASTNode::NodeType::FUNC_DECL) {
                    auto* func = static_cast<npk::FuncDeclStmt*>(node);
                    if (!func->body) return;

                    std::map<std::string, std::string> varTypes;
                    // Seed with parameter types
                    for (const auto& param : func->parameters) {
                        auto* pnode = static_cast<npk::ParameterNode*>(param.get());
                        if (pnode->typeNode) {
                            varTypes[pnode->paramName] = pnode->typeNode->toString();
                        }
                    }

                    std::vector<int64_t> pushTags;
                    bool hasAstack = false;
                    collectPushTags(func->body.get(), varTypes, pushTags, hasAstack);

                    if (hasAstack && !pushTags.empty()) {
                        // Check if all tags are known and identical
                        bool allKnown = true;
                        int64_t firstTag = pushTags[0];
                        for (int64_t t : pushTags) {
                            if (t < 0) { allKnown = false; break; }
                            if (t != firstTag) { allKnown = false; break; }
                        }

                        if (allKnown) {
                            // Ask Z3 to formally verify homogeneity
                            std::vector<npk::VerifyOutcome> outcomes;
                            npk::VerifyResult result = z3v.verifyUStackHomogeneous(
                                pushTags, firstTag, outcomes,
                                func->line, func->column);

                            if (result == npk::VerifyResult::PROVEN) {
                                ustack_opt_funcs.insert(func->funcName);
                                if (opts.verbose || opts.verify_report) {
                                    std::cout << "  [z3-smt-opt] " << func->funcName
                                              << "(): user stack proven homogeneous (tag "
                                              << firstTag << ") — fast path enabled\n";
                                }
                            }
                            for (const auto& out : outcomes) {
                                if (opts.verify_report) {
                                    diags.note(npk::SourceLocation(filename, out.line, out.column),
                                               "[z3-smt-opt] " + out.detail);
                                }
                            }
                        }
                    }
                } else if (node->type == npk::ASTNode::NodeType::PROGRAM) {
                    auto* prog = static_cast<npk::ProgramNode*>(node);
                    for (const auto& decl : prog->declarations) {
                        walkUStack(decl.get());
                    }
                } else if (node->type == npk::ASTNode::NodeType::TYPE_DECL) {
                    auto* typeDecl = static_cast<npk::TypeDeclStmt*>(node);
                    for (const auto& method : typeDecl->methods) {
                        walkUStack(method.get());
                    }
                }
            };

            walkUStack(program);

            if (opts.verbose && !ustack_opt_funcs.empty()) {
                std::cout << "  SMT optimization: " << ustack_opt_funcs.size()
                          << " function(s) eligible for fast user stack\n";
            }

            // ── User Hash (ahash) Homogeneity Analysis ──────────────────────
            // Collect value type tags from ahset() calls; if all are the same
            // type, Z3 proves homogeneity and we can use tag-free fast paths.

            std::function<void(npk::ASTNode*, std::map<std::string, std::string>&,
                              std::vector<int64_t>&, bool&)>
            collectSetTags = [&](npk::ASTNode* node,
                                 std::map<std::string, std::string>& varTypes,
                                 std::vector<int64_t>& tags,
                                 bool& hasAhash) {
                if (!node) return;

                // Track variable declarations
                if (node->type == npk::ASTNode::NodeType::VAR_DECL) {
                    auto* var = static_cast<npk::VarDeclStmt*>(node);
                    if (!var->typeName.empty()) {
                        varTypes[var->varName] = var->typeName;
                    }
                    if (var->initializer) collectSetTags(var->initializer.get(), varTypes, tags, hasAhash);
                    return;
                }

                // Check for ahash()/ahset() calls
                if (node->type == npk::ASTNode::NodeType::CALL) {
                    auto* call = static_cast<npk::CallExpr*>(node);
                    if (call->callee && call->callee->type == npk::ASTNode::NodeType::IDENTIFIER) {
                        auto* ident = static_cast<npk::IdentifierExpr*>(call->callee.get());
                        if (ident->name == "ahash") {
                            hasAhash = true;
                        } else if (ident->name == "ahset" && call->arguments.size() == 3) {
                            // ahset(handle, key, value) — value is arg index 2
                            auto* arg = call->arguments[2].get();
                            int64_t tag = -1;

                            // Literal argument
                            if (arg->type == npk::ASTNode::NodeType::LITERAL) {
                                auto* lit = static_cast<npk::LiteralExpr*>(arg);
                                if (std::holds_alternative<int64_t>(lit->value)) tag = 3; // int64
                                else if (std::holds_alternative<double>(lit->value)) tag = 5; // flt64
                                else if (std::holds_alternative<bool>(lit->value)) tag = 6; // bool
                                else if (std::holds_alternative<std::string>(lit->value)) tag = 7; // string
                            }
                            // Variable reference
                            else if (arg->type == npk::ASTNode::NodeType::IDENTIFIER) {
                                auto* varRef = static_cast<npk::IdentifierExpr*>(arg);
                                auto vit = varTypes.find(varRef->name);
                                if (vit != varTypes.end()) {
                                    tag = ariaTypeToTag(vit->second);
                                }
                            }
                            // Binary expression — recursively find a typed variable
                            else if (arg->type == npk::ASTNode::NodeType::BINARY_OP) {
                                std::function<int64_t(npk::ASTNode*)> inferBinType = [&](npk::ASTNode* n) -> int64_t {
                                    if (!n) return -1;
                                    if (n->type == npk::ASTNode::NodeType::IDENTIFIER) {
                                        auto* id = static_cast<npk::IdentifierExpr*>(n);
                                        auto vit = varTypes.find(id->name);
                                        if (vit != varTypes.end()) return ariaTypeToTag(vit->second);
                                    } else if (n->type == npk::ASTNode::NodeType::LITERAL) {
                                        auto* lit = static_cast<npk::LiteralExpr*>(n);
                                        if (std::holds_alternative<int64_t>(lit->value)) return 3;
                                        if (std::holds_alternative<double>(lit->value)) return 5;
                                    } else if (n->type == npk::ASTNode::NodeType::BINARY_OP) {
                                        auto* bin = static_cast<npk::BinaryExpr*>(n);
                                        int64_t lt = inferBinType(bin->left.get());
                                        if (lt >= 0) return lt;
                                        return inferBinType(bin->right.get());
                                    }
                                    return -1;
                                };
                                tag = inferBinType(arg);
                            }

                            if (tag >= 0) {
                                tags.push_back(tag);
                            } else {
                                tags.push_back(-1); // unknown → cannot optimize
                            }
                        }
                    }
                    // Recurse into call arguments
                    for (const auto& a : call->arguments) {
                        collectSetTags(a.get(), varTypes, tags, hasAhash);
                    }
                    return;
                }

                // Recurse into containers
                if (node->type == npk::ASTNode::NodeType::BLOCK) {
                    auto* block = static_cast<npk::BlockStmt*>(node);
                    for (const auto& s : block->statements) {
                        collectSetTags(s.get(), varTypes, tags, hasAhash);
                    }
                } else if (node->type == npk::ASTNode::NodeType::EXPRESSION_STMT) {
                    auto* es = static_cast<npk::ExpressionStmt*>(node);
                    if (es->expression) collectSetTags(es->expression.get(), varTypes, tags, hasAhash);
                } else if (node->type == npk::ASTNode::NodeType::WHILE) {
                    auto* ws = static_cast<npk::WhileStmt*>(node);
                    if (ws->condition) collectSetTags(ws->condition.get(), varTypes, tags, hasAhash);
                    if (ws->body) collectSetTags(ws->body.get(), varTypes, tags, hasAhash);
                } else if (node->type == npk::ASTNode::NodeType::IF) {
                    auto* is = static_cast<npk::IfStmt*>(node);
                    if (is->condition) collectSetTags(is->condition.get(), varTypes, tags, hasAhash);
                    if (is->thenBranch) collectSetTags(is->thenBranch.get(), varTypes, tags, hasAhash);
                    if (is->elseBranch) collectSetTags(is->elseBranch.get(), varTypes, tags, hasAhash);
                } else if (node->type == npk::ASTNode::NodeType::RETURN) {
                    auto* ret = static_cast<npk::ReturnStmt*>(node);
                    if (ret->value) collectSetTags(ret->value.get(), varTypes, tags, hasAhash);
                } else if (node->type == npk::ASTNode::NodeType::FOR) {
                    auto* fs = static_cast<npk::ForStmt*>(node);
                    if (fs->initializer) collectSetTags(fs->initializer.get(), varTypes, tags, hasAhash);
                    if (fs->condition) collectSetTags(fs->condition.get(), varTypes, tags, hasAhash);
                    if (fs->update) collectSetTags(fs->update.get(), varTypes, tags, hasAhash);
                    if (fs->body) collectSetTags(fs->body.get(), varTypes, tags, hasAhash);
                }
            };

            // Walk top-level functions for uhash homogeneity
            std::function<void(npk::ASTNode*)> walkUHash = [&](npk::ASTNode* node) {
                if (!node) return;
                if (node->type == npk::ASTNode::NodeType::FUNC_DECL) {
                    auto* func = static_cast<npk::FuncDeclStmt*>(node);
                    if (!func->body) return;

                    std::map<std::string, std::string> varTypes;
                    for (const auto& param : func->parameters) {
                        auto* pnode = static_cast<npk::ParameterNode*>(param.get());
                        if (pnode->typeNode) {
                            varTypes[pnode->paramName] = pnode->typeNode->toString();
                        }
                    }

                    std::vector<int64_t> setTags;
                    bool hasAhash = false;
                    collectSetTags(func->body.get(), varTypes, setTags, hasAhash);

                    if (hasAhash && !setTags.empty()) {
                        bool allKnown = true;
                        int64_t firstTag = setTags[0];
                        for (int64_t t : setTags) {
                            if (t < 0) { allKnown = false; break; }
                            if (t != firstTag) { allKnown = false; break; }
                        }

                        if (allKnown) {
                            std::vector<npk::VerifyOutcome> outcomes;
                            npk::VerifyResult result = z3v.verifyUHashHomogeneous(
                                setTags, firstTag, outcomes,
                                func->line, func->column);

                            if (result == npk::VerifyResult::PROVEN) {
                                uhash_opt_funcs.insert(func->funcName);
                                if (opts.verbose || opts.verify_report) {
                                    std::cout << "  [z3-smt-opt] " << func->funcName
                                              << "(): user hash proven homogeneous (tag "
                                              << firstTag << ") — fast path enabled\n";
                                }
                            }
                            for (const auto& out : outcomes) {
                                if (opts.verify_report) {
                                    diags.note(npk::SourceLocation(filename, out.line, out.column),
                                               "[z3-smt-opt] " + out.detail);
                                }
                            }
                        }
                    }
                } else if (node->type == npk::ASTNode::NodeType::PROGRAM) {
                    auto* prog = static_cast<npk::ProgramNode*>(node);
                    for (const auto& decl : prog->declarations) {
                        walkUHash(decl.get());
                    }
                } else if (node->type == npk::ASTNode::NodeType::TYPE_DECL) {
                    auto* typeDecl = static_cast<npk::TypeDeclStmt*>(node);
                    for (const auto& method : typeDecl->methods) {
                        walkUHash(method.get());
                    }
                }
            };

            walkUHash(program);

            if (opts.verbose && !uhash_opt_funcs.empty()) {
                std::cout << "  SMT optimization: " << uhash_opt_funcs.size()
                          << " function(s) eligible for fast user hash\n";
            }
        }

        // Phase 4.25: Dead Branch Elimination (Z3-powered)
        // For each IF statement whose condition involves Rules-constrained variables,
        // use Z3 to prove whether the condition is always true or always false.
        if (opts.smt_opt) {
            auto* program = dynamic_cast<npk::ProgramNode*>(module_node.get());
            if (program) {
                if (opts.verbose) {
                    std::cout << "Phase 4.25: Dead branch elimination analysis...\n";
                }

                // Collect all Rules declarations (name → RulesDeclStmt*)
                std::map<std::string, npk::RulesDeclStmt*> all_rules;
                for (const auto& decl : program->declarations) {
                    if (decl->type == npk::ASTNode::NodeType::RULES_DECL) {
                        auto* rules = static_cast<npk::RulesDeclStmt*>(decl.get());
                        all_rules[rules->rulesName] = rules;
                    }
                }

                if (!all_rules.empty() || opts.smt_opt) {
                    using VarRulesMap = std::map<std::string, std::pair<npk::RulesDeclStmt*, std::string>>;

                    // v0.14.1: Range analyzer for flow-sensitive inference (set per-function)
                    npk::RangeAnalyzer* currentRA = nullptr;

                    // Recursive walker: collect constrained var decls, probe IF conditions
                    std::function<void(npk::ASTNode*, VarRulesMap&)> walkForDeadBranches;
                    walkForDeadBranches = [&](npk::ASTNode* node, VarRulesMap& varRules) {
                        if (!node) return;
                        using NT = npk::ASTNode::NodeType;

                        switch (node->type) {
                            case NT::VAR_DECL: {
                                auto* var = static_cast<npk::VarDeclStmt*>(node);
                                if (!var->limitRulesName.empty()) {
                                    auto it = all_rules.find(var->limitRulesName);
                                    if (it != all_rules.end()) {
                                        varRules[var->varName] = {it->second, var->typeName};
                                    }
                                }
                                walkForDeadBranches(var->initializer.get(), varRules);
                                break;
                            }

                            case NT::IF: {
                                auto* ifStmt = static_cast<npk::IfStmt*>(node);
                                // v0.14.1: Merge explicit Rules + inferred ranges
                                VarRulesMap mergedRules = varRules;
                                if (currentRA) {
                                    const auto* inferred = currentRA->getRangesAt(ifStmt);
                                    if (inferred) currentRA->mergeInferred(mergedRules, *inferred);
                                }
                                if (!mergedRules.empty() && ifStmt->condition) {
                                    std::vector<npk::VerifyOutcome> outcomes;
                                    auto result = z3v.proveDeadBranch(
                                        ifStmt->condition.get(), mergedRules, outcomes,
                                        ifStmt->line, ifStmt->column);
                                    if (result == npk::VerifyResult::PROVEN) {
                                        dead_branch_true_set.insert(ifStmt);
                                    } else if (result == npk::VerifyResult::DISPROVEN) {
                                        dead_branch_false_set.insert(ifStmt);
                                    }
                                }
                                walkForDeadBranches(ifStmt->condition.get(), varRules);
                                walkForDeadBranches(ifStmt->thenBranch.get(), varRules);
                                walkForDeadBranches(ifStmt->elseBranch.get(), varRules);
                                break;
                            }

                            case NT::BLOCK: {
                                auto* block = static_cast<npk::BlockStmt*>(node);
                                for (auto& stmt : block->statements)
                                    walkForDeadBranches(stmt.get(), varRules);
                                break;
                            }

                            case NT::WHILE: {
                                auto* s = static_cast<npk::WhileStmt*>(node);
                                walkForDeadBranches(s->condition.get(), varRules);
                                walkForDeadBranches(s->body.get(), varRules);
                                break;
                            }

                            case NT::FOR: {
                                auto* s = static_cast<npk::ForStmt*>(node);
                                walkForDeadBranches(s->initializer.get(), varRules);
                                walkForDeadBranches(s->condition.get(), varRules);
                                walkForDeadBranches(s->update.get(), varRules);
                                walkForDeadBranches(s->body.get(), varRules);
                                walkForDeadBranches(s->rangeExpr.get(), varRules);
                                break;
                            }

                            case NT::LOOP: {
                                auto* s = static_cast<npk::LoopStmt*>(node);
                                walkForDeadBranches(s->start.get(), varRules);
                                walkForDeadBranches(s->limit.get(), varRules);
                                walkForDeadBranches(s->step.get(), varRules);
                                walkForDeadBranches(s->body.get(), varRules);
                                break;
                            }

                            case NT::TILL: {
                                auto* s = static_cast<npk::TillStmt*>(node);
                                walkForDeadBranches(s->limit.get(), varRules);
                                walkForDeadBranches(s->step.get(), varRules);
                                walkForDeadBranches(s->body.get(), varRules);
                                break;
                            }

                            case NT::WHEN: {
                                auto* s = static_cast<npk::WhenStmt*>(node);
                                walkForDeadBranches(s->condition.get(), varRules);
                                walkForDeadBranches(s->body.get(), varRules);
                                walkForDeadBranches(s->then_block.get(), varRules);
                                walkForDeadBranches(s->end_block.get(), varRules);
                                break;
                            }

                            case NT::PICK: {
                                auto* s = static_cast<npk::PickStmt*>(node);
                                walkForDeadBranches(s->selector.get(), varRules);
                                for (auto& c : s->cases) {
                                    auto* pc = static_cast<npk::PickCase*>(c.get());
                                    walkForDeadBranches(pc->pattern.get(), varRules);
                                    walkForDeadBranches(pc->body.get(), varRules);
                                }
                                break;
                            }

                            case NT::DEFER: {
                                auto* s = static_cast<npk::DeferStmt*>(node);
                                walkForDeadBranches(s->block.get(), varRules);
                                break;
                            }

                            case NT::EXPRESSION_STMT: {
                                auto* s = static_cast<npk::ExpressionStmt*>(node);
                                walkForDeadBranches(s->expression.get(), varRules);
                                break;
                            }

                            default:
                                break;
                        }
                    };

                    for (const auto& decl : program->declarations) {
                        if (decl->type != npk::ASTNode::NodeType::FUNC_DECL) continue;
                        auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                        if (!func->body) continue;

                        // v0.14.1: Run range analysis for flow-sensitive inference
                        npk::RangeAnalyzer ra;
                        ra.analyzeFunction(func);
                        currentRA = &ra;

                        VarRulesMap varRules;
                        walkForDeadBranches(func->body.get(), varRules);
                    }
                    currentRA = nullptr;

                    int total_dead = dead_branch_true_set.size() + dead_branch_false_set.size();
                    if (opts.verbose && total_dead > 0) {
                        std::cout << "  Dead branch elimination: " << total_dead
                                  << " branch(es) proven dead\n";
                        if (!dead_branch_true_set.empty())
                            std::cout << "    - " << dead_branch_true_set.size()
                                      << " always-true condition(s) (else branch dead)\n";
                        if (!dead_branch_false_set.empty())
                            std::cout << "    - " << dead_branch_false_set.size()
                                      << " always-false condition(s) (then branch dead)\n";
                    }
                }
            }
        }

        // Phase 4.5: Bounds Check Elimination (Z3-powered)
        // For each array index expression where the index variable has Rules constraints
        // and the array has a compile-time known size, use Z3 to prove the index is
        // always in [0, N). If proven, the runtime bounds check is skipped.
        if (opts.smt_opt) {
            auto* program = dynamic_cast<npk::ProgramNode*>(module_node.get());
            if (program) {
                if (opts.verbose) {
                    std::cout << "Phase 4.5: Bounds check elimination analysis...\n";
                }

                // Collect all Rules declarations
                std::map<std::string, npk::RulesDeclStmt*> all_rules;
                for (const auto& decl : program->declarations) {
                    if (decl->type == npk::ASTNode::NodeType::RULES_DECL) {
                        auto* rules = static_cast<npk::RulesDeclStmt*>(decl.get());
                        all_rules[rules->rulesName] = rules;
                    }
                }

                if (!all_rules.empty() || opts.smt_opt) {
                    using VarRulesMap = std::map<std::string, std::pair<npk::RulesDeclStmt*, std::string>>;

                    // Map variable names to their array sizes (for fixed-size arrays)
                    using ArraySizeMap = std::map<std::string, int64_t>;

                    // v0.14.1: Range analyzer for flow-sensitive inference (set per-function)
                    npk::RangeAnalyzer* currentRA = nullptr;

                    // Recursive walker: collect constrained vars + array decls, probe INDEX exprs
                    std::function<void(npk::ASTNode*, VarRulesMap&, ArraySizeMap&)> walkForBoundsCheck;
                    walkForBoundsCheck = [&](npk::ASTNode* node, VarRulesMap& varRules, ArraySizeMap& arraySizes) {
                        if (!node) return;
                        using NT = npk::ASTNode::NodeType;

                        switch (node->type) {
                            case NT::VAR_DECL: {
                                auto* var = static_cast<npk::VarDeclStmt*>(node);
                                // Track Rules-constrained variables
                                if (!var->limitRulesName.empty()) {
                                    auto it = all_rules.find(var->limitRulesName);
                                    if (it != all_rules.end()) {
                                        varRules[var->varName] = {it->second, var->typeName};
                                    }
                                }
                                // Track fixed-size array declarations
                                if (var->typeNode && var->typeNode->type == NT::ARRAY_TYPE) {
                                    auto* arrType = static_cast<npk::ArrayType*>(var->typeNode.get());
                                    if (!arrType->isDynamic && arrType->sizeExpr) {
                                        if (arrType->sizeExpr->type == NT::LITERAL) {
                                            auto* lit = static_cast<npk::LiteralExpr*>(arrType->sizeExpr.get());
                                            if (auto* intVal = std::get_if<int64_t>(&lit->value)) {
                                                arraySizes[var->varName] = *intVal;
                                            }
                                        }
                                    }
                                }
                                // Also detect arrays from array literal initializers
                                if (var->initializer && var->initializer->type == NT::ARRAY_LITERAL) {
                                    auto* arrLit = static_cast<npk::ArrayLiteralExpr*>(var->initializer.get());
                                    arraySizes[var->varName] = static_cast<int64_t>(arrLit->elements.size());
                                }
                                walkForBoundsCheck(var->initializer.get(), varRules, arraySizes);
                                break;
                            }

                            case NT::INDEX: {
                                auto* indexExpr = static_cast<npk::IndexExpr*>(node);
                                // Check if array is known fixed-size
                                if (indexExpr->array->type == NT::IDENTIFIER) {
                                    auto* ident = static_cast<npk::IdentifierExpr*>(indexExpr->array.get());
                                    auto sizeIt = arraySizes.find(ident->name);
                                    if (sizeIt != arraySizes.end()) {
                                        // v0.14.1: Merge explicit Rules + inferred ranges
                                        VarRulesMap mergedRules = varRules;
                                        if (currentRA) {
                                            const auto* inferred = currentRA->getRangesAt(indexExpr);
                                            if (inferred) currentRA->mergeInferred(mergedRules, *inferred);
                                        }
                                        if (!mergedRules.empty()) {
                                            std::vector<npk::VerifyOutcome> outcomes;
                                            auto result = z3v.proveBoundsInRange(
                                                indexExpr->index.get(), sizeIt->second,
                                                mergedRules, outcomes,
                                                indexExpr->line, indexExpr->column);
                                            if (result == npk::VerifyResult::PROVEN) {
                                                bounds_safe_set.insert(indexExpr);
                                            }
                                        }
                                    }
                                }
                                walkForBoundsCheck(indexExpr->array.get(), varRules, arraySizes);
                                walkForBoundsCheck(indexExpr->index.get(), varRules, arraySizes);
                                break;
                            }

                            case NT::IF: {
                                auto* ifStmt = static_cast<npk::IfStmt*>(node);
                                walkForBoundsCheck(ifStmt->condition.get(), varRules, arraySizes);
                                walkForBoundsCheck(ifStmt->thenBranch.get(), varRules, arraySizes);
                                walkForBoundsCheck(ifStmt->elseBranch.get(), varRules, arraySizes);
                                break;
                            }

                            case NT::BLOCK: {
                                auto* block = static_cast<npk::BlockStmt*>(node);
                                for (auto& stmt : block->statements)
                                    walkForBoundsCheck(stmt.get(), varRules, arraySizes);
                                break;
                            }

                            case NT::WHILE: {
                                auto* s = static_cast<npk::WhileStmt*>(node);
                                walkForBoundsCheck(s->condition.get(), varRules, arraySizes);
                                walkForBoundsCheck(s->body.get(), varRules, arraySizes);
                                break;
                            }

                            case NT::FOR: {
                                auto* s = static_cast<npk::ForStmt*>(node);
                                walkForBoundsCheck(s->initializer.get(), varRules, arraySizes);
                                walkForBoundsCheck(s->condition.get(), varRules, arraySizes);
                                walkForBoundsCheck(s->update.get(), varRules, arraySizes);
                                walkForBoundsCheck(s->body.get(), varRules, arraySizes);
                                walkForBoundsCheck(s->rangeExpr.get(), varRules, arraySizes);
                                break;
                            }

                            case NT::LOOP: {
                                auto* s = static_cast<npk::LoopStmt*>(node);
                                walkForBoundsCheck(s->start.get(), varRules, arraySizes);
                                walkForBoundsCheck(s->limit.get(), varRules, arraySizes);
                                walkForBoundsCheck(s->step.get(), varRules, arraySizes);
                                walkForBoundsCheck(s->body.get(), varRules, arraySizes);
                                break;
                            }

                            case NT::TILL: {
                                auto* s = static_cast<npk::TillStmt*>(node);
                                walkForBoundsCheck(s->limit.get(), varRules, arraySizes);
                                walkForBoundsCheck(s->step.get(), varRules, arraySizes);
                                walkForBoundsCheck(s->body.get(), varRules, arraySizes);
                                break;
                            }

                            case NT::WHEN: {
                                auto* s = static_cast<npk::WhenStmt*>(node);
                                walkForBoundsCheck(s->condition.get(), varRules, arraySizes);
                                walkForBoundsCheck(s->body.get(), varRules, arraySizes);
                                walkForBoundsCheck(s->then_block.get(), varRules, arraySizes);
                                walkForBoundsCheck(s->end_block.get(), varRules, arraySizes);
                                break;
                            }

                            case NT::PICK: {
                                auto* s = static_cast<npk::PickStmt*>(node);
                                walkForBoundsCheck(s->selector.get(), varRules, arraySizes);
                                for (auto& c : s->cases) {
                                    auto* pc = static_cast<npk::PickCase*>(c.get());
                                    walkForBoundsCheck(pc->pattern.get(), varRules, arraySizes);
                                    walkForBoundsCheck(pc->body.get(), varRules, arraySizes);
                                }
                                break;
                            }

                            case NT::DEFER: {
                                auto* s = static_cast<npk::DeferStmt*>(node);
                                walkForBoundsCheck(s->block.get(), varRules, arraySizes);
                                break;
                            }

                            case NT::EXPRESSION_STMT: {
                                auto* s = static_cast<npk::ExpressionStmt*>(node);
                                walkForBoundsCheck(s->expression.get(), varRules, arraySizes);
                                break;
                            }

                            default:
                                break;
                        }
                    };

                    for (const auto& decl : program->declarations) {
                        if (decl->type != npk::ASTNode::NodeType::FUNC_DECL) continue;
                        auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                        if (!func->body) continue;

                        // v0.14.1: Run range analysis for flow-sensitive inference
                        npk::RangeAnalyzer ra;
                        ra.analyzeFunction(func);
                        currentRA = &ra;

                        VarRulesMap varRules;
                        ArraySizeMap arraySizes;
                        walkForBoundsCheck(func->body.get(), varRules, arraySizes);
                    }
                    currentRA = nullptr;

                    if (opts.verbose && !bounds_safe_set.empty()) {
                        std::cout << "  Bounds check elimination: " << bounds_safe_set.size()
                                  << " array access(es) proven always in-bounds\n";
                    }
                }
            }
        }

        // Phase 4.75: Overflow Check Elimination (Z3-powered)
        // For each integer arithmetic operation (+, -, *) where the operands have
        // Rules constraints, use Z3 to prove no overflow is possible. If proven,
        // codegen uses plain add/sub/mul instead of generateSafeAdd/Sub/Mul.
        if (opts.smt_opt) {
            auto* program = dynamic_cast<npk::ProgramNode*>(module_node.get());
            if (program) {
                if (opts.verbose) {
                    std::cout << "Phase 4.75: Overflow check elimination analysis...\n";
                }

                // Collect all Rules declarations
                std::map<std::string, npk::RulesDeclStmt*> all_rules;
                for (const auto& decl : program->declarations) {
                    if (decl->type == npk::ASTNode::NodeType::RULES_DECL) {
                        auto* rules = static_cast<npk::RulesDeclStmt*>(decl.get());
                        all_rules[rules->rulesName] = rules;
                    }
                }

                if (!all_rules.empty() || opts.smt_opt) {
                    using VarRulesMap = std::map<std::string, std::pair<npk::RulesDeclStmt*, std::string>>;

                    // v0.14.1: Range analyzer for flow-sensitive inference (set per-function)
                    npk::RangeAnalyzer* currentRA = nullptr;

                    std::function<void(npk::ASTNode*, VarRulesMap&)> walkForOverflowCheck;
                    walkForOverflowCheck = [&](npk::ASTNode* node, VarRulesMap& varRules) {
                        if (!node) return;
                        using NT = npk::ASTNode::NodeType;

                        switch (node->type) {
                            case NT::VAR_DECL: {
                                auto* var = static_cast<npk::VarDeclStmt*>(node);
                                if (!var->limitRulesName.empty()) {
                                    auto it = all_rules.find(var->limitRulesName);
                                    if (it != all_rules.end()) {
                                        varRules[var->varName] = {it->second, var->typeName};
                                    }
                                }
                                walkForOverflowCheck(var->initializer.get(), varRules);
                                break;
                            }

                            case NT::BINARY_OP: {
                                auto* binary = static_cast<npk::BinaryExpr*>(node);
                                char op = 0;
                                switch (binary->op.type) {
                                    case npk::frontend::TokenType::TOKEN_PLUS:  op = '+'; break;
                                    case npk::frontend::TokenType::TOKEN_MINUS: op = '-'; break;
                                    case npk::frontend::TokenType::TOKEN_STAR:  op = '*'; break;
                                    default: break;
                                }
                                if (op != 0) {
                                    // v0.14.1: Merge explicit Rules + inferred ranges
                                    VarRulesMap mergedRules = varRules;
                                    if (currentRA) {
                                        const auto* inferred = currentRA->getRangesAt(binary);
                                        if (inferred) currentRA->mergeInferred(mergedRules, *inferred);
                                    }
                                    if (!mergedRules.empty()) {
                                        std::vector<npk::VerifyOutcome> outcomes;
                                        auto result = z3v.proveNoOverflowFromRules(
                                            op, binary->left.get(), binary->right.get(),
                                            mergedRules, outcomes,
                                            binary->line, binary->column);
                                        if (result == npk::VerifyResult::PROVEN) {
                                            overflow_safe_set.insert(binary);
                                        }
                                    }
                                }
                                walkForOverflowCheck(binary->left.get(), varRules);
                                walkForOverflowCheck(binary->right.get(), varRules);
                                break;
                            }

                            case NT::IF: {
                                auto* ifStmt = static_cast<npk::IfStmt*>(node);
                                walkForOverflowCheck(ifStmt->condition.get(), varRules);
                                walkForOverflowCheck(ifStmt->thenBranch.get(), varRules);
                                walkForOverflowCheck(ifStmt->elseBranch.get(), varRules);
                                break;
                            }

                            case NT::BLOCK: {
                                auto* block = static_cast<npk::BlockStmt*>(node);
                                for (auto& stmt : block->statements)
                                    walkForOverflowCheck(stmt.get(), varRules);
                                break;
                            }

                            case NT::WHILE: {
                                auto* s = static_cast<npk::WhileStmt*>(node);
                                walkForOverflowCheck(s->condition.get(), varRules);
                                walkForOverflowCheck(s->body.get(), varRules);
                                break;
                            }

                            case NT::FOR: {
                                auto* s = static_cast<npk::ForStmt*>(node);
                                walkForOverflowCheck(s->initializer.get(), varRules);
                                walkForOverflowCheck(s->condition.get(), varRules);
                                walkForOverflowCheck(s->update.get(), varRules);
                                walkForOverflowCheck(s->body.get(), varRules);
                                walkForOverflowCheck(s->rangeExpr.get(), varRules);
                                break;
                            }

                            case NT::LOOP: {
                                auto* s = static_cast<npk::LoopStmt*>(node);
                                walkForOverflowCheck(s->start.get(), varRules);
                                walkForOverflowCheck(s->limit.get(), varRules);
                                walkForOverflowCheck(s->step.get(), varRules);
                                walkForOverflowCheck(s->body.get(), varRules);
                                break;
                            }

                            case NT::TILL: {
                                auto* s = static_cast<npk::TillStmt*>(node);
                                walkForOverflowCheck(s->limit.get(), varRules);
                                walkForOverflowCheck(s->step.get(), varRules);
                                walkForOverflowCheck(s->body.get(), varRules);
                                break;
                            }

                            case NT::WHEN: {
                                auto* s = static_cast<npk::WhenStmt*>(node);
                                walkForOverflowCheck(s->condition.get(), varRules);
                                walkForOverflowCheck(s->body.get(), varRules);
                                walkForOverflowCheck(s->then_block.get(), varRules);
                                walkForOverflowCheck(s->end_block.get(), varRules);
                                break;
                            }

                            case NT::PICK: {
                                auto* s = static_cast<npk::PickStmt*>(node);
                                walkForOverflowCheck(s->selector.get(), varRules);
                                for (auto& c : s->cases) {
                                    auto* pc = static_cast<npk::PickCase*>(c.get());
                                    walkForOverflowCheck(pc->pattern.get(), varRules);
                                    walkForOverflowCheck(pc->body.get(), varRules);
                                }
                                break;
                            }

                            case NT::DEFER: {
                                auto* s = static_cast<npk::DeferStmt*>(node);
                                walkForOverflowCheck(s->block.get(), varRules);
                                break;
                            }

                            case NT::EXPRESSION_STMT: {
                                auto* s = static_cast<npk::ExpressionStmt*>(node);
                                walkForOverflowCheck(s->expression.get(), varRules);
                                break;
                            }

                            case NT::RETURN: {
                                auto* s = static_cast<npk::ReturnStmt*>(node);
                                walkForOverflowCheck(s->value.get(), varRules);
                                break;
                            }

                            case NT::PASS: {
                                auto* s = static_cast<npk::PassStmt*>(node);
                                walkForOverflowCheck(s->value.get(), varRules);
                                break;
                            }

                            default:
                                break;
                        }
                    };

                    for (const auto& decl : program->declarations) {
                        if (decl->type != npk::ASTNode::NodeType::FUNC_DECL) continue;
                        auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                        if (!func->body) continue;

                        // v0.14.1: Run range analysis for flow-sensitive inference
                        npk::RangeAnalyzer ra;
                        ra.analyzeFunction(func);
                        currentRA = &ra;

                        VarRulesMap varRules;
                        walkForOverflowCheck(func->body.get(), varRules);
                    }
                    currentRA = nullptr;

                    if (opts.verbose && !overflow_safe_set.empty()) {
                        std::cout << "  Overflow check elimination: " << overflow_safe_set.size()
                                  << " arithmetic op(s) proven overflow-free\n";
                    }
                }
            }
        }

        // Phase 4.85: Division-by-Zero Check Elimination (v0.14.4)
        // For each integer division (/) or modulo (%) operation where the divisor
        // is a Rules-constrained or range-inferred variable, prove the divisor
        // is never zero. If PROVEN, codegen can emit plain sdiv/srem instead of
        // the safe variant with the zero-check select.
        // Reuses proveNonNullFromRules (which proves expr != 0).
        if (opts.smt_opt) {
            auto* program = dynamic_cast<npk::ProgramNode*>(module_node.get());
            if (program) {
                if (opts.verbose) {
                    std::cout << "Phase 4.85: Division-by-zero check elimination analysis...\n";
                }

                std::map<std::string, npk::RulesDeclStmt*> all_rules;
                for (const auto& decl : program->declarations) {
                    if (decl->type == npk::ASTNode::NodeType::RULES_DECL) {
                        auto* rules = static_cast<npk::RulesDeclStmt*>(decl.get());
                        all_rules[rules->rulesName] = rules;
                    }
                }

                if (!all_rules.empty() || opts.smt_opt) {
                    using VarRulesMap = std::map<std::string, std::pair<npk::RulesDeclStmt*, std::string>>;

                    npk::RangeAnalyzer* currentRA = nullptr;

                    std::function<void(npk::ASTNode*, VarRulesMap&)> walkForDivCheck;
                    walkForDivCheck = [&](npk::ASTNode* node, VarRulesMap& varRules) {
                        if (!node) return;
                        using NT = npk::ASTNode::NodeType;

                        switch (node->type) {
                            case NT::VAR_DECL: {
                                auto* var = static_cast<npk::VarDeclStmt*>(node);
                                if (!var->limitRulesName.empty()) {
                                    auto it = all_rules.find(var->limitRulesName);
                                    if (it != all_rules.end()) {
                                        varRules[var->varName] = {it->second, var->typeName};
                                    }
                                }
                                walkForDivCheck(var->initializer.get(), varRules);
                                break;
                            }

                            case NT::BINARY_OP: {
                                auto* binary = static_cast<npk::BinaryExpr*>(node);
                                bool isDiv = binary->op.type == npk::frontend::TokenType::TOKEN_SLASH;
                                bool isMod = binary->op.type == npk::frontend::TokenType::TOKEN_PERCENT;
                                bool isDivEq = binary->op.type == npk::frontend::TokenType::TOKEN_SLASH_EQUAL;
                                bool isModEq = binary->op.type == npk::frontend::TokenType::TOKEN_PERCENT_EQUAL;
                                if (isDiv || isMod || isDivEq || isModEq) {
                                    // Prove the right operand (divisor) is never zero
                                    VarRulesMap mergedRules = varRules;
                                    if (currentRA) {
                                        const auto* inferred = currentRA->getRangesAt(binary);
                                        if (inferred) currentRA->mergeInferred(mergedRules, *inferred);
                                    }
                                    if (!mergedRules.empty()) {
                                        std::vector<npk::VerifyOutcome> outcomes;
                                        auto result = z3v.proveNonNullFromRules(
                                            binary->right.get(), mergedRules, outcomes,
                                            binary->line, binary->column);
                                        if (result == npk::VerifyResult::PROVEN) {
                                            div_safe_set.insert(binary);
                                        }
                                    }
                                }
                                walkForDivCheck(binary->left.get(), varRules);
                                walkForDivCheck(binary->right.get(), varRules);
                                break;
                            }

                            case NT::IF: {
                                auto* ifStmt = static_cast<npk::IfStmt*>(node);
                                walkForDivCheck(ifStmt->condition.get(), varRules);
                                walkForDivCheck(ifStmt->thenBranch.get(), varRules);
                                walkForDivCheck(ifStmt->elseBranch.get(), varRules);
                                break;
                            }

                            case NT::BLOCK: {
                                auto* block = static_cast<npk::BlockStmt*>(node);
                                for (auto& stmt : block->statements)
                                    walkForDivCheck(stmt.get(), varRules);
                                break;
                            }

                            case NT::WHILE: {
                                auto* s = static_cast<npk::WhileStmt*>(node);
                                walkForDivCheck(s->condition.get(), varRules);
                                walkForDivCheck(s->body.get(), varRules);
                                break;
                            }

                            case NT::FOR: {
                                auto* s = static_cast<npk::ForStmt*>(node);
                                walkForDivCheck(s->initializer.get(), varRules);
                                walkForDivCheck(s->condition.get(), varRules);
                                walkForDivCheck(s->update.get(), varRules);
                                walkForDivCheck(s->body.get(), varRules);
                                walkForDivCheck(s->rangeExpr.get(), varRules);
                                break;
                            }

                            case NT::LOOP: {
                                auto* s = static_cast<npk::LoopStmt*>(node);
                                walkForDivCheck(s->start.get(), varRules);
                                walkForDivCheck(s->limit.get(), varRules);
                                walkForDivCheck(s->step.get(), varRules);
                                walkForDivCheck(s->body.get(), varRules);
                                break;
                            }

                            case NT::TILL: {
                                auto* s = static_cast<npk::TillStmt*>(node);
                                walkForDivCheck(s->limit.get(), varRules);
                                walkForDivCheck(s->step.get(), varRules);
                                walkForDivCheck(s->body.get(), varRules);
                                break;
                            }

                            case NT::WHEN: {
                                auto* s = static_cast<npk::WhenStmt*>(node);
                                walkForDivCheck(s->condition.get(), varRules);
                                walkForDivCheck(s->body.get(), varRules);
                                walkForDivCheck(s->then_block.get(), varRules);
                                walkForDivCheck(s->end_block.get(), varRules);
                                break;
                            }

                            case NT::PICK: {
                                auto* s = static_cast<npk::PickStmt*>(node);
                                walkForDivCheck(s->selector.get(), varRules);
                                for (auto& c : s->cases) {
                                    auto* pc = static_cast<npk::PickCase*>(c.get());
                                    walkForDivCheck(pc->pattern.get(), varRules);
                                    walkForDivCheck(pc->body.get(), varRules);
                                }
                                break;
                            }

                            case NT::DEFER: {
                                auto* s = static_cast<npk::DeferStmt*>(node);
                                walkForDivCheck(s->block.get(), varRules);
                                break;
                            }

                            case NT::EXPRESSION_STMT: {
                                auto* s = static_cast<npk::ExpressionStmt*>(node);
                                walkForDivCheck(s->expression.get(), varRules);
                                break;
                            }

                            case NT::RETURN: {
                                auto* s = static_cast<npk::ReturnStmt*>(node);
                                walkForDivCheck(s->value.get(), varRules);
                                break;
                            }

                            case NT::PASS: {
                                auto* s = static_cast<npk::PassStmt*>(node);
                                walkForDivCheck(s->value.get(), varRules);
                                break;
                            }

                            default:
                                break;
                        }
                    };

                    for (const auto& decl : program->declarations) {
                        if (decl->type != npk::ASTNode::NodeType::FUNC_DECL) continue;
                        auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                        if (!func->body) continue;

                        npk::RangeAnalyzer ra;
                        ra.analyzeFunction(func);
                        currentRA = &ra;

                        VarRulesMap varRules;
                        walkForDivCheck(func->body.get(), varRules);
                    }
                    currentRA = nullptr;

                    if (opts.verbose && !div_safe_set.empty()) {
                        std::cout << "  Division-by-zero check elimination: " << div_safe_set.size()
                                  << " division op(s) proven safe\n";
                    }
                }
            }
        }

        // Phase 5.0: Null Check Elimination (Z3-powered + dataflow)
        // Two types of proofs:
        // 1. Z3: If Rules constraints on a variable prove it's never 0/null,
        //    ?? / ? null checks on that variable are dead.
        // 2. Dataflow: If an Optional/pointer variable is initialized from a
        //    provably non-null source (non-NIL literal, address-of, Rules-
        //    constrained variable), subsequent ?? checks are dead.
        if (opts.smt_opt) {
            auto* program = dynamic_cast<npk::ProgramNode*>(module_node.get());
            if (program) {
                if (opts.verbose) {
                    std::cout << "Phase 5.0: Null check elimination analysis...\n";
                }

                // Collect all Rules declarations
                std::map<std::string, npk::RulesDeclStmt*> all_rules;
                for (const auto& decl : program->declarations) {
                    if (decl->type == npk::ASTNode::NodeType::RULES_DECL) {
                        auto* rules = static_cast<npk::RulesDeclStmt*>(decl.get());
                        all_rules[rules->rulesName] = rules;
                    }
                }

                using VarRulesMap = std::map<std::string, std::pair<npk::RulesDeclStmt*, std::string>>;

                // v0.14.1: Range analyzer for flow-sensitive inference (set per-function)
                npk::RangeAnalyzer* currentRA = nullptr;

                // Track variables provably non-null via dataflow
                // (initialized from non-NIL literal, address-of, or Rules-constrained source)
                std::set<std::string> non_null_vars;

                // Helper: check if an expression is provably non-null
                auto isNonNullExpr = [&](npk::ASTNode* expr) -> bool {
                    if (!expr) return false;
                    using NT = npk::ASTNode::NodeType;

                    // Literal (non-NIL, non-NULL, non-unknown) → non-null
                    if (expr->type == NT::LITERAL) {
                        auto* lit = static_cast<npk::LiteralExpr*>(expr);
                        // Check it's not a zero integer literal
                        if (std::holds_alternative<int64_t>(lit->value)) {
                            return std::get<int64_t>(lit->value) != 0;
                        }
                        // Non-zero float, string, bool → non-null
                        return true;
                    }

                    // NIL/NULL identifier → null
                    if (expr->type == NT::IDENTIFIER) {
                        auto* ident = static_cast<npk::IdentifierExpr*>(expr);
                        if (ident->name == "NIL" || ident->name == "NULL"
                            || ident->name == "unknown") return false;
                        // Variable previously proven non-null → non-null
                        if (non_null_vars.count(ident->name)) return true;
                        return false;
                    }

                    // Address-of (@) always produces non-null pointer
                    if (expr->type == NT::UNARY_OP) {
                        auto* unary = static_cast<npk::UnaryExpr*>(expr);
                        if (unary->op.type == npk::frontend::TokenType::TOKEN_AT) {
                            return true;  // @ always non-null
                        }
                    }

                    // Function call → unknown (could return error/None)
                    return false;
                };

                std::function<void(npk::ASTNode*, VarRulesMap&)> walkForNullCheck;
                walkForNullCheck = [&](npk::ASTNode* node, VarRulesMap& varRules) {
                    if (!node) return;
                    using NT = npk::ASTNode::NodeType;

                    switch (node->type) {
                        case NT::VAR_DECL: {
                            auto* var = static_cast<npk::VarDeclStmt*>(node);
                            // Track Rules-constrained variables (for Z3 proofs)
                            if (!var->limitRulesName.empty()) {
                                auto it = all_rules.find(var->limitRulesName);
                                if (it != all_rules.end()) {
                                    varRules[var->varName] = {it->second, var->typeName};
                                    non_null_vars.insert(var->varName);
                                }
                            }
                            // Track variables initialized from provably non-null expressions
                            if (var->initializer && isNonNullExpr(var->initializer.get())) {
                                non_null_vars.insert(var->varName);
                            }
                            // Track Optional/pointer variables from @ or non-NIL
                            if (var->initializer) {
                                bool isOptOrPtr = (var->typeName.back() == '?' ||
                                                   (var->typeName.size() > 1 &&
                                                    var->typeName.substr(var->typeName.size()-2) == "->"));
                                if (isOptOrPtr && isNonNullExpr(var->initializer.get())) {
                                    non_null_vars.insert(var->varName);
                                }
                            }
                            walkForNullCheck(var->initializer.get(), varRules);
                            break;
                        }

                        case NT::UNWRAP: {
                            auto* unwrap = static_cast<npk::UnwrapExpr*>(node);

                            // Dataflow proof: if the result expression is a known non-null variable
                            if (unwrap->result && unwrap->result->type == NT::IDENTIFIER) {
                                auto* ident = static_cast<npk::IdentifierExpr*>(unwrap->result.get());
                                if (non_null_vars.count(ident->name)) {
                                    null_check_safe_set.insert(node);
                                    walkForNullCheck(unwrap->result.get(), varRules);
                                    walkForNullCheck(unwrap->defaultValue.get(), varRules);
                                    break;
                                }
                            }

                            // Dataflow proof: if result is address-of (@), always non-null
                            if (unwrap->result && unwrap->result->type == NT::UNARY_OP) {
                                auto* unary = static_cast<npk::UnaryExpr*>(unwrap->result.get());
                                if (unary->op.type == npk::frontend::TokenType::TOKEN_AT) {
                                    null_check_safe_set.insert(node);
                                    walkForNullCheck(unwrap->result.get(), varRules);
                                    walkForNullCheck(unwrap->defaultValue.get(), varRules);
                                    break;
                                }
                            }

                            // Z3 proof: if source expression involves Rules-constrained variables
                            if (unwrap->result) {
                                // v0.14.1: Merge explicit Rules + inferred ranges
                                VarRulesMap mergedRules = varRules;
                                if (currentRA) {
                                    const auto* inferred = currentRA->getRangesAt(node);
                                    if (inferred) currentRA->mergeInferred(mergedRules, *inferred);
                                }
                                if (!mergedRules.empty()) {
                                    std::vector<npk::VerifyOutcome> outcomes;
                                    auto result = z3v.proveNonNullFromRules(
                                        unwrap->result.get(),
                                        mergedRules, outcomes,
                                        unwrap->line, unwrap->column);
                                    if (result == npk::VerifyResult::PROVEN) {
                                        null_check_safe_set.insert(node);
                                    }
                                }
                            }
                            walkForNullCheck(unwrap->result.get(), varRules);
                            walkForNullCheck(unwrap->defaultValue.get(), varRules);
                            break;
                        }

                        case NT::BINARY_OP: {
                            auto* binary = static_cast<npk::BinaryExpr*>(node);
                            // Check NULL_COALESCE binary op (?? token)
                            if (binary->op.type == npk::frontend::TokenType::TOKEN_NULL_COALESCE) {
                                // Dataflow proof: known non-null variable
                                if (binary->left && binary->left->type == NT::IDENTIFIER) {
                                    auto* ident = static_cast<npk::IdentifierExpr*>(binary->left.get());
                                    if (non_null_vars.count(ident->name)) {
                                        null_check_safe_set.insert(node);
                                    }
                                }
                                // Dataflow proof: address-of (@)
                                else if (binary->left && binary->left->type == NT::UNARY_OP) {
                                    auto* unary = static_cast<npk::UnaryExpr*>(binary->left.get());
                                    if (unary->op.type == npk::frontend::TokenType::TOKEN_AT) {
                                        null_check_safe_set.insert(node);
                                    }
                                }
                                // Z3 proof (v0.14.1: with inferred ranges)
                                else {
                                    VarRulesMap mergedRules = varRules;
                                    if (currentRA) {
                                        const auto* inferred = currentRA->getRangesAt(node);
                                        if (inferred) currentRA->mergeInferred(mergedRules, *inferred);
                                    }
                                    if (!mergedRules.empty()) {
                                        std::vector<npk::VerifyOutcome> outcomes;
                                        auto result = z3v.proveNonNullFromRules(
                                            binary->left.get(),
                                            mergedRules, outcomes,
                                            binary->line, binary->column);
                                        if (result == npk::VerifyResult::PROVEN) {
                                            null_check_safe_set.insert(node);
                                        }
                                    }
                                }
                            }
                            walkForNullCheck(binary->left.get(), varRules);
                            walkForNullCheck(binary->right.get(), varRules);
                            break;
                        }

                        case NT::IF: {
                            auto* ifStmt = static_cast<npk::IfStmt*>(node);
                            walkForNullCheck(ifStmt->condition.get(), varRules);
                            walkForNullCheck(ifStmt->thenBranch.get(), varRules);
                            walkForNullCheck(ifStmt->elseBranch.get(), varRules);
                            break;
                        }

                        case NT::BLOCK: {
                            auto* block = static_cast<npk::BlockStmt*>(node);
                            for (auto& stmt : block->statements)
                                walkForNullCheck(stmt.get(), varRules);
                            break;
                        }

                        case NT::WHILE: {
                            auto* s = static_cast<npk::WhileStmt*>(node);
                            walkForNullCheck(s->condition.get(), varRules);
                            walkForNullCheck(s->body.get(), varRules);
                            break;
                        }

                        case NT::FOR: {
                            auto* s = static_cast<npk::ForStmt*>(node);
                            walkForNullCheck(s->initializer.get(), varRules);
                            walkForNullCheck(s->condition.get(), varRules);
                            walkForNullCheck(s->update.get(), varRules);
                            walkForNullCheck(s->body.get(), varRules);
                            walkForNullCheck(s->rangeExpr.get(), varRules);
                            break;
                        }

                        case NT::LOOP: {
                            auto* s = static_cast<npk::LoopStmt*>(node);
                            walkForNullCheck(s->start.get(), varRules);
                            walkForNullCheck(s->limit.get(), varRules);
                            walkForNullCheck(s->step.get(), varRules);
                            walkForNullCheck(s->body.get(), varRules);
                            break;
                        }

                        case NT::TILL: {
                            auto* s = static_cast<npk::TillStmt*>(node);
                            walkForNullCheck(s->limit.get(), varRules);
                            walkForNullCheck(s->step.get(), varRules);
                            walkForNullCheck(s->body.get(), varRules);
                            break;
                        }

                        case NT::WHEN: {
                            auto* s = static_cast<npk::WhenStmt*>(node);
                            walkForNullCheck(s->condition.get(), varRules);
                            walkForNullCheck(s->body.get(), varRules);
                            walkForNullCheck(s->then_block.get(), varRules);
                            walkForNullCheck(s->end_block.get(), varRules);
                            break;
                        }

                        case NT::PICK: {
                            auto* s = static_cast<npk::PickStmt*>(node);
                            walkForNullCheck(s->selector.get(), varRules);
                            for (auto& c : s->cases) {
                                auto* pc = static_cast<npk::PickCase*>(c.get());
                                walkForNullCheck(pc->pattern.get(), varRules);
                                walkForNullCheck(pc->body.get(), varRules);
                            }
                            break;
                        }

                        case NT::DEFER: {
                            auto* s = static_cast<npk::DeferStmt*>(node);
                            walkForNullCheck(s->block.get(), varRules);
                            break;
                        }

                        case NT::EXPRESSION_STMT: {
                            auto* s = static_cast<npk::ExpressionStmt*>(node);
                            walkForNullCheck(s->expression.get(), varRules);
                            break;
                        }

                        case NT::RETURN: {
                            auto* s = static_cast<npk::ReturnStmt*>(node);
                            walkForNullCheck(s->value.get(), varRules);
                            break;
                        }

                        case NT::PASS: {
                            auto* s = static_cast<npk::PassStmt*>(node);
                            walkForNullCheck(s->value.get(), varRules);
                            break;
                        }

                        default:
                            break;
                    }
                };

                for (const auto& decl : program->declarations) {
                    if (decl->type != npk::ASTNode::NodeType::FUNC_DECL) continue;
                    auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                    if (!func->body) continue;

                    // v0.14.1: Run range analysis for flow-sensitive inference
                    npk::RangeAnalyzer ra;
                    ra.analyzeFunction(func);
                    currentRA = &ra;

                    VarRulesMap varRules;
                    non_null_vars.clear();  // Reset per function
                    walkForNullCheck(func->body.get(), varRules);
                }
                currentRA = nullptr;

                if (opts.verbose && !null_check_safe_set.empty()) {
                    std::cout << "  Null check elimination: " << null_check_safe_set.size()
                              << " null check(s) proven unnecessary\n";
                }
            }
        }

        // Phase 5.25: Loop invariant hoisting
        // Identify VarDecl statements inside loops whose initializers don't reference
        // any variable modified within the loop body or the loop counter.
        // These are hoisted before the loop entry in codegen.
        if (opts.smt_opt) {
            if (opts.verbose) {
                std::cout << "  Phase 5.25: Loop invariant hoisting analysis...\n";
            }

            // Helper: collect all variables modified (assigned to) inside a subtree
            std::function<void(npk::ASTNode*, std::set<std::string>&)> collectModifiedVars;
            collectModifiedVars = [&](npk::ASTNode* node, std::set<std::string>& modified) {
                if (!node) return;
                using NT = npk::ASTNode::NodeType;
                switch (node->type) {
                    case NT::ASSIGNMENT: {
                        auto* assign = static_cast<npk::AssignmentExpr*>(node);
                        if (assign->target && assign->target->type == NT::IDENTIFIER) {
                            auto* ident = static_cast<npk::IdentifierExpr*>(assign->target.get());
                            modified.insert(ident->name);
                        }
                        collectModifiedVars(assign->value.get(), modified);
                        break;
                    }
                    case NT::BLOCK: {
                        auto* block = static_cast<npk::BlockStmt*>(node);
                        for (auto& s : block->statements)
                            collectModifiedVars(s.get(), modified);
                        break;
                    }
                    case NT::EXPRESSION_STMT: {
                        auto* s = static_cast<npk::ExpressionStmt*>(node);
                        collectModifiedVars(s->expression.get(), modified);
                        break;
                    }
                    case NT::IF: {
                        auto* s = static_cast<npk::IfStmt*>(node);
                        collectModifiedVars(s->condition.get(), modified);
                        collectModifiedVars(s->thenBranch.get(), modified);
                        collectModifiedVars(s->elseBranch.get(), modified);
                        break;
                    }
                    case NT::VAR_DECL: {
                        auto* var = static_cast<npk::VarDeclStmt*>(node);
                        collectModifiedVars(var->initializer.get(), modified);
                        break;
                    }
                    case NT::WHILE: {
                        auto* s = static_cast<npk::WhileStmt*>(node);
                        collectModifiedVars(s->condition.get(), modified);
                        collectModifiedVars(s->body.get(), modified);
                        break;
                    }
                    case NT::FOR: {
                        auto* s = static_cast<npk::ForStmt*>(node);
                        collectModifiedVars(s->initializer.get(), modified);
                        collectModifiedVars(s->condition.get(), modified);
                        collectModifiedVars(s->update.get(), modified);
                        collectModifiedVars(s->body.get(), modified);
                        collectModifiedVars(s->rangeExpr.get(), modified);
                        break;
                    }
                    case NT::LOOP: {
                        auto* s = static_cast<npk::LoopStmt*>(node);
                        collectModifiedVars(s->start.get(), modified);
                        collectModifiedVars(s->limit.get(), modified);
                        collectModifiedVars(s->step.get(), modified);
                        collectModifiedVars(s->body.get(), modified);
                        break;
                    }
                    case NT::TILL: {
                        auto* s = static_cast<npk::TillStmt*>(node);
                        collectModifiedVars(s->limit.get(), modified);
                        collectModifiedVars(s->step.get(), modified);
                        collectModifiedVars(s->body.get(), modified);
                        break;
                    }
                    case NT::WHEN: {
                        auto* s = static_cast<npk::WhenStmt*>(node);
                        collectModifiedVars(s->condition.get(), modified);
                        collectModifiedVars(s->body.get(), modified);
                        collectModifiedVars(s->then_block.get(), modified);
                        collectModifiedVars(s->end_block.get(), modified);
                        break;
                    }
                    case NT::BINARY_OP: {
                        auto* s = static_cast<npk::BinaryExpr*>(node);
                        collectModifiedVars(s->left.get(), modified);
                        collectModifiedVars(s->right.get(), modified);
                        break;
                    }
                    case NT::UNARY_OP: {
                        auto* s = static_cast<npk::UnaryExpr*>(node);
                        collectModifiedVars(s->operand.get(), modified);
                        break;
                    }
                    case NT::CALL: {
                        auto* s = static_cast<npk::CallExpr*>(node);
                        collectModifiedVars(s->callee.get(), modified);
                        for (auto& arg : s->arguments)
                            collectModifiedVars(arg.get(), modified);
                        break;
                    }
                    case NT::RETURN: {
                        auto* s = static_cast<npk::ReturnStmt*>(node);
                        collectModifiedVars(s->value.get(), modified);
                        break;
                    }
                    case NT::PASS: {
                        auto* s = static_cast<npk::PassStmt*>(node);
                        collectModifiedVars(s->value.get(), modified);
                        break;
                    }
                    default:
                        break;
                }
            };

            // Helper: check if an expression is loop-invariant
            // (all referenced identifiers are NOT in the modified set)
            std::function<bool(npk::ASTNode*, const std::set<std::string>&)> isLoopInvariantExpr;
            isLoopInvariantExpr = [&](npk::ASTNode* expr,
                                      const std::set<std::string>& modified) -> bool {
                if (!expr) return true;
                using NT = npk::ASTNode::NodeType;
                switch (expr->type) {
                    case NT::LITERAL: return true;
                    case NT::IDENTIFIER: {
                        auto* ident = static_cast<npk::IdentifierExpr*>(expr);
                        return modified.count(ident->name) == 0;
                    }
                    case NT::BINARY_OP: {
                        auto* bin = static_cast<npk::BinaryExpr*>(expr);
                        return isLoopInvariantExpr(bin->left.get(), modified) &&
                               isLoopInvariantExpr(bin->right.get(), modified);
                    }
                    case NT::UNARY_OP: {
                        auto* un = static_cast<npk::UnaryExpr*>(expr);
                        return isLoopInvariantExpr(un->operand.get(), modified);
                    }
                    case NT::UNWRAP: {
                        auto* uw = static_cast<npk::UnwrapExpr*>(expr);
                        return isLoopInvariantExpr(uw->result.get(), modified) &&
                               isLoopInvariantExpr(uw->defaultValue.get(), modified);
                    }
                    // Conservative: calls, member access, indexing etc. are NOT invariant
                    default: return false;
                }
            };

            // Process a loop: collect modified vars, find hoistable VarDecls
            auto processLoop = [&](npk::ASTNode* loopNode, npk::ASTNode* body,
                                   const std::set<std::string>& extraModified) {
                // Collect variables modified inside the loop body
                std::set<std::string> modified = extraModified;
                collectModifiedVars(body, modified);

                // Walk direct body statements to find hoistable VarDecls
                if (body && body->type == npk::ASTNode::NodeType::BLOCK) {
                    auto* block = static_cast<npk::BlockStmt*>(body);
                    for (auto& stmt : block->statements) {
                        if (stmt->type == npk::ASTNode::NodeType::VAR_DECL) {
                            auto* var = static_cast<npk::VarDeclStmt*>(stmt.get());
                            if (var->initializer &&
                                isLoopInvariantExpr(var->initializer.get(), modified)) {
                                loop_hoist_map[loopNode].push_back(stmt.get());
                            }
                        }
                    }
                }
            };

            // Walk function bodies to find loops
            std::function<void(npk::ASTNode*)> walkForLoopHoist;
            walkForLoopHoist = [&](npk::ASTNode* node) {
                if (!node) return;
                using NT = npk::ASTNode::NodeType;

                switch (node->type) {
                    case NT::WHILE: {
                        auto* s = static_cast<npk::WhileStmt*>(node);
                        std::set<std::string> extra;
                        processLoop(node, s->body.get(), extra);
                        walkForLoopHoist(s->body.get());
                        break;
                    }
                    case NT::FOR: {
                        auto* s = static_cast<npk::ForStmt*>(node);
                        std::set<std::string> extra;
                        if (s->isRangeBased) {
                            extra.insert(s->iteratorName);
                        }
                        processLoop(node, s->body.get(), extra);
                        walkForLoopHoist(s->body.get());
                        break;
                    }
                    case NT::LOOP: {
                        auto* s = static_cast<npk::LoopStmt*>(node);
                        std::set<std::string> extra;
                        extra.insert("$");
                        processLoop(node, s->body.get(), extra);
                        walkForLoopHoist(s->body.get());
                        break;
                    }
                    case NT::TILL: {
                        auto* s = static_cast<npk::TillStmt*>(node);
                        std::set<std::string> extra;
                        extra.insert("$");
                        processLoop(node, s->body.get(), extra);
                        walkForLoopHoist(s->body.get());
                        break;
                    }
                    case NT::WHEN: {
                        auto* s = static_cast<npk::WhenStmt*>(node);
                        std::set<std::string> extra;
                        processLoop(node, s->body.get(), extra);
                        walkForLoopHoist(s->body.get());
                        break;
                    }
                    case NT::BLOCK: {
                        auto* block = static_cast<npk::BlockStmt*>(node);
                        for (auto& s : block->statements)
                            walkForLoopHoist(s.get());
                        break;
                    }
                    case NT::IF: {
                        auto* s = static_cast<npk::IfStmt*>(node);
                        walkForLoopHoist(s->thenBranch.get());
                        walkForLoopHoist(s->elseBranch.get());
                        break;
                    }
                    case NT::DEFER: {
                        auto* s = static_cast<npk::DeferStmt*>(node);
                        walkForLoopHoist(s->block.get());
                        break;
                    }
                    case NT::PICK: {
                        auto* s = static_cast<npk::PickStmt*>(node);
                        for (auto& c : s->cases) {
                            auto* pc = static_cast<npk::PickCase*>(c.get());
                            walkForLoopHoist(pc->body.get());
                        }
                        break;
                    }
                    default:
                        break;
                }
            };

            for (const auto& decl : program->declarations) {
                if (decl->type != npk::ASTNode::NodeType::FUNC_DECL) continue;
                auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                if (!func->body) continue;
                walkForLoopHoist(func->body.get());
            }

            size_t total_hoisted = 0;
            for (const auto& [loop, decls] : loop_hoist_map)
                total_hoisted += decls.size();

            if (opts.verbose && total_hoisted > 0) {
                std::cout << "  Loop invariant hoisting: " << total_hoisted
                          << " declaration(s) hoisted out of loops\n";
            }
        }

        // Phase 5.5: Rules<T> Propagation (inter-procedural limit check elimination)
        // If a non-pub function has `limit<R> Type:var = param` and ALL callers
        // pass arguments already constrained by Rules that subsume R, the limit
        // check inside the callee is proven redundant.
        if (opts.smt_opt) {
            auto* program = dynamic_cast<npk::ProgramNode*>(module_node.get());
            if (program) {
                if (opts.verbose) {
                    std::cout << "Phase 5.5: Rules<T> propagation analysis...\n";
                }

                // Collect all Rules declarations
                std::map<std::string, npk::RulesDeclStmt*> all_rules;
                for (const auto& decl : program->declarations) {
                    if (decl->type == npk::ASTNode::NodeType::RULES_DECL) {
                        auto* rules = static_cast<npk::RulesDeclStmt*>(decl.get());
                        all_rules[rules->rulesName] = rules;
                    }
                }

                // Collect all function declarations
                std::map<std::string, npk::FuncDeclStmt*> all_funcs;
                for (const auto& decl : program->declarations) {
                    if (decl->type == npk::ASTNode::NodeType::FUNC_DECL) {
                        auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                        all_funcs[func->funcName] = func;
                    }
                }

                // Callee requirement: a limit<R> check on a parameter inside a function
                struct CalleeReq {
                    size_t paramIndex;
                    std::string rulesName;
                    std::string typeName;
                    npk::VarDeclStmt* varDecl;
                };

                // funcName → vector of requirements (limit checks tied to parameters)
                std::map<std::string, std::vector<CalleeReq>> callee_reqs;

                // Build callee requirements: scan non-pub function bodies for
                // limit<R> Type:var = paramName patterns
                for (const auto& [funcName, func] : all_funcs) {
                    if (func->isPublic || func->isExtern || !func->body) continue;

                    // Map parameter names → indices
                    std::map<std::string, size_t> paramNameToIndex;
                    for (size_t i = 0; i < func->parameters.size(); ++i) {
                        auto* param = static_cast<npk::ParameterNode*>(
                            func->parameters[i].get());
                        paramNameToIndex[param->paramName] = i;
                    }

                    // Walk top-level statements in function body
                    auto* block = dynamic_cast<npk::BlockStmt*>(func->body.get());
                    if (!block) continue;

                    for (const auto& stmt : block->statements) {
                        if (stmt->type != npk::ASTNode::NodeType::VAR_DECL) continue;
                        auto* var = static_cast<npk::VarDeclStmt*>(stmt.get());
                        if (var->limitRulesName.empty()) continue;
                        if (!var->initializer) continue;

                        // Check if initializer is a bare identifier referencing a parameter
                        if (var->initializer->type != npk::ASTNode::NodeType::IDENTIFIER)
                            continue;
                        auto* ident = static_cast<npk::IdentifierExpr*>(
                            var->initializer.get());
                        auto pIt = paramNameToIndex.find(ident->name);
                        if (pIt == paramNameToIndex.end()) continue;

                        CalleeReq req;
                        req.paramIndex = pIt->second;
                        req.rulesName = var->limitRulesName;
                        req.typeName = var->typeName;
                        req.varDecl = var;
                        callee_reqs[funcName].push_back(req);
                    }
                }

                if (!callee_reqs.empty()) {
                    // Track constraints: variable name → Rules name
                    using VarConstraintMap = std::map<std::string, std::string>;

                    // Info about a call site's argument constraints
                    struct CallSiteInfo {
                        std::map<size_t, std::string> argConstraints;
                        int line;
                        int column;
                    };

                    // callee funcName → all observed call sites
                    std::map<std::string, std::vector<CallSiteInfo>> call_sites;

                    // Recursive walker to find call sites and track variable constraints
                    std::function<void(npk::ASTNode*, VarConstraintMap&)> findCallSites;
                    findCallSites = [&](npk::ASTNode* node, VarConstraintMap& varConstraints) {
                        if (!node) return;
                        using NT = npk::ASTNode::NodeType;

                        switch (node->type) {
                            case NT::VAR_DECL: {
                                auto* var = static_cast<npk::VarDeclStmt*>(node);
                                if (!var->limitRulesName.empty()) {
                                    varConstraints[var->varName] = var->limitRulesName;
                                }
                                findCallSites(var->initializer.get(), varConstraints);
                                break;
                            }

                            case NT::CALL: {
                                auto* call = static_cast<npk::CallExpr*>(node);
                                if (call->callee && call->callee->type == NT::IDENTIFIER) {
                                    auto* callee = static_cast<npk::IdentifierExpr*>(
                                        call->callee.get());
                                    if (callee_reqs.count(callee->name)) {
                                        CallSiteInfo info;
                                        info.line = call->line;
                                        info.column = call->column;
                                        for (size_t i = 0; i < call->arguments.size(); ++i) {
                                            if (call->arguments[i]->type == NT::IDENTIFIER) {
                                                auto* argIdent = static_cast<npk::IdentifierExpr*>(
                                                    call->arguments[i].get());
                                                auto cIt = varConstraints.find(argIdent->name);
                                                if (cIt != varConstraints.end()) {
                                                    info.argConstraints[i] = cIt->second;
                                                }
                                            }
                                        }
                                        call_sites[callee->name].push_back(info);
                                    }
                                }
                                for (auto& arg : call->arguments)
                                    findCallSites(arg.get(), varConstraints);
                                break;
                            }

                            case NT::BLOCK: {
                                auto* b = static_cast<npk::BlockStmt*>(node);
                                for (auto& s : b->statements)
                                    findCallSites(s.get(), varConstraints);
                                break;
                            }

                            case NT::EXPRESSION_STMT: {
                                auto* s = static_cast<npk::ExpressionStmt*>(node);
                                findCallSites(s->expression.get(), varConstraints);
                                break;
                            }

                            case NT::IF: {
                                auto* s = static_cast<npk::IfStmt*>(node);
                                findCallSites(s->condition.get(), varConstraints);
                                findCallSites(s->thenBranch.get(), varConstraints);
                                findCallSites(s->elseBranch.get(), varConstraints);
                                break;
                            }

                            case NT::WHILE: {
                                auto* s = static_cast<npk::WhileStmt*>(node);
                                findCallSites(s->condition.get(), varConstraints);
                                findCallSites(s->body.get(), varConstraints);
                                break;
                            }

                            case NT::FOR: {
                                auto* s = static_cast<npk::ForStmt*>(node);
                                findCallSites(s->initializer.get(), varConstraints);
                                findCallSites(s->condition.get(), varConstraints);
                                findCallSites(s->update.get(), varConstraints);
                                findCallSites(s->body.get(), varConstraints);
                                break;
                            }

                            case NT::LOOP: {
                                auto* s = static_cast<npk::LoopStmt*>(node);
                                findCallSites(s->start.get(), varConstraints);
                                findCallSites(s->limit.get(), varConstraints);
                                findCallSites(s->step.get(), varConstraints);
                                findCallSites(s->body.get(), varConstraints);
                                break;
                            }

                            case NT::TILL: {
                                auto* s = static_cast<npk::TillStmt*>(node);
                                findCallSites(s->limit.get(), varConstraints);
                                findCallSites(s->step.get(), varConstraints);
                                findCallSites(s->body.get(), varConstraints);
                                break;
                            }

                            case NT::WHEN: {
                                auto* s = static_cast<npk::WhenStmt*>(node);
                                findCallSites(s->condition.get(), varConstraints);
                                findCallSites(s->body.get(), varConstraints);
                                findCallSites(s->then_block.get(), varConstraints);
                                findCallSites(s->end_block.get(), varConstraints);
                                break;
                            }

                            case NT::PICK: {
                                auto* s = static_cast<npk::PickStmt*>(node);
                                findCallSites(s->selector.get(), varConstraints);
                                for (auto& c : s->cases) {
                                    auto* pc = static_cast<npk::PickCase*>(c.get());
                                    findCallSites(pc->pattern.get(), varConstraints);
                                    findCallSites(pc->body.get(), varConstraints);
                                }
                                break;
                            }

                            case NT::DEFER: {
                                auto* s = static_cast<npk::DeferStmt*>(node);
                                findCallSites(s->block.get(), varConstraints);
                                break;
                            }

                            case NT::RETURN: {
                                auto* s = static_cast<npk::ReturnStmt*>(node);
                                findCallSites(s->value.get(), varConstraints);
                                break;
                            }

                            case NT::PASS: {
                                auto* s = static_cast<npk::PassStmt*>(node);
                                findCallSites(s->value.get(), varConstraints);
                                break;
                            }

                            default:
                                break;
                        }
                    };

                    // Walk every function body to collect call sites
                    for (const auto& decl : program->declarations) {
                        if (decl->type != npk::ASTNode::NodeType::FUNC_DECL) continue;
                        auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                        if (!func->body) continue;
                        VarConstraintMap varConstraints;
                        findCallSites(func->body.get(), varConstraints);
                    }

                    // For each callee requirement, check if ALL call sites satisfy it
                    for (const auto& [funcName, reqs] : callee_reqs) {
                        auto csIt = call_sites.find(funcName);
                        if (csIt == call_sites.end()) continue;
                        const auto& sites = csIt->second;
                        if (sites.empty()) continue;

                        for (const auto& req : reqs) {
                            bool allSatisfied = true;
                            for (const auto& site : sites) {
                                auto argIt = site.argConstraints.find(req.paramIndex);
                                if (argIt == site.argConstraints.end()) {
                                    allSatisfied = false;
                                    break;
                                }

                                const std::string& callerRules = argIt->second;
                                if (callerRules == req.rulesName) continue;

                                // Different Rules names — Z3 subsumption check
                                std::vector<npk::VerifyOutcome> outcomes;
                                auto result = z3v.proveRulesSubsumption(
                                    callerRules, req.rulesName, req.typeName,
                                    outcomes, site.line, site.column);
                                if (result != npk::VerifyResult::PROVEN) {
                                    allSatisfied = false;
                                    break;
                                }
                            }

                            if (allSatisfied) {
                                limit_check_safe_set.insert(req.varDecl);
                            }
                        }
                    }
                }

                if (opts.verbose && !limit_check_safe_set.empty()) {
                    std::cout << "  Rules propagation: " << limit_check_safe_set.size()
                              << " limit check(s) proven redundant via caller constraints\n";
                }
            }
        }

        // Phase 6: User assertions — prove(expr) and assert_static(expr)
        // Walk AST to find ProveStmt and AssertStaticStmt nodes, collect
        // local Rules constraints, and verify with Z3.
        int assert_static_soft_disproven = 0;  // don't count toward abort
        {
            std::set<npk::ASTNode*> assert_static_proven_set;
            bool prove_failed = false;

            auto* program = dynamic_cast<npk::ProgramNode*>(module_node.get());
            if (program) {
                if (opts.verbose) {
                    std::cout << "Phase 6: User assertion verification...\n";
                }

                int prove_count = 0;
                int assert_static_count = 0;

                // v0.14.3: Count eligible functions for progress reporting
                int verify_func_total = 0;
                int verify_func_current = 0;
                if (opts.verbose) {
                    for (const auto& decl : program->declarations) {
                        if (decl->type == npk::ASTNode::NodeType::FUNC_DECL) {
                            auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                            if (func->body) verify_func_total++;
                        }
                    }
                }

                // For each function, collect Rules-constrained variables from parameters
                // and local VarDecls, then check proves within the function body.
                for (const auto& decl : program->declarations) {
                    if (decl->type != npk::ASTNode::NodeType::FUNC_DECL) continue;
                    auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                    if (!func->body) continue;

                    // v0.14.3: Progress reporting
                    verify_func_current++;
                    if (opts.verbose && verify_func_total > 1) {
                        std::cout << "  [" << verify_func_current << "/" << verify_func_total
                                  << "] Verifying " << func->funcName << "()...\n";
                    }

                    // Build varRules map: variable name → (RulesDeclStmt*, typeName)
                    std::map<std::string, std::pair<npk::RulesDeclStmt*, std::string>> varRules;

                    // Collect from function parameters
                    for (auto& param : func->parameters) {
                        if (param->type == npk::ASTNode::NodeType::VAR_DECL) {
                            auto* vd = static_cast<npk::VarDeclStmt*>(param.get());
                            if (!vd->limitRulesName.empty()) {
                                auto it = rules_table.find(vd->limitRulesName);
                                if (it != rules_table.end()) {
                                    varRules[vd->varName] = {it->second, vd->typeName};
                                }
                            }
                        }
                    }

                    // Walk the function body to find prove/assert_static and
                    // also collect VarDecl with limit<> types
                    std::function<void(npk::ASTNode*)> walkBody;
                    walkBody = [&](npk::ASTNode* node) {
                        if (!node) return;
                        using NT = npk::ASTNode::NodeType;

                        switch (node->type) {
                            case NT::PROVE: {
                                prove_count++;
                                auto* ps = static_cast<npk::ProveStmt*>(node);
                                std::vector<npk::VerifyOutcome> outcomes;
                                auto result = z3v.proveUserAssertion(
                                    ps->condition.get(), varRules, outcomes,
                                    ps->line, ps->column);

                                for (auto& out : outcomes) {
                                    if (opts.prove_report || opts.verbose) {
                                        std::cout << "  prove(" << out.conditionText << ") at line "
                                                  << out.line << ": ";
                                        if (out.result == npk::VerifyResult::PROVEN)
                                            std::cout << "PROVEN\n";
                                        else if (out.result == npk::VerifyResult::DISPROVEN)
                                            std::cout << "DISPROVEN — " << out.detail << "\n";
                                        else
                                            std::cout << "UNKNOWN — " << out.detail << "\n";
                                    }
                                }

                                if (result != npk::VerifyResult::PROVEN) {
                                    // prove failure is a compiler error
                                    std::string msg = "prove() failed";
                                    if (!outcomes.empty()) {
                                        msg += ": " + outcomes.back().detail;
                                    }
                                    std::cerr << opts.input_files[0] << ":"
                                              << ps->line << ":" << ps->column
                                              << ": error: " << msg << "\n";
                                    prove_failed = true;
                                }
                                return;  // don't recurse into prove's condition
                            }

                            case NT::ASSERT_STATIC: {
                                assert_static_count++;
                                auto* as = static_cast<npk::AssertStaticStmt*>(node);
                                std::vector<npk::VerifyOutcome> outcomes;
                                auto result = z3v.proveUserAssertion(
                                    as->condition.get(), varRules, outcomes,
                                    as->line, as->column);

                                for (auto& out : outcomes) {
                                    if (opts.prove_report || opts.verbose) {
                                        std::cout << "  assert_static(" << out.conditionText << ") at line "
                                                  << out.line << ": ";
                                        if (out.result == npk::VerifyResult::PROVEN)
                                            std::cout << "PROVEN (erased)\n";
                                        else if (out.result == npk::VerifyResult::DISPROVEN)
                                            std::cout << "WARNING — " << out.detail << " (runtime fallback)\n";
                                        else
                                            std::cout << "WARNING — " << out.detail << " (runtime fallback)\n";
                                    }
                                }

                                if (result == npk::VerifyResult::PROVEN) {
                                    assert_static_proven_set.insert(node);
                                } else {
                                    assert_static_soft_disproven++;
                                    // Emit warning (not error — keeps runtime fallback)
                                    std::string msg = "assert_static() could not be proven";
                                    if (!outcomes.empty()) {
                                        msg += ": " + outcomes.back().detail;
                                    }
                                    std::cerr << opts.input_files[0] << ":"
                                              << as->line << ":" << as->column
                                              << ": warning: " << msg << "\n";
                                }
                                return;  // don't recurse into assert_static's condition
                            }

                            case NT::VAR_DECL: {
                                // Collect limit<Rules> local variables
                                auto* vd = static_cast<npk::VarDeclStmt*>(node);
                                if (!vd->limitRulesName.empty()) {
                                    auto it = rules_table.find(vd->limitRulesName);
                                    if (it != rules_table.end()) {
                                        varRules[vd->varName] = {it->second, vd->typeName};
                                    }
                                }
                                if (vd->initializer)
                                    walkBody(vd->initializer.get());
                                break;
                            }

                            case NT::BLOCK: {
                                auto* blk = static_cast<npk::BlockStmt*>(node);
                                for (auto& s : blk->statements)
                                    walkBody(s.get());
                                break;
                            }

                            case NT::IF: {
                                auto* s = static_cast<npk::IfStmt*>(node);
                                walkBody(s->thenBranch.get());
                                walkBody(s->elseBranch.get());
                                break;
                            }

                            case NT::WHILE: {
                                auto* s = static_cast<npk::WhileStmt*>(node);
                                walkBody(s->body.get());
                                break;
                            }

                            case NT::FOR: {
                                auto* s = static_cast<npk::ForStmt*>(node);
                                walkBody(s->body.get());
                                break;
                            }

                            case NT::LOOP: {
                                auto* s = static_cast<npk::LoopStmt*>(node);
                                walkBody(s->body.get());
                                break;
                            }

                            case NT::TILL: {
                                auto* s = static_cast<npk::TillStmt*>(node);
                                walkBody(s->body.get());
                                break;
                            }

                            case NT::WHEN: {
                                auto* s = static_cast<npk::WhenStmt*>(node);
                                walkBody(s->body.get());
                                walkBody(s->then_block.get());
                                walkBody(s->end_block.get());
                                break;
                            }

                            case NT::EXPRESSION_STMT: {
                                auto* s = static_cast<npk::ExpressionStmt*>(node);
                                walkBody(s->expression.get());
                                break;
                            }

                            default:
                                break;
                        }
                    };

                    walkBody(func->body.get());
                }

                if (opts.verbose && (prove_count > 0 || assert_static_count > 0)) {
                    std::cout << "  User assertions: " << prove_count << " prove, "
                              << assert_static_count << " assert_static\n";
                }
                if (!assert_static_proven_set.empty()) {
                    ir_gen.setAssertStaticProven(assert_static_proven_set);
                }
            }

            if (prove_failed) {
                // Don't abort here yet — let the summary print,
                // then the existing disproven > 0 check will abort
            }
        }

        // Prove report (dedicated output section)
        if (opts.prove_report) {
            const auto& sum = z3v.getSummary();
            std::cout << "\n=== Prove/Assert Report ===\n";
            for (auto& out : sum.outcomes) {
                std::string status = (out.result == npk::VerifyResult::PROVEN) ? "PROVEN" :
                    (out.result == npk::VerifyResult::DISPROVEN) ? "DISPROVEN" : "UNKNOWN";
                std::cout << "  [" << status << "] " << out.conditionText;
                if (out.line > 0) std::cout << " (line " << out.line << ")";
                if (!out.detail.empty()) std::cout << " — " << out.detail;
                std::cout << "\n";
            }
            std::cout << "===========================\n\n";
        }

        // Print verification summary
        const auto& sum = z3v.getSummary();
        if (sum.total() > 0) {
            if (opts.verify_report) {
                std::cout << "\n=== Z3 Verification Report ===\n";
                std::cout << "  Proven:    " << sum.proven << "\n";
                std::cout << "  Disproven: " << sum.disproven << "\n";
                std::cout << "  Unknown:   " << sum.unknown << "\n";
                std::cout << "  Total:     " << sum.total() << "\n";
                std::cout << "==============================\n\n";
            } else if (opts.verbose) {
                std::cout << "Z3: " << sum.proven << " proven, "
                          << sum.disproven << " disproven, "
                          << sum.unknown << " unknown\n";
            }
        }
        
        // Abort compilation if Z3 found provable violations
        // (assert_static disproven are soft warnings, not hard failures)
        if ((sum.disproven - assert_static_soft_disproven) > 0) {
            return nullptr;
        }
        
        // v0.14.3: Report verification timing
        if (opts.verbose) {
            auto verify_end = std::chrono::steady_clock::now();
            auto verify_ms = std::chrono::duration_cast<std::chrono::milliseconds>(verify_end - verify_start).count();
            std::cout << "Verification completed in " << verify_ms << "ms\n";
        }
    }
#endif // ARIA_HAS_Z3

    // Phase 3.5: Borrow Checker (Phase 5b in research)
    if (opts.verbose) {
        std::cout << "Phase 3.5: Borrow checking...\n";
    }

    npk::sema::BorrowChecker borrow_checker;
    if (opts.borrow_debug) {
        borrow_checker.setBorrowDebug(true);
    }
    if (opts.borrow_dump) {
        borrow_checker.setBorrowDump(true);
    }

#ifdef ARIA_HAS_Z3
    // A-004: Wire Rules table and a lightweight Z3 verifier into the borrow
    // checker so that limit<Rules>-constrained index variables can be proven
    // disjoint, suppressing conservative ARIA-023/ARIA-026 false positives.
    const auto& bc_rules = type_checker.getRulesTable();
    std::unique_ptr<npk::Z3Verifier> borrow_z3v;
    if (!bc_rules.empty()) {
        borrow_z3v = std::make_unique<npk::Z3Verifier>(opts.smt_timeout);
        borrow_z3v->setTypeSystem(&type_system);
        for (const auto& [name, rules_ptr] : bc_rules) {
            borrow_z3v->registerRules(name, rules_ptr);
        }
        borrow_checker.setZ3Verifier(borrow_z3v.get());
        borrow_checker.setRulesTable(&bc_rules);
    }
#endif

    auto borrow_errors = borrow_checker.analyze(module_node.get());
    
    if (!borrow_errors.empty()) {
        bool has_errors = false;
        for (const auto& err : borrow_errors) {
            npk::SourceLocation loc(filename, err.line, err.column);
            std::string msg = err.code.empty() ? err.message : "[" + err.code + "] " + err.message;
            
            switch (err.severity) {
                case npk::sema::BorrowSeverity::WARNING:
                    diags.warning(loc, msg);
                    break;
                case npk::sema::BorrowSeverity::HINT:
                    diags.note(loc, msg);
                    break;
                default:
                    diags.error(loc, msg);
                    has_errors = true;
                    break;
            }
            
            // Print related information if available
            if (err.related_line >= 0) {
                npk::SourceLocation related_loc(filename, err.related_line, err.related_column);
                diags.note(related_loc, err.related_message);
            }
            
            // Print suggestion if available
            if (!err.suggestion.empty()) {
                diags.note(loc, err.suggestion);
            }
        }
        if (has_errors) return nullptr;
    }

    // Phase 3.75: Result<T> Elision Analysis (static, no Z3 required)
    // Identifies functions that can never fail (no fail/sys calls, all callees infallible).
    // These functions return raw T instead of Result{T, ptr, i8}, eliminating wrapping overhead.
    std::set<std::string> result_elide_set;  // Declare outside smt_opt block for use in Phase 4
    if (opts.smt_opt) {
        if (opts.verbose) {
            std::cout << "Phase 3.75: Result<T> elision analysis...\n";
        }

        auto* program = dynamic_cast<npk::ProgramNode*>(module_node.get());
        if (program) {
            // Per-function analysis data
            struct FuncInfo {
                bool has_fail = false;
                bool has_sys_call = false;
                std::set<std::string> called_user_funcs;
            };

            // Step 1: Identify all user-defined functions (with bodies, excluding main/failsafe)
            std::set<std::string> user_func_names;
            for (const auto& decl : program->declarations) {
                if (decl->type != npk::ASTNode::NodeType::FUNC_DECL) continue;
                auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                if (!func->body || func->funcName == "main" || func->funcName == "failsafe") continue;
                if (!func->genericParams.empty()) continue;
                user_func_names.insert(func->funcName);
            }

            // Step 2: Recursive AST walker to detect fail/sys/calls
            std::function<void(npk::ASTNode*, FuncInfo&)> analyzeNode;
            analyzeNode = [&](npk::ASTNode* node, FuncInfo& info) {
                if (!node) return;
                // Early exit: already proven fallible on both axes
                if (info.has_fail && info.has_sys_call) return;

                using NT = npk::ASTNode::NodeType;
                switch (node->type) {
                    case NT::FAIL:
                        info.has_fail = true;
                        return;

                    case NT::CALL: {
                        auto* call = static_cast<npk::CallExpr*>(node);
                        if (auto* ident = dynamic_cast<npk::IdentifierExpr*>(call->callee.get())) {
                            if (ident->name == "sys" || ident->name == "sys!!" || ident->name == "sys!!!") {
                                info.has_sys_call = true;
                            } else if (user_func_names.count(ident->name)) {
                                info.called_user_funcs.insert(ident->name);
                            }
                        }
                        // Recurse into callee (for member access chains) and arguments
                        analyzeNode(call->callee.get(), info);
                        for (auto& arg : call->arguments) analyzeNode(arg.get(), info);
                        return;
                    }

                    case NT::BLOCK: {
                        auto* block = static_cast<npk::BlockStmt*>(node);
                        for (auto& stmt : block->statements) analyzeNode(stmt.get(), info);
                        return;
                    }

                    case NT::IF: {
                        auto* s = static_cast<npk::IfStmt*>(node);
                        analyzeNode(s->condition.get(), info);
                        analyzeNode(s->thenBranch.get(), info);
                        analyzeNode(s->elseBranch.get(), info);
                        return;
                    }

                    case NT::WHILE: {
                        auto* s = static_cast<npk::WhileStmt*>(node);
                        analyzeNode(s->condition.get(), info);
                        analyzeNode(s->body.get(), info);
                        return;
                    }

                    case NT::FOR: {
                        auto* s = static_cast<npk::ForStmt*>(node);
                        analyzeNode(s->initializer.get(), info);
                        analyzeNode(s->condition.get(), info);
                        analyzeNode(s->update.get(), info);
                        analyzeNode(s->body.get(), info);
                        analyzeNode(s->rangeExpr.get(), info);
                        return;
                    }

                    case NT::LOOP: {
                        auto* s = static_cast<npk::LoopStmt*>(node);
                        analyzeNode(s->start.get(), info);
                        analyzeNode(s->limit.get(), info);
                        analyzeNode(s->step.get(), info);
                        analyzeNode(s->body.get(), info);
                        return;
                    }

                    case NT::TILL: {
                        auto* s = static_cast<npk::TillStmt*>(node);
                        analyzeNode(s->limit.get(), info);
                        analyzeNode(s->step.get(), info);
                        analyzeNode(s->body.get(), info);
                        return;
                    }

                    case NT::WHEN: {
                        auto* s = static_cast<npk::WhenStmt*>(node);
                        analyzeNode(s->condition.get(), info);
                        analyzeNode(s->body.get(), info);
                        analyzeNode(s->then_block.get(), info);
                        analyzeNode(s->end_block.get(), info);
                        return;
                    }

                    case NT::PICK: {
                        auto* s = static_cast<npk::PickStmt*>(node);
                        analyzeNode(s->selector.get(), info);
                        for (auto& c : s->cases) {
                            auto* pc = static_cast<npk::PickCase*>(c.get());
                            analyzeNode(pc->pattern.get(), info);
                            analyzeNode(pc->body.get(), info);
                        }
                        return;
                    }

                    case NT::DEFER: {
                        auto* s = static_cast<npk::DeferStmt*>(node);
                        analyzeNode(s->block.get(), info);
                        return;
                    }

                    case NT::EXPRESSION_STMT: {
                        auto* s = static_cast<npk::ExpressionStmt*>(node);
                        analyzeNode(s->expression.get(), info);
                        return;
                    }

                    case NT::VAR_DECL: {
                        auto* s = static_cast<npk::VarDeclStmt*>(node);
                        analyzeNode(s->initializer.get(), info);
                        return;
                    }

                    case NT::RETURN: {
                        auto* s = static_cast<npk::ReturnStmt*>(node);
                        analyzeNode(s->value.get(), info);
                        return;
                    }

                    case NT::PASS: {
                        auto* s = static_cast<npk::PassStmt*>(node);
                        analyzeNode(s->value.get(), info);
                        return;
                    }

                    case NT::BINARY_OP: {
                        auto* s = static_cast<npk::BinaryExpr*>(node);
                        analyzeNode(s->left.get(), info);
                        analyzeNode(s->right.get(), info);
                        return;
                    }

                    case NT::UNARY_OP: {
                        auto* s = static_cast<npk::UnaryExpr*>(node);
                        analyzeNode(s->operand.get(), info);
                        return;
                    }

                    case NT::LAMBDA: {
                        auto* s = static_cast<npk::LambdaExpr*>(node);
                        analyzeNode(s->body.get(), info);
                        return;
                    }

                    case NT::INDEX: {
                        auto* s = static_cast<npk::IndexExpr*>(node);
                        analyzeNode(s->array.get(), info);
                        analyzeNode(s->index.get(), info);
                        return;
                    }

                    case NT::ASSIGNMENT: {
                        auto* s = static_cast<npk::AssignmentExpr*>(node);
                        analyzeNode(s->target.get(), info);
                        analyzeNode(s->value.get(), info);
                        return;
                    }

                    case NT::TERNARY: {
                        auto* s = static_cast<npk::TernaryExpr*>(node);
                        analyzeNode(s->condition.get(), info);
                        analyzeNode(s->trueValue.get(), info);
                        analyzeNode(s->falseValue.get(), info);
                        return;
                    }

                    case NT::MEMBER_ACCESS:
                    case NT::POINTER_MEMBER: {
                        auto* s = static_cast<npk::MemberAccessExpr*>(node);
                        analyzeNode(s->object.get(), info);
                        return;
                    }

                    case NT::TEMPLATE_LITERAL: {
                        auto* s = static_cast<npk::TemplateLiteralExpr*>(node);
                        for (auto& interp : s->interpolations) analyzeNode(interp.get(), info);
                        return;
                    }

                    case NT::ARRAY_LITERAL: {
                        auto* s = static_cast<npk::ArrayLiteralExpr*>(node);
                        for (auto& elem : s->elements) analyzeNode(elem.get(), info);
                        return;
                    }

                    case NT::OBJECT_LITERAL: {
                        auto* s = static_cast<npk::ObjectLiteralExpr*>(node);
                        for (auto& field : s->fields) analyzeNode(field.value.get(), info);
                        return;
                    }

                    case NT::CAST: {
                        auto* s = static_cast<npk::CastExpr*>(node);
                        analyzeNode(s->expression.get(), info);
                        return;
                    }

                    case NT::AWAIT: {
                        auto* s = static_cast<npk::AwaitExpr*>(node);
                        analyzeNode(s->operand.get(), info);
                        return;
                    }

                    case NT::DEFAULTS: {
                        auto* s = static_cast<npk::DefaultsExpr*>(node);
                        analyzeNode(s->expr.get(), info);
                        analyzeNode(s->fallback.get(), info);
                        return;
                    }

                    default:
                        return;  // Leaf nodes (LITERAL, IDENTIFIER, BREAK, CONTINUE, etc.)
                }
            };

            // Step 3: Analyze each user function
            std::map<std::string, FuncInfo> func_info;
            for (const auto& decl : program->declarations) {
                if (decl->type != npk::ASTNode::NodeType::FUNC_DECL) continue;
                auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                if (!func->body || func->funcName == "main" || func->funcName == "failsafe") continue;
                if (!func->genericParams.empty()) continue;

                FuncInfo info;
                analyzeNode(func->body.get(), info);
                func_info[func->funcName] = info;
            }

            // Step 4: Fixed-point iteration to resolve transitive call dependencies
            // A function is infallible iff: no fail, no sys, and all callees are infallible
            std::set<std::string> can_fail_set;
            bool changed = true;
            while (changed) {
                changed = false;
                for (auto& [name, info] : func_info) {
                    if (result_elide_set.count(name) || can_fail_set.count(name)) continue;

                    // Direct fail/sys → immediately fallible
                    if (info.has_fail || info.has_sys_call) {
                        can_fail_set.insert(name);
                        changed = true;
                        continue;
                    }

                    // Check all called user functions
                    bool all_infallible = true;
                    bool has_unresolved = false;
                    for (const auto& callee : info.called_user_funcs) {
                        if (can_fail_set.count(callee)) {
                            all_infallible = false;
                            break;
                        }
                        if (!result_elide_set.count(callee)) {
                            has_unresolved = true;
                        }
                    }

                    if (!all_infallible) {
                        can_fail_set.insert(name);
                        changed = true;
                    } else if (!has_unresolved) {
                        result_elide_set.insert(name);
                        changed = true;
                    }
                }
            }

            if (opts.verbose && !result_elide_set.empty()) {
                std::cout << "  Result<T> elision: " << result_elide_set.size()
                         << " infallible function(s)\n";
                for (const auto& name : result_elide_set) {
                    std::cout << "    - " << name << "\n";
                }
            }
        }
    }

    // Phase 5.75: Defaults/?| fallback elimination
    // If the sub-expression of a DefaultsExpr only calls infallible functions
    // (those in result_elide_set), the fallback path is dead code.
    if (!result_elide_set.empty()) {
        auto* program = dynamic_cast<npk::ProgramNode*>(module_node.get());
        if (program) {
            if (opts.verbose) {
                std::cout << "Phase 5.75: Defaults fallback elimination...\n";
            }

            // Check if an expression tree contains only infallible calls
            std::function<bool(npk::ASTNode*)> isInfallibleExpr;
            isInfallibleExpr = [&](npk::ASTNode* node) -> bool {
                if (!node) return true;
                using NT = npk::ASTNode::NodeType;

                switch (node->type) {
                    case NT::CALL: {
                        auto* call = static_cast<npk::CallExpr*>(node);
                        // Check callee name
                        if (call->callee && call->callee->type == NT::IDENTIFIER) {
                            auto* ident = static_cast<npk::IdentifierExpr*>(
                                call->callee.get());
                            if (!result_elide_set.count(ident->name)) {
                                return false;  // Fallible call
                            }
                        } else {
                            return false;  // Indirect call — assume fallible
                        }
                        // Check all arguments recursively
                        for (auto& arg : call->arguments) {
                            if (!isInfallibleExpr(arg.get())) return false;
                        }
                        return true;
                    }

                    case NT::BINARY_OP: {
                        auto* bin = static_cast<npk::BinaryExpr*>(node);
                        return isInfallibleExpr(bin->left.get()) &&
                               isInfallibleExpr(bin->right.get());
                    }

                    case NT::UNARY_OP: {
                        auto* un = static_cast<npk::UnaryExpr*>(node);
                        return isInfallibleExpr(un->operand.get());
                    }

                    // Leaf nodes: always infallible
                    case NT::LITERAL:
                    case NT::IDENTIFIER:
                        return true;

                    // DefaultsExpr always produces a value (expr or fallback)
                    case NT::DEFAULTS:
                        return true;

                    case NT::INDEX: {
                        auto* idx = static_cast<npk::IndexExpr*>(node);
                        return isInfallibleExpr(idx->array.get()) &&
                               isInfallibleExpr(idx->index.get());
                    }

                    case NT::MEMBER_ACCESS: {
                        auto* mem = static_cast<npk::MemberAccessExpr*>(node);
                        return isInfallibleExpr(mem->object.get());
                    }

                    default:
                        return false;  // Unknown — assume may fail
                }
            };

            // Walk AST to find DefaultsExpr nodes
            std::function<void(npk::ASTNode*)> findDefaults;
            findDefaults = [&](npk::ASTNode* node) {
                if (!node) return;
                using NT = npk::ASTNode::NodeType;

                switch (node->type) {
                    case NT::DEFAULTS: {
                        auto* def = static_cast<npk::DefaultsExpr*>(node);
                        if (isInfallibleExpr(def->expr.get())) {
                            defaults_safe_set.insert(node);
                        }
                        findDefaults(def->expr.get());
                        findDefaults(def->fallback.get());
                        break;
                    }

                    case NT::VAR_DECL: {
                        auto* var = static_cast<npk::VarDeclStmt*>(node);
                        findDefaults(var->initializer.get());
                        break;
                    }

                    case NT::BLOCK: {
                        auto* blk = static_cast<npk::BlockStmt*>(node);
                        for (auto& s : blk->statements)
                            findDefaults(s.get());
                        break;
                    }

                    case NT::EXPRESSION_STMT: {
                        auto* s = static_cast<npk::ExpressionStmt*>(node);
                        findDefaults(s->expression.get());
                        break;
                    }

                    case NT::IF: {
                        auto* s = static_cast<npk::IfStmt*>(node);
                        findDefaults(s->condition.get());
                        findDefaults(s->thenBranch.get());
                        findDefaults(s->elseBranch.get());
                        break;
                    }

                    case NT::WHILE: {
                        auto* s = static_cast<npk::WhileStmt*>(node);
                        findDefaults(s->condition.get());
                        findDefaults(s->body.get());
                        break;
                    }

                    case NT::FOR: {
                        auto* s = static_cast<npk::ForStmt*>(node);
                        findDefaults(s->initializer.get());
                        findDefaults(s->condition.get());
                        findDefaults(s->update.get());
                        findDefaults(s->body.get());
                        break;
                    }

                    case NT::LOOP: {
                        auto* s = static_cast<npk::LoopStmt*>(node);
                        findDefaults(s->start.get());
                        findDefaults(s->limit.get());
                        findDefaults(s->step.get());
                        findDefaults(s->body.get());
                        break;
                    }

                    case NT::TILL: {
                        auto* s = static_cast<npk::TillStmt*>(node);
                        findDefaults(s->limit.get());
                        findDefaults(s->step.get());
                        findDefaults(s->body.get());
                        break;
                    }

                    case NT::WHEN: {
                        auto* s = static_cast<npk::WhenStmt*>(node);
                        findDefaults(s->condition.get());
                        findDefaults(s->body.get());
                        findDefaults(s->then_block.get());
                        findDefaults(s->end_block.get());
                        break;
                    }

                    case NT::PICK: {
                        auto* s = static_cast<npk::PickStmt*>(node);
                        findDefaults(s->selector.get());
                        for (auto& c : s->cases) {
                            auto* pc = static_cast<npk::PickCase*>(c.get());
                            findDefaults(pc->pattern.get());
                            findDefaults(pc->body.get());
                        }
                        break;
                    }

                    case NT::DEFER: {
                        auto* s = static_cast<npk::DeferStmt*>(node);
                        findDefaults(s->block.get());
                        break;
                    }

                    case NT::RETURN: {
                        auto* s = static_cast<npk::ReturnStmt*>(node);
                        findDefaults(s->value.get());
                        break;
                    }

                    case NT::PASS: {
                        auto* s = static_cast<npk::PassStmt*>(node);
                        findDefaults(s->value.get());
                        break;
                    }

                    case NT::CALL: {
                        auto* call = static_cast<npk::CallExpr*>(node);
                        findDefaults(call->callee.get());
                        for (auto& arg : call->arguments)
                            findDefaults(arg.get());
                        break;
                    }

                    case NT::BINARY_OP: {
                        auto* bin = static_cast<npk::BinaryExpr*>(node);
                        findDefaults(bin->left.get());
                        findDefaults(bin->right.get());
                        break;
                    }

                    case NT::UNARY_OP: {
                        auto* un = static_cast<npk::UnaryExpr*>(node);
                        findDefaults(un->operand.get());
                        break;
                    }

                    case NT::UNWRAP: {
                        auto* uw = static_cast<npk::UnwrapExpr*>(node);
                        findDefaults(uw->result.get());
                        findDefaults(uw->defaultValue.get());
                        break;
                    }

                    default:
                        break;
                }
            };

            for (const auto& decl : program->declarations) {
                if (decl->type != npk::ASTNode::NodeType::FUNC_DECL) continue;
                auto* func = static_cast<npk::FuncDeclStmt*>(decl.get());
                if (!func->body) continue;
                findDefaults(func->body.get());
            }

            if (opts.verbose && !defaults_safe_set.empty()) {
                std::cout << "  Defaults fallback elimination: " << defaults_safe_set.size()
                          << " fallback path(s) proven dead\n";
            }
        }
    }

    // Phase 4: IR Generation
    if (opts.verbose) {
        std::cout << "Phase 4: IR generation...\n";
    }
    
    // Pass TypeSystem to IR generator for struct type lookups
    ir_gen.setTypeSystem(&type_system);

#ifdef ARIA_HAS_Z3
    // Pass SMT-proven ustack optimization set to IR generator
    if (!ustack_opt_funcs.empty()) {
        ir_gen.setUStackOptimizedFuncs(ustack_opt_funcs);
    }
    // Pass SMT-proven uhash optimization set to IR generator
    if (!uhash_opt_funcs.empty()) {
        ir_gen.setUHashOptimizedFuncs(uhash_opt_funcs);
    }
#endif

    // Pass Result<T> elision set to IR generator (static analysis, no Z3 needed)
    if (!result_elide_set.empty()) {
        ir_gen.setResultElideFuncs(result_elide_set);
    }

    // Pass dead branch sets to IR generator (Z3-proven dead branches)
    if (!dead_branch_true_set.empty()) {
        ir_gen.setDeadBranchTrue(dead_branch_true_set);
    }
    if (!dead_branch_false_set.empty()) {
        ir_gen.setDeadBranchFalse(dead_branch_false_set);
    }

    // Pass bounds-safe set to IR generator (Z3-proven always-in-bounds array accesses)
    if (!bounds_safe_set.empty()) {
        ir_gen.setBoundsCheckSafe(bounds_safe_set);
    }

    // Pass overflow-safe set to IR generator (Z3-proven overflow-free arithmetic)
    if (!overflow_safe_set.empty()) {
        ir_gen.setOverflowCheckSafe(overflow_safe_set);
    }

    // Pass div-safe set to IR generator (Z3-proven non-zero divisors) (v0.14.4)
    if (!div_safe_set.empty()) {
        ir_gen.setDivCheckSafe(div_safe_set);
    }

    // Pass null-check-safe set to IR generator (Z3-proven non-null expressions)
    if (!null_check_safe_set.empty()) {
        ir_gen.setNullCheckSafe(null_check_safe_set);
    }

    // Pass loop-hoist map to IR generator (loop-invariant declarations to hoist)
    if (!loop_hoist_map.empty()) {
        ir_gen.setLoopHoistMap(loop_hoist_map);
    }

    // Pass limit-check-safe set to IR generator (Rules propagation)
    if (!limit_check_safe_set.empty()) {
        ir_gen.setLimitCheckSafe(limit_check_safe_set);
    }

    // Pass defaults-safe set to IR generator (fallback elimination)
    if (!defaults_safe_set.empty()) {
        ir_gen.setDefaultsSafe(defaults_safe_set);
    }
    
    // Initialize debug info if -g flag is set
    if (opts.debug_info) {
        std::string file = filename;
        std::string dir;
        size_t slash = filename.find_last_of("/\\");
        if (slash != std::string::npos) {
            dir = filename.substr(0, slash);
            file = filename.substr(slash + 1);
        }
        // If the path is relative, make it absolute using cwd
        if (dir.empty() || dir[0] != '/') {
            char cwd[PATH_MAX];
            std::string cwd_str = getcwd(cwd, sizeof(cwd)) ? cwd : ".";
            dir = dir.empty() ? cwd_str : cwd_str + "/" + dir;
        }
        ir_gen.initDebugInfo(file, dir);
    }
    
    // v0.2.36: Forward-declare specialized generic functions FIRST (before main codegen)
    // This ensures call sites can resolve, but bodies are generated AFTER main codegen
    // so that trait impl methods (e.g. int32_add_ten from impl:Addable:for:int32) exist.
    const auto& specializations = monomorphizer.getSpecializations();
    if (!specializations.empty()) {
        if (opts.verbose) {
            std::cout << "  Forward-declaring " << specializations.size()
                     << " specialized generic function(s)...\n";
        }
        ir_gen.declareSpecializedFunctions(specializations);
    }
    
    // Generate IR for loaded modules (needed for cross-module function calls)
    // Use load order so dependencies are compiled before the modules that use them
    const auto& load_order = module_loader.getLoadOrder();
    for (const auto& path : load_order) {
        auto it = loaded_modules.find(path);
        if (it == loaded_modules.end() || !it->second || !it->second->ast) continue;
        const auto& loaded_module = it->second;
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
                    diags.error(npk::SourceLocation(path, 0, 0), "Module IR generation failed");
                    return nullptr;
                }
            } catch (const std::exception& e) {
                diags.error(npk::SourceLocation(path, 0, 0),
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
            diags.error(npk::SourceLocation(filename, 0, 0), "IR generation failed");
            return nullptr;
        }
    } catch (const std::exception& e) {
        diags.error(npk::SourceLocation(filename, 0, 0),
                   std::string("IR generation error: ") + e.what());
        return nullptr;
    }
    
    // v0.2.36: Now generate specialization BODIES (after main codegen created impl methods)
    if (!specializations.empty()) {
        if (opts.verbose) {
            std::cout << "  Generating " << specializations.size()
                     << " specialized generic function bodies...\n";
        }
        try {
            size_t generated = ir_gen.codegenSpecializedBodies(specializations);
            if (opts.verbose) {
                std::cout << "  Generated " << generated << " specialization(s)\n";
            }
        } catch (const std::exception& e) {
            diags.error(npk::SourceLocation(filename, 0, 0),
                       std::string("Specialization IR error: ") + e.what());
            return nullptr;
        }
    }

    // Finalize debug info (must be done after all codegen)
    if (opts.debug_info) {
        ir_gen.finalizeDebugInfo();
    }

    // v0.7.0+: Inject wild/wildx memory flags into main() entry block
    // v0.8.0+: Inject GC configuration flags
    llvm::Module* mod = ir_gen.getModule();
    bool has_runtime_flags = opts.wild_stats || opts.guard_pages || opts.wildx_audit || 
                             opts.wildx_guard_pages || opts.gc_stats ||
                             opts.gc_nursery_size || opts.gc_threshold || opts.gc_max_heap;
    if (mod && has_runtime_flags) {
        llvm::Function* main_func = mod->getFunction("main");
        if (main_func && !main_func->empty()) {
            llvm::BasicBlock& entry = main_func->getEntryBlock();
            // Find the insertion point: right after the runtime init calls
            // (npk_gc_init, npk_args_init, npk_streams_init)
            // but before any user code.
            llvm::Instruction* insert_before = nullptr;
            int init_calls_seen = 0;
            for (auto& inst : entry) {
                if (auto* call = llvm::dyn_cast<llvm::CallInst>(&inst)) {
                    llvm::Function* callee = call->getCalledFunction();
                    if (callee) {
                        std::string name = callee->getName().str();
                        if (name == "npk_gc_init" || name == "npk_args_init" ||
                            name == "npk_streams_init") {
                            init_calls_seen++;
                            continue;
                        }
                    }
                }
                // First non-init instruction = insert point
                insert_before = &inst;
                break;
            }
            
            if (!insert_before) {
                // Fallback: insert before terminator
                insert_before = entry.getTerminator();
            }

            llvm::IRBuilder<> inject_builder(insert_before);
            llvm::LLVMContext& ctx = mod->getContext();
            llvm::FunctionType* void_bool_ty = llvm::FunctionType::get(
                llvm::Type::getVoidTy(ctx),
                {llvm::Type::getInt8Ty(ctx)},
                false
            );
            
            if (opts.wild_stats) {
                llvm::FunctionCallee enable_stats = mod->getOrInsertFunction(
                    "npk_wild_enable_stats_at_exit", void_bool_ty);
                inject_builder.CreateCall(enable_stats, {inject_builder.getInt8(1)});
            }
            if (opts.guard_pages) {
                llvm::FunctionCallee enable_guards = mod->getOrInsertFunction(
                    "npk_wild_enable_guard_pages", void_bool_ty);
                inject_builder.CreateCall(enable_guards, {inject_builder.getInt8(1)});
            }
            if (opts.wildx_audit) {
                llvm::FunctionCallee enable_audit = mod->getOrInsertFunction(
                    "npk_wildx_enable_audit", void_bool_ty);
                inject_builder.CreateCall(enable_audit, {inject_builder.getInt8(1)});
            }
            if (opts.wildx_guard_pages) {
                llvm::FunctionCallee enable_wildx_guards = mod->getOrInsertFunction(
                    "npk_wildx_enable_guard_pages", void_bool_ty);
                inject_builder.CreateCall(enable_wildx_guards, {inject_builder.getInt8(1)});
            }
            
            // v0.8.0: GC configuration injection
            if (opts.gc_nursery_size || opts.gc_threshold) {
                // Replace default npk_gc_init with custom parameters
                // Find and update the existing npk_gc_init call
                for (auto& inst : entry) {
                    if (auto* call = llvm::dyn_cast<llvm::CallInst>(&inst)) {
                        llvm::Function* callee = call->getCalledFunction();
                        if (callee && callee->getName() == "npk_gc_init") {
                            // Replace operands with user-specified values
                            if (opts.gc_nursery_size) {
                                call->setArgOperand(0, llvm::ConstantInt::get(
                                    llvm::Type::getInt64Ty(ctx), opts.gc_nursery_size));
                            }
                            if (opts.gc_threshold) {
                                call->setArgOperand(1, llvm::ConstantInt::get(
                                    llvm::Type::getInt64Ty(ctx), opts.gc_threshold));
                            }
                            break;
                        }
                    }
                }
            }
            
            if (opts.gc_max_heap) {
                llvm::FunctionType* void_i64_ty = llvm::FunctionType::get(
                    llvm::Type::getVoidTy(ctx),
                    {llvm::Type::getInt64Ty(ctx)},
                    false
                );
                llvm::FunctionCallee set_max_heap = mod->getOrInsertFunction(
                    "npk_gc_set_max_heap", void_i64_ty);
                inject_builder.CreateCall(set_max_heap, {
                    llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), opts.gc_max_heap)
                });
            }
            
            if (opts.gc_stats) {
                llvm::FunctionCallee enable_gc_stats = mod->getOrInsertFunction(
                    "npk_gc_enable_stats_at_exit", void_bool_ty);
                inject_builder.CreateCall(enable_gc_stats, {inject_builder.getInt8(1)});
            }
        }
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
bool emit_assembly(llvm::Module* module, const std::string& output_file, bool debug_info = false) {
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

        // Use O0 when debug info is enabled to preserve variables and line entries
        auto opt = debug_info ? llvm::OptimizationLevel::O0 : llvm::OptimizationLevel::O1;
        llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(opt);

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
 * Emit object file (.o) for library compilation
 */
bool emit_object(llvm::Module* module, const std::string& output_file) {
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

    bool useLargeIntMode = hasLargeIntegerTypes(module);
    if (useLargeIntMode) {
        std::cerr << "[WARN] Large integer types (128+ bit) detected. "
                  << "Using unoptimized codegen to avoid LLVM bugs.\n";
    }

    auto cpu = "generic";
    auto features = "";
    llvm::TargetOptions opt;

    if (useLargeIntMode) {
        opt.EnableGlobalISel = true;
        opt.GlobalISelAbort = llvm::GlobalISelAbortMode::Disable;
    }

    llvm::CodeGenOptLevel codegenOpt = useLargeIntMode ?
        llvm::CodeGenOptLevel::None : llvm::CodeGenOptLevel::Default;

    auto target_machine = target->createTargetMachine(
        target_triple,
        cpu,
        features,
        opt,
        llvm::Reloc::PIC_,
        llvm::CodeModel::Small,
        codegenOpt
    );

    module->setDataLayout(target_machine->createDataLayout());

    // ARIA-021: For large integers (128+ bits), skip optimization entirely
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

        // Use default O1 optimization pipeline
        llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O1);

        // Run optimization pipeline
        MPM.run(*module, MAM);
    }
    // For large integers: no optimization passes at all, direct codegen

    std::error_code ec;
    llvm::raw_fd_ostream out(output_file, ec, llvm::sys::fs::OF_None);

    if (ec) {
        std::cerr << "Error: Could not open output file: " << ec.message() << "\n";
        return false;
    }

    llvm::legacy::PassManager pass;
    auto file_type = llvm::CodeGenFileType::ObjectFile;  // Object file, not assembly

    if (target_machine->addPassesToEmitFile(pass, out, nullptr, file_type)) {
        std::cerr << "Error: Target machine can't emit a file of this type\n";
        return false;
    }

    pass.run(*module);
    out.flush();

    return true;
}

/**
 * Emit PTX assembly for GPU execution (NVIDIA CUDA)
 * Phase 2: PTX Code Generation
 * 
 * @param module LLVM module to compile
 * @param output_file Output PTX file path
 * @param gpu_arch CUDA compute capability (e.g., "sm_75" for Turing, "sm_86" for Ampere)
 * @param opt_level GPU optimization level (0-3, default 3)
 * @param enable_debug Embed debug info in PTX
 * @return true on success
 */
bool emit_ptx(
    llvm::Module* module,
    const std::string& output_file,
    const std::string& gpu_arch = "sm_50",
    int opt_level = 3,
    bool enable_debug = false
) {
    // Set target triple to NVPTX64 (64-bit CUDA)
    module->setTargetTriple("nvptx64-nvidia-cuda");
    
    // Lookup NVPTX target
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget("nvptx64", error);
    
    if (!target) {
        std::cerr << "Error: NVPTX target not found: " << error << "\n";
        std::cerr << "Make sure LLVM was built with NVPTX backend enabled\n";
        std::cerr << "Check: llvm-config --targets-built | grep NVPTX\n";
        return false;
    }
    
    // Configure target machine for GPU
    std::string cpu = gpu_arch;  // e.g., "sm_75" (Turing), "sm_86" (Ampere), "sm_90" (Hopper)
    std::string features = "+ptx70";  // PTX ISA version 7.0+ (CUDA 10.0+)
    llvm::TargetOptions target_opts;
    
    // Map Aria opt level to LLVM optimization level
    llvm::CodeGenOptLevel codegenOpt;
    llvm::OptimizationLevel llvm_opt;
    switch (opt_level) {
        case 0:
            codegenOpt = llvm::CodeGenOptLevel::None;
            llvm_opt = llvm::OptimizationLevel::O0;
            break;
        case 1:
            codegenOpt = llvm::CodeGenOptLevel::Less;
            llvm_opt = llvm::OptimizationLevel::O1;
            break;
        case 2:
            codegenOpt = llvm::CodeGenOptLevel::Default;
            llvm_opt = llvm::OptimizationLevel::O2;
            break;
        case 3:
        default:
            codegenOpt = llvm::CodeGenOptLevel::Aggressive;
            llvm_opt = llvm::OptimizationLevel::O3;
            break;
    }
    
    // Create NVPTX target machine
    auto target_machine = target->createTargetMachine(
        "nvptx64-nvidia-cuda",
        cpu,
        features,
        target_opts,
        llvm::Reloc::PIC_,    // Position-independent code
        std::nullopt,          // Code model (default)
        codegenOpt
    );
    
    if (!target_machine) {
        std::cerr << "Error: Could not create NVPTX target machine\n";
        return false;
    }
    
    // Set data layout for NVPTX
    module->setDataLayout(target_machine->createDataLayout());
    
    // Run optimization passes (GPU code benefits from aggressive optimization)
    if (opt_level > 0) {
        llvm::LoopAnalysisManager LAM;
        llvm::FunctionAnalysisManager FAM;
        llvm::CGSCCAnalysisManager CGAM;
        llvm::ModuleAnalysisManager MAM;
        
        llvm::PassBuilder PB;
        PB.registerModuleAnalyses(MAM);
        PB.registerCGSCCAnalyses(CGAM);
        PB.registerFunctionAnalyses(FAM);
        PB.registerLoopAnalyses(LAM);
        PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
        
        llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(llvm_opt);
        MPM.run(*module, MAM);
    }
    
    // Open output file
    std::error_code ec;
    llvm::raw_fd_ostream out(output_file, ec, llvm::sys::fs::OF_None);
    
    if (ec) {
        std::cerr << "Error: Could not open PTX output file: " << output_file << "\n";
        std::cerr << "  " << ec.message() << "\n";
        return false;
    }
    
    // Emit PTX assembly
    llvm::legacy::PassManager pass;
    auto file_type = llvm::CodeGenFileType::AssemblyFile;  // PTX is textual assembly
    
    if (target_machine->addPassesToEmitFile(pass, out, nullptr, file_type)) {
        std::cerr << "Error: NVPTX target can't emit PTX file\n";
        return false;
    }
    
    pass.run(*module);
    
    std::cerr << "[PTX] Generated GPU code: " << output_file << "\n";
    std::cerr << "[PTX] Target: " << gpu_arch << ", Optimization: O" << opt_level << "\n";
    
    return true;
}

/**
 * Emit WebAssembly object file (.o in wasm32 format)
 *
 * Uses LLVM's WebAssembly backend to compile the module to a relocatable
 * WASM object file. This can then be linked with wasm-ld + WASI runtime.
 *
 * @param module LLVM module to compile
 * @param output_file Output object file path
 * @param target_triple WASM target triple (wasm32-wasi or wasm32-unknown-unknown)
 * @param opt_level Optimization level (0-3)
 * @return true on success
 */
bool emit_wasm_object(
    llvm::Module* module,
    const std::string& output_file,
    const std::string& target_triple = "wasm32-wasi",
    int opt_level = 2
) {
    // Set target triple for WebAssembly
    module->setTargetTriple(target_triple);

    // WASI expects __main_argc_argv instead of main (crt1.o -> __main_void -> __main_argc_argv)
    if (target_triple.find("wasi") != std::string::npos) {
        if (auto* main_fn = module->getFunction("main")) {
            main_fn->setName("__main_argc_argv");
        }
    }

    // Lookup WebAssembly target
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget("wasm32", error);

    if (!target) {
        std::cerr << "Error: WebAssembly target not found: " << error << "\n";
        std::cerr << "Make sure LLVM was built with WebAssembly backend enabled\n";
        std::cerr << "Check: llvm-config --targets-built | grep WebAssembly\n";
        return false;
    }

    // Configure target machine for WASM
    std::string cpu = "generic";
    std::string features = "+mutable-globals,+sign-ext";  // Common WASM features
    llvm::TargetOptions target_opts;

    // Map optimization level
    llvm::CodeGenOptLevel codegenOpt;
    llvm::OptimizationLevel llvm_opt;
    switch (opt_level) {
        case 0:
            codegenOpt = llvm::CodeGenOptLevel::None;
            llvm_opt = llvm::OptimizationLevel::O0;
            break;
        case 1:
            codegenOpt = llvm::CodeGenOptLevel::Less;
            llvm_opt = llvm::OptimizationLevel::O1;
            break;
        case 3:
            codegenOpt = llvm::CodeGenOptLevel::Aggressive;
            llvm_opt = llvm::OptimizationLevel::O3;
            break;
        case 2:
        default:
            codegenOpt = llvm::CodeGenOptLevel::Default;
            llvm_opt = llvm::OptimizationLevel::O2;
            break;
    }

    // Create WebAssembly target machine
    auto target_machine = target->createTargetMachine(
        target_triple,
        cpu,
        features,
        target_opts,
        llvm::Reloc::PIC_,    // Position-independent code (required for WASM)
        std::nullopt,          // Code model
        codegenOpt
    );

    if (!target_machine) {
        std::cerr << "Error: Could not create WebAssembly target machine\n";
        return false;
    }

    // Set data layout for wasm32
    module->setDataLayout(target_machine->createDataLayout());

    // Run optimization passes
    if (opt_level > 0) {
        llvm::LoopAnalysisManager LAM;
        llvm::FunctionAnalysisManager FAM;
        llvm::CGSCCAnalysisManager CGAM;
        llvm::ModuleAnalysisManager MAM;

        llvm::PassBuilder PB(target_machine);
        PB.registerModuleAnalyses(MAM);
        PB.registerCGSCCAnalyses(CGAM);
        PB.registerFunctionAnalyses(FAM);
        PB.registerLoopAnalyses(LAM);
        PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

        llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(llvm_opt);
        MPM.run(*module, MAM);
    }

    // Open output file
    std::error_code ec;
    llvm::raw_fd_ostream out(output_file, ec, llvm::sys::fs::OF_None);

    if (ec) {
        std::cerr << "Error: Could not open WASM output file: " << output_file << "\n";
        std::cerr << "  " << ec.message() << "\n";
        return false;
    }

    // Emit WASM object file
    llvm::legacy::PassManager pass;
    auto file_type = llvm::CodeGenFileType::ObjectFile;

    if (target_machine->addPassesToEmitFile(pass, out, nullptr, file_type)) {
        std::cerr << "Error: WebAssembly target can't emit object file\n";
        return false;
    }

    pass.run(*module);
    out.flush();

    return true;
}

/**
 * Check LLVM module for WASM-incompatible features and emit warnings.
 */
void check_wasm_compatibility(llvm::Module* module, bool verbose) {
    // Functions that are not available in WASM
    static const char* unsupported_prefixes[] = {
        "npk_thread_", "npk_mutex_", "npk_rwlock_", "npk_condvar_",
        "npk_channel_", "npk_threadpool_", "npk_atomic_",
        "npk_async_", "npk_io_uring_", "npk_event_loop_",
        "npk_process_", "npk_fork", "npk_exec", "npk_signal_",
        "npk_pipe_", "npk_mmap", "npk_mprotect",
        "npk_ipc_", "npk_shmem_",
        "npk_avx_", "npk_simd_",
        nullptr
    };

    int warning_count = 0;
    for (auto& func : module->functions()) {
        if (!func.isDeclaration()) continue;
        std::string name = func.getName().str();
        for (int i = 0; unsupported_prefixes[i]; i++) {
            if (name.find(unsupported_prefixes[i]) == 0) {
                std::cerr << "Warning: WASM target does not support '" << name
                          << "' — this call will fail at runtime\n";
                warning_count++;
                break;
            }
        }
    }
    if (warning_count > 0) {
        std::cerr << "Warning: " << warning_count
                  << " WASM-incompatible function(s) detected. "
                  << "Threading, async I/O, process management, and SIMD "
                  << "are not available in WebAssembly.\n";
    } else if (verbose) {
        std::cout << "WASM compatibility check passed — no incompatible features detected\n";
    }
}

/**
 * Link WASM object file with wasm-ld + WASI runtime to produce final .wasm
 *
 * @param object_file Input WASM object file
 * @param output_file Output .wasm file
 * @param opts Compiler options
 * @return true on success
 */
bool link_wasm(const std::string& object_file, const std::string& output_file, const CompilerOptions& opts) {
    // Get compiler executable directory for runtime library search
    std::string exe_dir;
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (len != -1) {
        exe_path[len] = '\0';
        std::string full_exe_path(exe_path);
        size_t last_slash = full_exe_path.find_last_of('/');
        if (last_slash != std::string::npos) {
            exe_dir = full_exe_path.substr(0, last_slash);
        }
    }

    // Find the WASM runtime library
    std::vector<std::string> runtime_search = {
        "build/libaria_runtime_wasm.a",
        "../build/libaria_runtime_wasm.a",
        "libaria_runtime_wasm.a",
        "/usr/local/lib/libaria_runtime_wasm.a",
        "/usr/lib/libaria_runtime_wasm.a"
    };
    if (!exe_dir.empty()) {
        runtime_search.insert(runtime_search.begin(), exe_dir + "/libaria_runtime_wasm.a");
    }

    std::string wasm_runtime;
    for (const auto& path : runtime_search) {
        if (access(path.c_str(), F_OK) == 0) {
            wasm_runtime = path;
            break;
        }
    }

    // WASI libc sysroot locations
    std::string wasi_crt1;
    std::string wasi_libc;
    std::vector<std::string> wasi_sysroot_paths = {
        "/usr/lib/wasm32-wasi",
        "/opt/wasi-sdk/share/wasi-sysroot/lib/wasm32-wasi"
    };

    std::string wasi_lib_dir;
    for (const auto& dir : wasi_sysroot_paths) {
        std::string crt1 = dir + "/crt1.o";
        std::string libc = dir + "/libc.a";
        if (access(crt1.c_str(), F_OK) == 0 && access(libc.c_str(), F_OK) == 0) {
            wasi_lib_dir = dir;
            wasi_crt1 = crt1;
            wasi_libc = libc;
            break;
        }
    }

    if (wasi_lib_dir.empty()) {
        std::cerr << "Error: WASI libc sysroot not found\n";
        std::cerr << "Install wasi-libc: sudo apt install wasi-libc\n";
        std::cerr << "  Or install WASI SDK from https://github.com/WebAssembly/wasi-sdk\n";
        std::cerr << "  Searched:\n";
        for (const auto& p : wasi_sysroot_paths) {
            std::cerr << "    " << p << "\n";
        }
        return false;
    }

    // Build wasm-ld command
    std::vector<std::string> link_args;
    link_args.push_back("wasm-ld");

    // WASI CRT startup
    link_args.push_back(wasi_crt1);

    // Input object file
    link_args.push_back(object_file);

    // WASM runtime library
    if (!wasm_runtime.empty()) {
        link_args.push_back(wasm_runtime);
    } else {
        std::cerr << "Warning: Could not find libaria_runtime_wasm.a, linking without WASM runtime\n";
    }

    // WASI libc
    link_args.push_back(wasi_libc);

    // Library search path
    link_args.push_back("-L" + wasi_lib_dir);

    // Allow undefined symbols that WASI will provide
    // (some runtime functions may reference WASI imports)
    
    // Output
    link_args.push_back("-o");
    link_args.push_back(output_file);

    if (opts.verbose) {
        std::cout << "[WASM-LD]";
        for (const auto& a : link_args) std::cout << " " << a;
        std::cout << "\n";
    }

    // Invoke wasm-ld
    std::vector<char*> argv_vec;
    for (auto& s : link_args) argv_vec.push_back(const_cast<char*>(s.c_str()));
    argv_vec.push_back(nullptr);

    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "npkc: fork failed: " << strerror(errno) << "\n";
        return false;
    }
    if (pid == 0) {
        execvp("wasm-ld", argv_vec.data());
        std::cerr << "npkc: execvp(wasm-ld): " << strerror(errno) << "\n";
        std::cerr << "Install wasm-ld: sudo apt install lld\n";
        _exit(127);
    }
    int wstatus = 0;
    if (waitpid(pid, &wstatus, 0) == -1) {
        std::cerr << "npkc: waitpid failed: " << strerror(errno) << "\n";
        return false;
    }

    if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) {
        std::cerr << "[WASM] Generated: " << output_file << "\n";
        std::cerr << "[WASM] Target: " << opts.wasm_target << "\n";
        std::cerr << "[WASM] Run with: wasmtime " << output_file << "\n";
        return true;
    }

    std::cerr << "Error: wasm-ld linking failed (exit code " << WEXITSTATUS(wstatus) << ")\n";
    return false;
}

/**
 * Link object file to executable
 */
bool link_executable(const std::string& object_file, const std::string& output_file, const CompilerOptions& opts) {
    // Get compiler executable directory for runtime library search
    std::string exe_dir;
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (len != -1) {
        exe_path[len] = '\0';
        std::string full_exe_path(exe_path);
        size_t last_slash = full_exe_path.find_last_of('/');
        if (last_slash != std::string::npos) {
            exe_dir = full_exe_path.substr(0, last_slash);
        }
    }
    
    // Try to find the runtime library in several locations
    std::vector<std::string> search_paths = {
        "build/libnitpick_runtime.a",        // Build directory (new name)
        "build/libaria_runtime.a",           // Build directory (compat name)
        "../build/libnitpick_runtime.a",
        "../build/libaria_runtime.a",
        "libnitpick_runtime.a",
        "libaria_runtime.a",
        "/usr/local/lib/libnitpick_runtime.a",
        "/usr/local/lib/libaria_runtime.a",
        "/usr/lib/libnitpick_runtime.a",
        "/usr/lib/libaria_runtime.a"
    };
    
    // Add path relative to compiler executable (if we could determine it)
    if (!exe_dir.empty()) {
        search_paths.insert(search_paths.begin(), exe_dir + "/libaria_runtime.a");
    }
    
    std::string runtime_lib;
    for (const auto& path : search_paths) {
        if (access(path.c_str(), F_OK) == 0) {
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
    // Use a vector of arguments instead of a shell command string to avoid
    // shell interpretation of special characters (e.g. $ORIGIN in -rpath).
    std::vector<std::string> link_args;
    link_args.push_back("clang++");

    // Preserve debug info through linking
    if (opts.debug_info) {
        link_args.push_back("-g");
        link_args.push_back("-O0");
    }

    // Add object file
    link_args.push_back(object_file);

    // Add runtime library if found
    if (!runtime_lib.empty()) {
        link_args.push_back(runtime_lib);
    }

    // Static linking: produce a fully static executable
    if (opts.build_static) {
        link_args.push_back("-static");
    }

    // Link libatomic for C++11 atomic operations (required by npk_runtime atomics)
    link_args.push_back("-latomic");

    // Add user link args in order (preserves -L/-l/-Wl, interleaving)
    for (const auto& arg : opts.link_args_ordered) {
        link_args.push_back(arg);
    }

    // Add output file
    link_args.push_back("-o");
    link_args.push_back(output_file);

    if (opts.verbose) {
        std::cout << "[LINK]";
        for (const auto& a : link_args) std::cout << " " << a;
        std::cout << "\n";
    }

    // Build argv for execvp (no shell — avoids expansion of $ORIGIN etc.)
    std::vector<char*> argv_vec;
    for (auto& s : link_args) argv_vec.push_back(const_cast<char*>(s.c_str()));
    argv_vec.push_back(nullptr);

    pid_t pid = fork();
    if (pid == -1) {
        std::cerr << "npkc: fork failed: " << strerror(errno) << "\n";
        return false;
    }
    if (pid == 0) {
        // Child: exec clang++ directly — no shell involved
        execvp("clang++", argv_vec.data());
        std::cerr << "npkc: execvp(clang++): " << strerror(errno) << "\n";
        _exit(127);
    }
    int wstatus = 0;
    if (waitpid(pid, &wstatus, 0) == -1) {
        std::cerr << "npkc: waitpid failed: " << strerror(errno) << "\n";
        return false;
    }
    return WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0;
}

/**
 * Main entry point
 */
int main(int argc, char** argv) {
    // Initialize GPU targets (NVPTX for CUDA) - Phase 1
    initialize_gpu_targets();
    
    // Initialize WASM targets (WebAssembly) - Phase 2
    initialize_wasm_targets();
    
    // Parse command-line arguments
    CompilerOptions opts;
    if (!parse_arguments(argc, argv, opts)) {
        return 1;
    }

    if (opts.verbose) {
        std::cout << "Nitpick Compiler (npkc) " << NPK_VERSION << "\n";
        std::cout << "Input files (" << opts.input_files.size() << "):" << "\n";
        for (const auto& file : opts.input_files) {
            std::cout << "  " << file << "\n";
        }
        std::cout << "Output: " << opts.output_file << "\n";
        if (opts.emit_ptx) {
            std::cout << "Target: GPU (NVIDIA CUDA/PTX)\n";
            std::cout << "GPU Architecture: " << opts.gpu_arch << "\n";
            std::cout << "GPU Optimization: O" << opts.gpu_opt_level << "\n";
        }
        if (opts.emit_wasm) {
            std::cout << "Target: WebAssembly (" << opts.wasm_target << ")\n";
        }
    }

    // Create diagnostic engine
    npk::DiagnosticEngine diags;

    // For --dump-tokens, --dump-ast, or --expand-macros, only process first file
    if (opts.dump_tokens || opts.dump_ast || opts.expand_macros) {
        std::string source;
        if (!read_source_file(opts.input_files[0], source, diags)) {
            diags.printAll();
            return 1;
        }
        npk::IRGenerator ir_gen(opts.input_files[0], opts.debug_info);
        llvm::Module* module = compile_to_module(source, opts.input_files[0], opts, ir_gen, diags);
        // compile_to_module returns nullptr for early exits (dump modes)
        if (diags.hasErrors()) {
            diags.printAll();
            return 1;
        }
        return module ? 1 : 0;
    }

    // ARIA-011: For --emit-deps, extract and output dependencies as JSON
    // Used by npk_make for accurate dependency scanning using the proper parser
    if (opts.emit_deps) {
        // Process first input file only (npk_make calls per-file)
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
    std::vector<std::unique_ptr<npk::IRGenerator>> ir_generators;
    std::vector<llvm::Module*> modules;
    // v0.8.2: Incremental compilation — owned contexts for cache-loaded modules
    std::vector<std::unique_ptr<llvm::LLVMContext>> cached_contexts;
    std::vector<std::unique_ptr<llvm::Module>> cached_modules;
    
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
        
        // v0.8.2: Try incremental cache before full compilation
        if (opts.incremental) {
            auto cache_ctx = std::make_unique<llvm::LLVMContext>();
            auto cached = load_cached_ir(opts.cache_dir, source, *cache_ctx, opts.verbose);
            if (cached) {
                llvm::Module* mod_ptr = cached.get();
                cached_contexts.push_back(std::move(cache_ctx));
                cached_modules.push_back(std::move(cached));
                modules.push_back(mod_ptr);
                continue;
            }
        }
        
        // Create IR generator (must stay alive)
        ir_generators.push_back(std::make_unique<npk::IRGenerator>(input_file, opts.debug_info));
        
        // Compile to LLVM module
        llvm::Module* module = compile_to_module(source, input_file, opts, *ir_generators.back(), diags);
        if (!module) {
            diags.printAll();
            return 1;
        }
        
        // v0.8.2: Save to cache for next build
        if (opts.incremental) {
            save_ir_to_cache(opts.cache_dir, source, module, opts.verbose);
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

    // Verify module (re-enabled in v0.4.6 — was previously commented out)
    std::string verify_error;
    llvm::raw_string_ostream verify_stream(verify_error);
    if (llvm::verifyModule(*final_module, &verify_stream)) {
        std::cerr << "Error: LLVM module verification failed:\n";
        std::cerr << verify_error << "\n";
        return 1;
    }

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
    } else if (opts.emit_ptx) {
        // GPU/PTX backend - emit PTX assembly for CUDA execution
        if (!emit_ptx(
            final_module,
            opts.output_file,
            opts.gpu_arch,
            opts.gpu_opt_level,
            opts.enable_gpu_debug
        )) {
            return 1;
        }
        if (opts.verbose) {
            std::cout << "PTX assembly written to: " << opts.output_file << "\n";
        }
    } else if (opts.emit_wasm) {
        // WASM backend - emit WebAssembly binary
        check_wasm_compatibility(final_module, opts.verbose);
        std::string wasm_obj = opts.output_file + ".o";
        if (!emit_wasm_object(final_module, wasm_obj, opts.wasm_target, opts.opt_level)) {
            return 1;
        }
        if (!link_wasm(wasm_obj, opts.output_file, opts)) {
            std::remove(wasm_obj.c_str());
            return 1;
        }
        std::remove(wasm_obj.c_str());
        if (opts.verbose) {
            std::cout << "WebAssembly binary written to: " << opts.output_file << "\n";
        }
    } else if (opts.emit_asm) {
        if (!emit_assembly(final_module, opts.output_file, opts.debug_info)) {
            return 1;
        }
        if (opts.verbose) {
            std::cout << "Assembly written to: " << opts.output_file << "\n";
        }
    } else if (opts.build_shared) {
        // --shared flag: Compile to shared library (.so)
        std::string obj_file = opts.output_file + ".o";
        if (!emit_object(final_module, obj_file)) {
            return 1;
        }

        // Build shared library link command
        std::vector<std::string> link_args;
        link_args.push_back("clang++");
        link_args.push_back("-shared");
        link_args.push_back("-fPIC");
        link_args.push_back(obj_file);

        // Add user link args in order (preserves -L/-l/-Wl, interleaving)
        for (const auto& arg : opts.link_args_ordered) {
            link_args.push_back(arg);
        }
        link_args.push_back("-o");
        link_args.push_back(opts.output_file);

        if (opts.verbose) {
            std::cout << "[LINK]";
            for (const auto& a : link_args) std::cout << " " << a;
            std::cout << "\n";
        }

        std::vector<char*> argv_vec;
        for (auto& s : link_args) argv_vec.push_back(const_cast<char*>(s.c_str()));
        argv_vec.push_back(nullptr);

        pid_t pid = fork();
        if (pid == -1) {
            std::cerr << "npkc: fork failed: " << strerror(errno) << "\n";
            return 1;
        }
        if (pid == 0) {
            execvp("clang++", argv_vec.data());
            std::cerr << "npkc: execvp(clang++): " << strerror(errno) << "\n";
            _exit(127);
        }
        int wstatus = 0;
        if (waitpid(pid, &wstatus, 0) == -1 || !WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
            std::cerr << "Error: Shared library linking failed\n";
            std::remove(obj_file.c_str());
            return 1;
        }
        std::remove(obj_file.c_str());
        if (opts.verbose) {
            std::cout << "Shared library written to: " << opts.output_file << "\n";
        }
    } else if (opts.build_library) {
        // -c flag: Compile to object file (no linking)
        if (!emit_object(final_module, opts.output_file)) {
            return 1;
        }
        if (opts.verbose) {
            std::cout << "Object file written to: " << opts.output_file << "\n";
        }
    } else {
        // Generate executable
        std::string asm_file = opts.output_file + ".s";
        if (!emit_assembly(final_module, asm_file, opts.debug_info)) {
            return 1;
        }
        
        // Auto-detect aria-libc libraries needed by scanning module for
        // undefined npk_libc_* symbols and adding corresponding -L/-l flags
        {
            // Map: symbol prefix → library name
            static const std::pair<std::string, std::string> lib_map[] = {
                {"npk_libc_mem_",     "npk_libc_mem"},
                {"npk_libc_io_",      "npk_libc_io"},
                {"npk_libc_math_",    "npk_libc_math"},
                {"npk_libc_string_",  "npk_libc_string"},
                {"npk_libc_time_",    "npk_libc_time"},
                {"npk_libc_fs_",      "npk_libc_fs"},
                {"npk_libc_net_",     "npk_libc_net"},
                {"npk_libc_process_", "npk_libc_process"},
                {"npk_libc_posix_",   "npk_libc_posix"},
                {"npk_libc_thread_",  "npk_libc_thread"},
                {"npk_libc_mutex_",   "npk_libc_mutex"},
                {"npk_libc_channel_", "npk_libc_channel"},
                {"npk_libc_hexstream_", "npk_libc_hexstream"},
                {"npk_libc_pool_",    "npk_libc_pool"},
                {"npk_libc_actor_",   "npk_libc_actor"},
                {"npk_libc_shm_",     "npk_libc_shm"},
                {"npk_libc_rwlock_",  "npk_libc_rwlock"},
                {"npk_libc_reply",    "npk_libc_actor"},
                {"npk_libc_get_reply_channel", "npk_libc_actor"},
                {"npk_libc_set_reply_channel", "npk_libc_actor"},
            };

            std::set<std::string> needed_libs;
            for (const auto& func : *final_module) {
                if (func.isDeclaration() && !func.isIntrinsic()) {
                    std::string name = func.getName().str();
                    for (const auto& [prefix, lib] : lib_map) {
                        if (name.compare(0, prefix.size(), prefix) == 0) {
                            needed_libs.insert(lib);
                            break;
                        }
                    }
                }
            }

            if (!needed_libs.empty()) {
                // Resolve compiler exe directory for relative shim path search
                std::string al_exe_dir;
                {
                    char al_buf[4096];
                    ssize_t al_len = readlink("/proc/self/exe", al_buf, sizeof(al_buf) - 1);
                    if (al_len > 0) {
                        al_buf[al_len] = '\0';
                        std::string al_full(al_buf);
                        size_t al_slash = al_full.find_last_of('/');
                        if (al_slash != std::string::npos)
                            al_exe_dir = al_full.substr(0, al_slash);
                    }
                }
                // Search relative to compiler exe, then fallback to known paths
                std::string shim_dir;
                std::vector<std::string> shim_search = {
                    al_exe_dir + "/../aria-libc/shim",
                    al_exe_dir + "/../../aria-libc/shim",
                    std::string(getenv("HOME") ? getenv("HOME") : "") + "/Workspace/REPOS/aria-libc/shim",
                    "/usr/local/lib",
                    "/usr/lib",
                };
                for (const auto& dir : shim_search) {
                    if (!dir.empty() && access(dir.c_str(), F_OK) == 0) {
                        shim_dir = dir;
                        break;
                    }
                }

                if (!shim_dir.empty()) {
                    opts.link_args_ordered.push_back("-L" + shim_dir);
                    for (const auto& lib : needed_libs) {
                        // Only add if the library file actually exists
                        std::string lib_path = shim_dir + "/lib" + lib + ".a";
                        std::string lib_so = shim_dir + "/lib" + lib + ".so";
                        if (access(lib_path.c_str(), F_OK) == 0 || access(lib_so.c_str(), F_OK) == 0) {
                            opts.link_args_ordered.push_back("-l" + lib);
                            if (opts.verbose) {
                                std::cerr << "[AUTO-LINK] " << lib << "\n";
                            }
                        }
                    }
                    // Add -lpthread if any threading-related libs were linked
                    static const std::set<std::string> pthread_libs = {
                        "npk_libc_thread", "npk_libc_mutex", "npk_libc_channel",
                        "npk_libc_pool", "npk_libc_actor", "npk_libc_rwlock"
                    };
                    for (const auto& lib : needed_libs) {
                        if (pthread_libs.count(lib)) {
                            opts.link_args_ordered.push_back("-lpthread");
                            break;
                        }
                    }
                }
            }
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
